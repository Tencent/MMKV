/*
* Tencent is pleased to support the open source community by making
* MMKV available.
*
* Copyright (C) 2020 THL A29 Limited, a Tencent company.
* All rights reserved.
*
* Licensed under the BSD 3-Clause License (the "License"); you may not use
* this file except in compliance with the License. You may obtain a copy of
* the License at
*
*       https://opensource.org/licenses/BSD-3-Clause
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "MMKV_IO.h"
#include "CodedInputData.h"
#include "CodedOutputData.h"
#include "InterProcessLock.h"
#include "MMBuffer.h"
#include "MMKVLog.h"
#include "MMKVMetaInfo.hpp"
#include "MemoryFile.h"
#include "MiniPBCoder.h"
#include "PBUtility.h"
#include "ScopedLock.hpp"
#include "ThreadLock.h"
#include "aes/AESCrypt.h"
#include "aes/openssl/openssl_aes.h"
#include "aes/openssl/openssl_md5.h"
#include "crc32/Checksum.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <ctime>

#ifdef MMKV_IOS
#    include "MMKV_OSX.h"
#endif

#ifdef MMKV_APPLE
#    if __has_feature(objc_arc)
#        error This file must be compiled with MRC. Use -fno-objc-arc flag.
#    endif
#endif // MMKV_APPLE

using namespace std;
using namespace mmkv;
using KVHolderRet_t = std::pair<bool, KeyValueHolder>;
extern ThreadLock *g_instanceLock;
extern unordered_map<string, MMKV *> *g_instanceDic;

MMKV_NAMESPACE_BEGIN

void MMKV::loadFromFile() {
    loadMetaInfoAndCheck();
#ifndef MMKV_DISABLE_CRYPT
    if (m_crypter) {
        if (m_metaInfo->m_version >= MMKVVersionRandomIV) {
            m_crypter->resetIV(m_metaInfo->m_vector, sizeof(m_metaInfo->m_vector));
        }
    }
#endif
    if (!m_file->isFileValid()) {
        m_file->reloadFromFile(m_expectedCapacity);
    }
    if (!m_file->isFileValid()) {
        MMKVError("file [%s] not valid", m_path.c_str());
    } else {
        // error checking
        bool loadFromFile = false, needFullWriteback = false;
        checkDataValid(loadFromFile, needFullWriteback);
        MMKVInfo("loading [%s] with %zu actual size, file size %zu, InterProcess %d, meta info "
                 "version:%u",
                 m_mmapID.c_str(), m_actualSize, m_file->getFileSize(), m_isInterProcess, m_metaInfo->m_version);
        auto ptr = (uint8_t *) m_file->getMemory();
        // loading
        if (loadFromFile && m_actualSize > 0) {
            MMKVInfo("loading [%s] with crc %u sequence %u version %u", m_mmapID.c_str(), m_metaInfo->m_crcDigest,
                     m_metaInfo->m_sequence, m_metaInfo->m_version);
            MMBuffer inputBuffer(ptr + Fixed32Size, m_actualSize, MMBufferNoCopy);
            if (m_crypter) {
                clearDictionary(m_dicCrypt);
            } else {
                clearDictionary(m_dic);
            }
            if (needFullWriteback) {
#ifndef MMKV_DISABLE_CRYPT
                if (m_crypter) {
                    MiniPBCoder::greedyDecodeMap(*m_dicCrypt, inputBuffer, m_crypter);
                } else
#endif
                {
                    MiniPBCoder::greedyDecodeMap(*m_dic, inputBuffer);
                }
            } else {
#ifndef MMKV_DISABLE_CRYPT
                if (m_crypter) {
                    MiniPBCoder::decodeMap(*m_dicCrypt, inputBuffer, m_crypter);
                } else
#endif
                {
                    MiniPBCoder::decodeMap(*m_dic, inputBuffer);
                }
            }
            m_output = new CodedOutputData(ptr + Fixed32Size, m_file->getFileSize() - Fixed32Size);
            m_output->seek(m_actualSize);
            if (needFullWriteback) {
                fullWriteback();
            }
        } else {
            // file not valid or empty, discard everything
            SCOPED_LOCK(m_exclusiveProcessLock);

            m_output = new CodedOutputData(ptr + Fixed32Size, m_file->getFileSize() - Fixed32Size);
            if (m_actualSize > 0) {
                writeActualSize(0, 0, nullptr, IncreaseSequence);
                sync(MMKV_SYNC);
            } else {
                writeActualSize(0, 0, nullptr, KeepSequence);
            }
        }
        auto count = m_crypter ? m_dicCrypt->size() : m_dic->size();
        MMKVInfo("loaded [%s] with %zu key-values", m_mmapID.c_str(), count);
    }

    m_needLoadFromFile = false;
}

// read from last m_position
void MMKV::partialLoadFromFile() {
    if (!m_file->isFileValid()) {
        return;
    }
    m_metaInfo->read(m_metaFile->getMemory());

    size_t oldActualSize = m_actualSize;
    m_actualSize = readActualSize();
    auto fileSize = m_file->getFileSize();
    MMKVDebug("loading [%s] with file size %zu, oldActualSize %zu, newActualSize %zu", m_mmapID.c_str(), fileSize,
              oldActualSize, m_actualSize);

    if (m_actualSize > 0) {
        if (m_actualSize < fileSize && m_actualSize + Fixed32Size <= fileSize) {
            if (m_actualSize > oldActualSize) {
                auto position = oldActualSize;
                size_t addedSize = m_actualSize - position;
                auto basePtr = (uint8_t *) m_file->getMemory() + Fixed32Size;
                // incremental update crc digest
                m_crcDigest = (uint32_t) CRC32(m_crcDigest, basePtr + position, addedSize);
                if (m_crcDigest == m_metaInfo->m_crcDigest) {
                    MMBuffer inputBuffer(basePtr, m_actualSize, MMBufferNoCopy);
#ifndef MMKV_DISABLE_CRYPT
                    if (m_crypter) {
                        MiniPBCoder::greedyDecodeMap(*m_dicCrypt, inputBuffer, m_crypter, position);
                    } else
#endif
                    {
                        MiniPBCoder::greedyDecodeMap(*m_dic, inputBuffer, position);
                    }
                    m_output->seek(addedSize);
                    m_hasFullWriteback = false;

                    [[maybe_unused]] auto count = m_crypter ? m_dicCrypt->size() : m_dic->size();
                    MMKVDebug("partial loaded [%s] with %zu values", m_mmapID.c_str(), count);
                    return;
                } else {
                    MMKVError("m_crcDigest[%u] != m_metaInfo->m_crcDigest[%u]", m_crcDigest, m_metaInfo->m_crcDigest);
                }
            }
        }
    }
    // something is wrong, do a full load
    clearMemoryCache();
    loadFromFile();
}

void MMKV::loadMetaInfoAndCheck() {
    if (!m_metaFile->isFileValid()) {
        m_metaFile->reloadFromFile();
    }
    if (!m_metaFile->isFileValid()) {
        MMKVError("file [%s] not valid", m_metaFile->getPath().c_str());
        return;
    }

    m_metaInfo->read(m_metaFile->getMemory());
    // the meta file is in specious status
    if (m_metaInfo->m_version >= MMKVVersionHolder) {
        MMKVWarning("meta file [%s] in specious state, version %u, flags 0x%llx", m_mmapID.c_str(),
                    m_metaInfo->m_version, m_metaInfo->m_flags);

        // MMKVVersionActualSize is the last version we don't check meta file
        m_metaInfo->m_version = MMKVVersionActualSize;
        m_metaInfo->m_flags = 0;
        m_metaInfo->write(m_metaFile->getMemory());
    }

    if (m_metaInfo->m_version >= MMKVVersionFlag) {
        m_enableKeyExpire = m_metaInfo->hasFlag(MMKVMetaInfo::EnableKeyExipre);
        if (m_enableKeyExpire && m_enableCompareBeforeSet) {
            MMKVError("enableCompareBeforeSet will be invalid when Expiration is on");
            m_enableCompareBeforeSet = false;
        }
        MMKVInfo("meta file [%s] has flag [%llu]", m_mmapID.c_str(), m_metaInfo->m_flags);
    } else {
        if (m_metaInfo->m_flags != 0) {
            m_metaInfo->m_flags = 0;
            m_metaInfo->write(m_metaFile->getMemory());
        }
    }
}

void MMKV::checkDataValid(bool &loadFromFile, bool &needFullWriteback) {
    // try auto recover from last confirmed location
    auto fileSize = m_file->getFileSize();
    auto checkLastConfirmedInfo = [&] {
        if (m_metaInfo->m_version >= MMKVVersionActualSize) {
            // downgrade & upgrade support
            uint32_t oldStyleActualSize = 0;
            memcpy(&oldStyleActualSize, m_file->getMemory(), Fixed32Size);
            if (oldStyleActualSize != m_actualSize) {
                MMKVWarning("oldStyleActualSize %u not equal to meta actual size %lu", oldStyleActualSize,
                            m_actualSize);
                if (oldStyleActualSize < fileSize && (oldStyleActualSize + Fixed32Size) <= fileSize) {
                    if (checkFileCRCValid(oldStyleActualSize, m_metaInfo->m_crcDigest)) {
                        MMKVInfo("looks like [%s] been downgrade & upgrade again", m_mmapID.c_str());
                        loadFromFile = true;
                        writeActualSize(oldStyleActualSize, m_metaInfo->m_crcDigest, nullptr, KeepSequence);
                        return;
                    }
                } else {
                    MMKVWarning("oldStyleActualSize %u greater than file size %lu", oldStyleActualSize, fileSize);
                }
            }

            auto lastActualSize = m_metaInfo->m_lastConfirmedMetaInfo.lastActualSize;
            if (lastActualSize < fileSize && (lastActualSize + Fixed32Size) <= fileSize) {
                auto lastCRCDigest = m_metaInfo->m_lastConfirmedMetaInfo.lastCRCDigest;
                if (checkFileCRCValid(lastActualSize, lastCRCDigest)) {
                    loadFromFile = true;
                    writeActualSize(lastActualSize, lastCRCDigest, nullptr, KeepSequence);
                } else {
                    MMKVError("check [%s] error: lastActualSize %u, lastActualCRC %u", m_mmapID.c_str(), lastActualSize,
                              lastCRCDigest);
                }
            } else {
                MMKVError("check [%s] error: lastActualSize %u, file size is %u", m_mmapID.c_str(), lastActualSize,
                          fileSize);
            }
        }
    };

    m_actualSize = readActualSize();

    if (m_actualSize < fileSize && (m_actualSize + Fixed32Size) <= fileSize) {
        if (checkFileCRCValid(m_actualSize, m_metaInfo->m_crcDigest)) {
            loadFromFile = true;
        } else {
            checkLastConfirmedInfo();
            if (!loadFromFile) {

                auto strategic = onMMKVCRCCheckFail(m_mmapID);
                if (strategic == OnErrorRecover) {
                    loadFromFile = true;
                    needFullWriteback = true;
                }
                MMKVInfo("recover strategic for [%s] is %d", m_mmapID.c_str(), strategic);
            }
        }
    } else {
        MMKVError("check [%s] error: %zu size in total, file size is %zu", m_mmapID.c_str(), m_actualSize, fileSize);

        checkLastConfirmedInfo();

        if (!loadFromFile) {
            auto strategic = onMMKVFileLengthError(m_mmapID);
            if (strategic == OnErrorRecover) {
                // make sure we don't over read the file
                m_actualSize = fileSize - Fixed32Size;
                loadFromFile = true;
                needFullWriteback = true;
            }
            MMKVInfo("recover strategic for [%s] is %d", m_mmapID.c_str(), strategic);
        }
    }
}

void MMKV::checkLoadData() {
    if (m_needLoadFromFile) {
        SCOPED_LOCK(m_sharedProcessLock);

        m_needLoadFromFile = false;
        loadFromFile();
        return;
    }
    if (!m_isInterProcess) {
        return;
    }

    if (!m_metaFile->isFileValid()) {
        return;
    }
    SCOPED_LOCK(m_sharedProcessLock);

    MMKVMetaInfo metaInfo;
    metaInfo.read(m_metaFile->getMemory());
    if (m_metaInfo->m_sequence != metaInfo.m_sequence) {
        MMKVInfo("[%s] oldSeq %u, newSeq %u", m_mmapID.c_str(), m_metaInfo->m_sequence, metaInfo.m_sequence);
        SCOPED_LOCK(m_sharedProcessLock);

        clearMemoryCache();
        loadFromFile();
        notifyContentChanged();
    } else if (m_metaInfo->m_crcDigest != metaInfo.m_crcDigest) {
        MMKVDebug("[%s] oldCrc %u, newCrc %u, new actualSize %u", m_mmapID.c_str(), m_metaInfo->m_crcDigest,
                  metaInfo.m_crcDigest, metaInfo.m_actualSize);
        SCOPED_LOCK(m_sharedProcessLock);

        size_t fileSize = m_file->getActualFileSize();
        if (m_file->getFileSize() != fileSize) {
            MMKVInfo("file size has changed [%s] from %zu to %zu", m_mmapID.c_str(), m_file->getFileSize(), fileSize);
            clearMemoryCache();
            loadFromFile();
        } else {
            partialLoadFromFile();
        }
        notifyContentChanged();
    }
}

constexpr uint32_t ItemSizeHolder = 0x00ffffff;
constexpr uint32_t ItemSizeHolderSize = 4;

static pair<MMBuffer, size_t> prepareEncode(const MMKVMap &dic) {
    // make some room for placeholder
    size_t totalSize = ItemSizeHolderSize;
    for (auto &itr : dic) {
        auto &kvHolder = itr.second;
        totalSize += kvHolder.computedKVSize + kvHolder.valueSize;
    }
    return make_pair(MMBuffer(), totalSize);
}

#ifndef MMKV_DISABLE_CRYPT
static pair<MMBuffer, size_t> prepareEncode(const MMKVMapCrypt &dic) {
    MMKVVector vec;
    size_t totalSize = 0;
    // make some room for placeholder
    uint32_t smallestOffet = 5 + 1; // 5 is the largest size needed to encode varint32
    for (auto &itr : dic) {
        auto &kvHolder = itr.second;
        if (kvHolder.type == KeyValueHolderType_Offset) {
            totalSize += kvHolder.pbKeyValueSize + kvHolder.keySize + kvHolder.valueSize;
            smallestOffet = min(smallestOffet, kvHolder.offset);
        } else {
            vec.emplace_back(itr.first, kvHolder.toMMBuffer(nullptr, nullptr));
        }
    }
    if (smallestOffet > 5) {
        smallestOffet = ItemSizeHolderSize;
    }
    totalSize += smallestOffet;
    if (vec.empty()) {
        return make_pair(MMBuffer(), totalSize);
    }
    auto buffer = MiniPBCoder::encodeDataWithObject(vec);
    // skip the pb size of buffer
    auto sizeOfMap = CodedInputData(buffer.getPtr(), buffer.length()).readUInt32();
    totalSize += sizeOfMap;
    return make_pair(std::move(buffer), totalSize);
}
#endif

static pair<MMBuffer, size_t> prepareEncode(MMKVVector &&vec) {
    // make some room for placeholder
    size_t totalSize = ItemSizeHolderSize;
    auto buffer = MiniPBCoder::encodeDataWithObject(vec);
    // skip the pb size of buffer
    auto sizeOfMap = CodedInputData(buffer.getPtr(), buffer.length()).readUInt32();
    totalSize += sizeOfMap;
    return make_pair(std::move(buffer), totalSize);
}

// since we use append mode, when -[setData: forKey:] many times, space may not be enough
// try a full rewrite to make space
bool MMKV::ensureMemorySize(size_t newSize) {
    if (!isFileValid()) {
        MMKVWarning("[%s] file not valid", m_mmapID.c_str());
        return false;
    }

    if (newSize >= m_output->spaceLeft() || (m_crypter ? m_dicCrypt->empty() : m_dic->empty())) {
        // remove expired keys
        if (m_enableKeyExpire) {
            filterExpiredKeys();
        }
        // try a full rewrite to make space
        auto preparedData = m_crypter ? prepareEncode(*m_dicCrypt) : prepareEncode(*m_dic);
        // dic.empty() means inserting key-value for the first time, no need to call msync()
        return expandAndWriteBack(newSize, std::move(preparedData), m_crypter ? !m_dicCrypt->empty() : !m_dic->empty());
    }
    return true;
}

// try a full rewrite to make space
bool MMKV::expandAndWriteBack(size_t newSize, std::pair<mmkv::MMBuffer, size_t> preparedData, bool needSync) {
    auto fileSize = m_file->getFileSize();
    auto sizeOfDic = preparedData.second;
    size_t lenNeeded = sizeOfDic + Fixed32Size + newSize;
    size_t nowDicCount = m_crypter ? m_dicCrypt->size() : m_dic->size();
    size_t laterDicCount = std::max<size_t>(1, nowDicCount + 1);
    // or use <cmath> ceil()
    size_t avgItemSize = (lenNeeded + laterDicCount - 1) / laterDicCount;
    size_t futureUsage = avgItemSize * std::max<size_t>(8, laterDicCount / 2);
    // 1. no space for a full rewrite, double it
    // 2. or space is not large enough for future usage, double it to avoid frequently full rewrite
    if (lenNeeded >= fileSize || (needSync && (lenNeeded + futureUsage) >= fileSize)) {
        size_t oldSize = fileSize;
        do {
            fileSize *= 2;
        } while (lenNeeded + futureUsage >= fileSize);
        MMKVInfo("extending [%s] file size from %zu to %zu, incoming size:%zu, future usage:%zu", m_mmapID.c_str(),
                 oldSize, fileSize, newSize, futureUsage);

        // if we can't extend size, rollback to old state
        // this is a good place to mock enlarging file failure
        if (!m_file->truncate(fileSize)) {
            return false;
        }

        // check if we fail to make more space
        if (!isFileValid()) {
            MMKVWarning("[%s] file not valid", m_mmapID.c_str());
            return false;
        }
    }
    return doFullWriteBack(std::move(preparedData), nullptr, needSync);
}

size_t MMKV::readActualSize() {
    MMKV_ASSERT(m_file->getMemory());
    MMKV_ASSERT(m_metaFile->isFileValid());

    uint32_t actualSize = 0;
    memcpy(&actualSize, m_file->getMemory(), Fixed32Size);

    if (m_metaInfo->m_version >= MMKVVersionActualSize) {
        if (m_metaInfo->m_actualSize != actualSize) {
            MMKVWarning("[%s] actual size %u, meta actual size %u", m_mmapID.c_str(), actualSize,
                        m_metaInfo->m_actualSize);
        }
        return m_metaInfo->m_actualSize;
    } else {
        return actualSize;
    }
}

void MMKV::oldStyleWriteActualSize(size_t actualSize) {
    MMKV_ASSERT(m_file->getMemory());

    m_actualSize = actualSize;
#ifdef MMKV_IOS
    auto ret = guardForBackgroundWriting(m_file->getMemory(), Fixed32Size);
    if (!ret.first) {
        return;
    }
#endif
    memcpy(m_file->getMemory(), &actualSize, Fixed32Size);
}

bool MMKV::writeActualSize(size_t size, uint32_t crcDigest, const void *iv, bool increaseSequence) {
    // backward compatibility
    oldStyleWriteActualSize(size);

    if (!m_metaFile->isFileValid()) {
        return false;
    }

    bool needsFullWrite = false;
    m_actualSize = size;
    m_metaInfo->m_actualSize = static_cast<uint32_t>(size);
    m_crcDigest = crcDigest;
    m_metaInfo->m_crcDigest = crcDigest;
    if (m_metaInfo->m_version < MMKVVersionSequence) {
        m_metaInfo->m_version = MMKVVersionSequence;
        needsFullWrite = true;
    }
#ifndef MMKV_DISABLE_CRYPT
    if (unlikely(iv)) {
        memcpy(m_metaInfo->m_vector, iv, sizeof(m_metaInfo->m_vector));
        if (m_metaInfo->m_version < MMKVVersionRandomIV) {
            m_metaInfo->m_version = MMKVVersionRandomIV;
        }
        needsFullWrite = true;
    }
#endif
    if (unlikely(increaseSequence)) {
        m_metaInfo->m_sequence++;
        m_metaInfo->m_lastConfirmedMetaInfo.lastActualSize = static_cast<uint32_t>(size);
        m_metaInfo->m_lastConfirmedMetaInfo.lastCRCDigest = crcDigest;
        if (m_metaInfo->m_version < MMKVVersionActualSize) {
            m_metaInfo->m_version = MMKVVersionActualSize;
        }
        needsFullWrite = true;
        MMKVInfo("[%s] increase sequence to %u, crc %u, actualSize %u", m_mmapID.c_str(), m_metaInfo->m_sequence,
                 m_metaInfo->m_crcDigest, m_metaInfo->m_actualSize);
    }
    if (m_metaInfo->m_version < MMKVVersionFlag) {
        m_metaInfo->m_flags = 0;
        m_metaInfo->m_version = MMKVVersionFlag;
        needsFullWrite = true;
    }
#ifdef MMKV_IOS
    auto ret = guardForBackgroundWriting(m_metaFile->getMemory(), sizeof(MMKVMetaInfo));
    if (!ret.first) {
        return false;
    }
#endif
    if (unlikely(needsFullWrite)) {
        m_metaInfo->write(m_metaFile->getMemory());
    } else {
        m_metaInfo->writeCRCAndActualSizeOnly(m_metaFile->getMemory());
    }
    return true;
}

MMBuffer MMKV::getRawDataForKey(MMKVKey_t key) {
    checkLoadData();
#ifndef MMKV_DISABLE_CRYPT
    if (m_crypter) {
        auto itr = m_dicCrypt->find(key);
        if (itr != m_dicCrypt->end()) {
            auto basePtr = (uint8_t *) (m_file->getMemory()) + Fixed32Size;
            return itr->second.toMMBuffer(basePtr, m_crypter);
        }
    } else
#endif
    {
        auto itr = m_dic->find(key);
        if (itr != m_dic->end()) {
            auto basePtr = (uint8_t *) (m_file->getMemory()) + Fixed32Size;
            return itr->second.toMMBuffer(basePtr);
        }
    }
    MMBuffer nan;
    return nan;
}

mmkv::MMBuffer MMKV::getDataForKey(MMKVKey_t key) {
    if (unlikely(m_enableKeyExpire)) {
        return getDataWithoutMTimeForKey(key);
    }
    return getRawDataForKey(key);
}

#ifndef MMKV_DISABLE_CRYPT
// for Apple watch simulator
#    if defined(TARGET_OS_SIMULATOR) && defined(TARGET_CPU_X86)
static AESCryptStatus t_status;
#    else
thread_local AESCryptStatus t_status;
#    endif
#endif // MMKV_DISABLE_CRYPT

bool MMKV::setDataForKey(MMBuffer &&data, MMKVKey_t key, bool isDataHolder) {
    if ((!isDataHolder && data.length() == 0) || isKeyEmpty(key)) {
        return false;
    }
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_exclusiveProcessLock);
    checkLoadData();

#ifndef MMKV_DISABLE_CRYPT
    if (m_crypter) {
        if (isDataHolder) {
            auto sizeNeededForData = pbRawVarint32Size((uint32_t) data.length()) + data.length();
            if (!KeyValueHolderCrypt::isValueStoredAsOffset(sizeNeededForData)) {
                data = MiniPBCoder::encodeDataWithObject(data);
                isDataHolder = false;
            }
        }
        auto itr = m_dicCrypt->find(key);
        if (itr != m_dicCrypt->end()) {
            bool onlyOneKey = !m_isInterProcess && m_dicCrypt->size() == 1;
#    ifdef MMKV_APPLE
            KVHolderRet_t ret;
            if (onlyOneKey) {
                ret = overrideDataWithKey(data, key, itr->second, isDataHolder);
            } else {
                ret = appendDataWithKey(data, key, itr->second, isDataHolder);
            }
#    else
            KVHolderRet_t ret;
            if (onlyOneKey) {
                ret = overrideDataWithKey(data, key, isDataHolder);
            } else {
                ret = appendDataWithKey(data, key, isDataHolder);
            }
#    endif
            if (!ret.first) {
                return false;
            }
            KeyValueHolderCrypt kvHolder;
            if (KeyValueHolderCrypt::isValueStoredAsOffset(ret.second.valueSize)) {
                kvHolder = KeyValueHolderCrypt(ret.second.keySize, ret.second.valueSize, ret.second.offset);
                memcpy(&kvHolder.cryptStatus, &t_status, sizeof(t_status));
            } else {
                kvHolder = KeyValueHolderCrypt(std::move(data));
            }
            if (likely(!m_enableKeyExpire)) {
                itr->second = std::move(kvHolder);
            } else {
                itr = m_dicCrypt->find(key);
                if (itr != m_dicCrypt->end()) {
                    itr->second = std::move(kvHolder);
                } else {
                    // in case filterExpiredKeys() is triggered
                    m_dicCrypt->emplace(key, std::move(kvHolder));
                    mmkv_retain_key(key);
                }
            }
        } else {
            bool needOverride = !m_isInterProcess && m_dicCrypt->empty() && m_actualSize > 0;
            KVHolderRet_t ret;
            if (needOverride) {
                ret = overrideDataWithKey(data, key, isDataHolder);
            } else {
                ret = appendDataWithKey(data, key, isDataHolder);
            }
            if (!ret.first) {
                return false;
            }
            if (KeyValueHolderCrypt::isValueStoredAsOffset(ret.second.valueSize)) {
                auto r = m_dicCrypt->emplace(
                    key, KeyValueHolderCrypt(ret.second.keySize, ret.second.valueSize, ret.second.offset));
                if (r.second) {
                    memcpy(&(r.first->second.cryptStatus), &t_status, sizeof(t_status));
                }
            } else {
                m_dicCrypt->emplace(key, KeyValueHolderCrypt(std::move(data)));
            }
            mmkv_retain_key(key);
        }
    } else
#endif // MMKV_DISABLE_CRYPT
    {
        auto itr = m_dic->find(key);
        if (itr != m_dic->end()) {
            // compare data before appending to file
            if (isCompareBeforeSetEnabled()) {
                auto basePtr = (uint8_t *) (m_file->getMemory()) + Fixed32Size;
                MMBuffer oldValueData = itr->second.toMMBuffer(basePtr);
                if (isDataHolder) {
                    CodedInputData inputData(oldValueData.getPtr(), oldValueData.length());
                    try {
                        // read extra holder header bytes and to real MMBuffer
                        oldValueData = CodedInputData::readRealData(oldValueData);
                        if (oldValueData == data) {
                            // MMKVInfo("[key] %s, set the same data", key.c_str());
                            return true;
                        }
                    } catch (std::exception &exception) {
                        MMKVError("compareBeforeSet exception: %s", exception.what());
                    } catch (...) {
                        MMKVError("compareBeforeSet fail");
                    }
                } else {
                    if (oldValueData == data) {
                        //  MMKVInfo("[key] %s, set the same data", key.c_str());
                        return true;
                    }
                }
            }

            bool onlyOneKey = !m_isInterProcess && m_dic->size() == 1;
            if (likely(!m_enableKeyExpire)) {
                KVHolderRet_t ret;
                if (onlyOneKey) {
                    ret = overrideDataWithKey(data, itr->second, isDataHolder);
                } else {
                    ret = appendDataWithKey(data, itr->second, isDataHolder);
                }
                if (!ret.first) {
                    return false;
                }
                itr->second = std::move(ret.second);
            } else {
                KVHolderRet_t ret;
                if (onlyOneKey) {
                    ret = overrideDataWithKey(data, key, isDataHolder);
                } else {
                    ret = appendDataWithKey(data, key, isDataHolder);
                }
                if (!ret.first) {
                    return false;
                }
                itr = m_dic->find(key);
                if (itr != m_dic->end()) {
                    itr->second = std::move(ret.second);
                } else {
                    // in case filterExpiredKeys() is triggered
                    m_dic->emplace(key, std::move(ret.second));
                    mmkv_retain_key(key);
                }
            }
        } else {
            bool needOverride = !m_isInterProcess && m_dic->empty() && m_actualSize > 0;
            KVHolderRet_t ret;
            if (needOverride) {
                ret = overrideDataWithKey(data, key, isDataHolder);
            } else {
                ret = appendDataWithKey(data, key, isDataHolder);
            }
            if (!ret.first) {
                return false;
            }
            m_dic->emplace(key, std::move(ret.second));
            mmkv_retain_key(key);
        }
    }
    m_hasFullWriteback = false;
    return true;
}

bool MMKV::removeDataForKey(MMKVKey_t key) {
    if (isKeyEmpty(key)) {
        return false;
    }
#ifndef MMKV_DISABLE_CRYPT
    if (m_crypter) {
        auto itr = m_dicCrypt->find(key);
        if (itr != m_dicCrypt->end()) {
            m_hasFullWriteback = false;
            static MMBuffer nan;
#    ifdef MMKV_APPLE
            auto ret = appendDataWithKey(nan, key, itr->second);
            if (ret.first) {
                if (unlikely(m_enableKeyExpire)) {
                    // filterExpiredKeys() may invalid itr
                    itr = m_dicCrypt->find(key);
                    if (itr == m_dicCrypt->end()) {
                        return true;
                    }
                }
                auto oldKey = itr->first;
                m_dicCrypt->erase(itr);
                [oldKey release];
            }
#    else
            auto ret = appendDataWithKey(nan, key);
            if (ret.first) {
                if (unlikely(m_enableKeyExpire)) {
                    m_dicCrypt->erase(key);
                } else {
                    m_dicCrypt->erase(itr);
                }
            }
#    endif
            return ret.first;
        }
    } else
#endif // MMKV_DISABLE_CRYPT
    {
        auto itr = m_dic->find(key);
        if (itr != m_dic->end()) {
            m_hasFullWriteback = false;
            static MMBuffer nan;
            auto ret = likely(!m_enableKeyExpire) ? appendDataWithKey(nan, itr->second) : appendDataWithKey(nan, key);
            if (ret.first) {
#ifdef MMKV_APPLE
                if (unlikely(m_enableKeyExpire)) {
                    // filterExpiredKeys() may invalid itr
                    itr = m_dic->find(key);
                    if (itr == m_dic->end()) {
                        return true;
                    }
                }
                auto oldKey = itr->first;
                m_dic->erase(itr);
                [oldKey release];
#else
                if (unlikely(m_enableKeyExpire)) {
                    // filterExpiredKeys() may invalid itr
                    m_dic->erase(key);
                } else {
                    m_dic->erase(itr);
                }
#endif
            }
            return ret.first;
        }
    }

    return false;
}

KVHolderRet_t
MMKV::doAppendDataWithKey(const MMBuffer &data, const MMBuffer &keyData, bool isDataHolder, uint32_t originKeyLength) {
    auto isKeyEncoded = (originKeyLength < keyData.length());
    auto keyLength = static_cast<uint32_t>(keyData.length());
    auto valueLength = static_cast<uint32_t>(data.length());
    if (isDataHolder) {
        valueLength += pbRawVarint32Size(valueLength);
    }
    // size needed to encode the key
    size_t size = isKeyEncoded ? keyLength : (keyLength + pbRawVarint32Size(keyLength));
    // size needed to encode the value
    size += valueLength + pbRawVarint32Size(valueLength);

    SCOPED_LOCK(m_exclusiveProcessLock);

    bool hasEnoughSize = ensureMemorySize(size);
    if (!hasEnoughSize || !isFileValid()) {
        return make_pair(false, KeyValueHolder());
    }

#ifdef MMKV_IOS
    auto ret = guardForBackgroundWriting(m_output->curWritePointer(), size);
    if (!ret.first) {
        return make_pair(false, KeyValueHolder());
    }
#endif
#ifndef MMKV_DISABLE_CRYPT
    if (m_crypter) {
        if (KeyValueHolderCrypt::isValueStoredAsOffset(valueLength)) {
            m_crypter->getCurStatus(t_status);
        }
    }
#endif
    try {
        if (isKeyEncoded) {
            m_output->writeRawData(keyData);
        } else {
            m_output->writeData(keyData);
        }
        if (isDataHolder) {
            m_output->writeRawVarint32((int32_t) valueLength);
        }
        m_output->writeData(data); // note: write size of data
    } catch (std::exception &e) {
        MMKVError("%s", e.what());
        return make_pair(false, KeyValueHolder());
    } catch (...) {
        MMKVError("append fail");
        return make_pair(false, KeyValueHolder());
    }

    auto offset = static_cast<uint32_t>(m_actualSize);
    auto ptr = (uint8_t *) m_file->getMemory() + Fixed32Size + m_actualSize;
#ifndef MMKV_DISABLE_CRYPT
    if (m_crypter) {
        m_crypter->encrypt(ptr, ptr, size);
    }
#endif
    m_actualSize += size;
    updateCRCDigest(ptr, size);

    return make_pair(true, KeyValueHolder(originKeyLength, valueLength, offset));
}

KVHolderRet_t MMKV::doOverrideDataWithKey(const MMBuffer &data,
                                          const MMBuffer &keyData,
                                          bool isDataHolder,
                                          uint32_t originKeyLength) {
    auto isKeyEncoded = (originKeyLength < keyData.length());
    auto keyLength = static_cast<uint32_t>(keyData.length());
    auto valueLength = static_cast<uint32_t>(data.length());
    if (isDataHolder) {
        valueLength += pbRawVarint32Size(valueLength);
    }
    // size needed to encode the key
    size_t size = isKeyEncoded ? keyLength : (keyLength + pbRawVarint32Size(keyLength));
    // size needed to encode the value
    size += valueLength + pbRawVarint32Size(valueLength);

    if (!checkSizeForOverride(size)) {
        return doAppendDataWithKey(data, keyData, isDataHolder, originKeyLength);
    }

    // we don't not support override in multi-process mode
    // SCOPED_LOCK(m_exclusiveProcessLock);

#ifdef MMKV_IOS
    auto ret = guardForBackgroundWriting(m_output->curWritePointer(), size);
    if (!ret.first) {
        return make_pair(false, KeyValueHolder());
    }
#endif
#ifndef MMKV_DISABLE_CRYPT
    if (m_crypter) {
        if (m_metaInfo->m_version >= MMKVVersionRandomIV) {
            m_crypter->resetIV(m_metaInfo->m_vector, sizeof(m_metaInfo->m_vector));
        } else {
            m_crypter->resetIV();
        }
        if (KeyValueHolderCrypt::isValueStoredAsOffset(valueLength)) {
            m_crypter->getCurStatus(t_status);
        }
    }
#endif
    try {
        // write ItemSizeHolder
        m_output->setPosition(0);
        m_output->writeRawVarint32(ItemSizeHolder);
        m_actualSize = ItemSizeHolderSize;

        if (isKeyEncoded) {
            m_output->writeRawData(keyData);
        } else {
            m_output->writeData(keyData);
        }
        if (isDataHolder) {
            m_output->writeRawVarint32((int32_t) valueLength);
        }
        m_output->writeData(data); // note: write size of data
    } catch (std::exception &e) {
        MMKVError("%s", e.what());
        return make_pair(false, KeyValueHolder());
    } catch (...) {
        MMKVError("append fail");
        return make_pair(false, KeyValueHolder());
    }

    auto offset = static_cast<uint32_t>(m_actualSize);
    m_actualSize += size;
    auto ptr = (uint8_t *) m_file->getMemory() + Fixed32Size;
#ifndef MMKV_DISABLE_CRYPT
    if (m_crypter) {
        m_crypter->encrypt(ptr, ptr, m_actualSize);
    }
#endif
    recaculateCRCDigestOnly();

    return make_pair(true, KeyValueHolder(originKeyLength, valueLength, offset));
}

bool MMKV::checkSizeForOverride(size_t size) {
    if (!isFileValid()) {
        MMKVWarning("[%s] file not valid", m_mmapID.c_str());
        return false;
    }

    // only override if the file can hole it without ftruncate()
    auto fileSize = m_file->getFileSize();
    auto spaceNeededForOverride = size + Fixed32Size + ItemSizeHolderSize;
    if (size > fileSize || spaceNeededForOverride > fileSize) {
        return false;
    }
    return true;
}

KVHolderRet_t MMKV::appendDataWithKey(const MMBuffer &data, MMKVKey_t key, bool isDataHolder) {
#ifdef MMKV_APPLE
    auto oData = [key dataUsingEncoding:NSUTF8StringEncoding];
    auto keyData = MMBuffer(oData, MMBufferNoCopy);
#else
    auto keyData = MMBuffer((void *) key.data(), key.size(), MMBufferNoCopy);
#endif
    return doAppendDataWithKey(data, keyData, isDataHolder, static_cast<uint32_t>(keyData.length()));
}

KVHolderRet_t MMKV::overrideDataWithKey(const MMBuffer &data, MMKVKey_t key, bool isDataHolder) {
#ifdef MMKV_APPLE
    auto oData = [key dataUsingEncoding:NSUTF8StringEncoding];
    auto keyData = MMBuffer(oData, MMBufferNoCopy);
#else
    auto keyData = MMBuffer((void *) key.data(), key.size(), MMBufferNoCopy);
#endif
    return doOverrideDataWithKey(data, keyData, isDataHolder, static_cast<uint32_t>(keyData.length()));
}

KVHolderRet_t MMKV::appendDataWithKey(const MMBuffer &data, const KeyValueHolder &kvHolder, bool isDataHolder) {
    SCOPED_LOCK(m_exclusiveProcessLock);

    uint32_t keyLength = kvHolder.keySize;
    // size needed to encode the key
    size_t rawKeySize = keyLength + pbRawVarint32Size(keyLength);

    // ensureMemorySize() might change kvHolder.offset, so have to do it early
    {
        auto valueLength = static_cast<uint32_t>(data.length());
        if (isDataHolder) {
            valueLength += pbRawVarint32Size(valueLength);
        }
        auto size = rawKeySize + valueLength + pbRawVarint32Size(valueLength);
        bool hasEnoughSize = ensureMemorySize(size);
        if (!hasEnoughSize) {
            return make_pair(false, KeyValueHolder());
        }
    }
    auto basePtr = (uint8_t *) m_file->getMemory() + Fixed32Size;
    MMBuffer keyData(basePtr + kvHolder.offset, rawKeySize, MMBufferNoCopy);

    return doAppendDataWithKey(data, keyData, isDataHolder, keyLength);
}

// only one key in dict, do not append, just rewrite from beginning
KVHolderRet_t MMKV::overrideDataWithKey(const MMBuffer &data, const KeyValueHolder &kvHolder, bool isDataHolder) {
    // we don't not support override in multi-process mode
    // SCOPED_LOCK(m_exclusiveProcessLock);

    uint32_t keyLength = kvHolder.keySize;
    // size needed to encode the key
    size_t rawKeySize = keyLength + pbRawVarint32Size(keyLength);

    // ensureMemorySize() (inside doAppendDataWithKey() which be called from doOverrideDataWithKey())
    // might change kvHolder.offset, so have to do it early
    {
        auto valueLength = static_cast<uint32_t>(data.length());
        if (isDataHolder) {
            valueLength += pbRawVarint32Size(valueLength);
        }
        auto size = rawKeySize + valueLength + pbRawVarint32Size(valueLength);
        bool hasEnoughSize = checkSizeForOverride(size);
        if (!hasEnoughSize) {
            return appendDataWithKey(data, kvHolder, isDataHolder);
        }
    }
    auto basePtr = (uint8_t *) m_file->getMemory() + Fixed32Size;
    MMBuffer keyData(basePtr + kvHolder.offset, rawKeySize, MMBufferNoCopy);

    return doOverrideDataWithKey(data, keyData, isDataHolder, keyLength);
}

bool MMKV::fullWriteback(AESCrypt *newCrypter, bool onlyWhileExpire) {
    if (m_hasFullWriteback) {
        return true;
    }
    if (m_needLoadFromFile) {
        return true;
    }
    if (!isFileValid()) {
        MMKVWarning("[%s] file not valid", m_mmapID.c_str());
        return false;
    }

    if (unlikely(m_enableKeyExpire)) {
        auto expiredCount = filterExpiredKeys();
        if (onlyWhileExpire && expiredCount == 0) {
            return true;
        }
    }

    auto isEmpty = m_crypter ? m_dicCrypt->empty() : m_dic->empty();
    if (isEmpty) {
        clearAll();
        return true;
    }

    SCOPED_LOCK(m_exclusiveProcessLock);
    auto preparedData = m_crypter ? prepareEncode(*m_dicCrypt) : prepareEncode(*m_dic);
    auto sizeOfDic = preparedData.second;
    if (sizeOfDic > 0) {
        auto fileSize = m_file->getFileSize();
        if (sizeOfDic + Fixed32Size <= fileSize) {
            return doFullWriteBack(std::move(preparedData), newCrypter);
        } else {
            assert(0);
            assert(newCrypter == nullptr);
            // expandAndWriteBack() will extend file & full rewrite, no need to write back again
            auto newSize = sizeOfDic + Fixed32Size - fileSize;
            return expandAndWriteBack(newSize, std::move(preparedData));
        }
    }
    return false;
}

// we don't need to really serialize the dictionary, just reuse what's already in the file
static void
memmoveDictionary(MMKVMap &dic, CodedOutputData *output, uint8_t *ptr, AESCrypt *encrypter, size_t totalSize) {
    auto originOutputPtr = output->curWritePointer();
    // make space to hold the fake size of dictionary's serialization result
    auto writePtr = originOutputPtr + ItemSizeHolderSize;
    // reuse what's already in the file
    if (!dic.empty()) {
        // sort by offset
        vector<KeyValueHolder *> vec;
        vec.reserve(dic.size());
        for (auto &itr : dic) {
            vec.push_back(&itr.second);
        }
        sort(vec.begin(), vec.end(), [](const auto &left, const auto &right) { return left->offset < right->offset; });

        // merge nearby items to make memmove quicker
        vector<pair<uint32_t, uint32_t>> dataSections; // pair(offset, size)
        dataSections.emplace_back(vec.front()->offset, vec.front()->computedKVSize + vec.front()->valueSize);
        for (size_t index = 1, total = vec.size(); index < total; index++) {
            auto kvHolder = vec[index];
            auto &lastSection = dataSections.back();
            if (kvHolder->offset == lastSection.first + lastSection.second) {
                lastSection.second += kvHolder->computedKVSize + kvHolder->valueSize;
            } else {
                dataSections.emplace_back(kvHolder->offset, kvHolder->computedKVSize + kvHolder->valueSize);
            }
        }
        // do the move
        auto basePtr = ptr + Fixed32Size;
        for (auto &section : dataSections) {
            // memmove() should handle this well: src == dst
            memmove(writePtr, basePtr + section.first, section.second);
            writePtr += section.second;
        }
        // update offset
        if (!encrypter) {
            auto offset = ItemSizeHolderSize;
            for (auto kvHolder : vec) {
                kvHolder->offset = offset;
                offset += kvHolder->computedKVSize + kvHolder->valueSize;
            }
        }
    }
    // hold the fake size of dictionary's serialization result
    output->writeRawVarint32(ItemSizeHolder);
    auto writtenSize = static_cast<size_t>(writePtr - originOutputPtr);
#ifndef MMKV_DISABLE_CRYPT
    if (encrypter) {
        encrypter->encrypt(originOutputPtr, originOutputPtr, writtenSize);
    }
#endif
    assert(writtenSize == totalSize);
    output->seek(writtenSize - ItemSizeHolderSize);
}

#ifndef MMKV_DISABLE_CRYPT

static void memmoveDictionary(MMKVMapCrypt &dic,
                              CodedOutputData *output,
                              uint8_t *ptr,
                              AESCrypt *decrypter,
                              AESCrypt *encrypter,
                              pair<MMBuffer, size_t> &preparedData) {
    // reuse what's already in the file
    vector<KeyValueHolderCrypt *> vec;
    if (!dic.empty()) {
        // sort by offset
        vec.reserve(dic.size());
        for (auto &itr : dic) {
            if (itr.second.type == KeyValueHolderType_Offset) {
                vec.push_back(&itr.second);
            }
        }
        sort(vec.begin(), vec.end(), [](auto left, auto right) { return left->offset < right->offset; });
    }
    auto sizeHolder = ItemSizeHolder, sizeHolderSize = ItemSizeHolderSize;
    if (!vec.empty()) {
        auto smallestOffset = vec.front()->offset;
        if (smallestOffset != ItemSizeHolderSize && smallestOffset <= 5) {
            sizeHolderSize = smallestOffset;
            assert(sizeHolderSize != 0);
            static const uint32_t ItemSizeHolders[] = {0, 0x0f, 0xff, 0xffff, 0xffffff, 0xffffffff};
            sizeHolder = ItemSizeHolders[sizeHolderSize];
        }
    }
    output->writeRawVarint32(static_cast<int32_t>(sizeHolder));
    auto writePtr = output->curWritePointer();
    if (encrypter) {
        encrypter->encrypt(writePtr - sizeHolderSize, writePtr - sizeHolderSize, sizeHolderSize);
    }
    if (!vec.empty()) {
        // merge nearby items to make memmove quicker
        vector<tuple<uint32_t, uint32_t, AESCryptStatus *>> dataSections; // pair(offset, size)
        dataSections.push_back(vec.front()->toTuple());
        for (size_t index = 1, total = vec.size(); index < total; index++) {
            auto kvHolder = vec[index];
            auto &lastSection = dataSections.back();
            if (kvHolder->offset == get<0>(lastSection) + get<1>(lastSection)) {
                get<1>(lastSection) += kvHolder->pbKeyValueSize + kvHolder->keySize + kvHolder->valueSize;
            } else {
                dataSections.push_back(kvHolder->toTuple());
            }
        }
        // do the move
        auto basePtr = ptr + Fixed32Size;
        for (auto &section : dataSections) {
            auto crypter = decrypter->cloneWithStatus(*get<2>(section));
            crypter.decrypt(basePtr + get<0>(section), writePtr, get<1>(section));
            writePtr += get<1>(section);
        }
        // update offset & AESCryptStatus
        if (encrypter) {
            auto offset = sizeHolderSize;
            for (auto kvHolder : vec) {
                kvHolder->offset = offset;
                auto size = kvHolder->pbKeyValueSize + kvHolder->keySize + kvHolder->valueSize;
                encrypter->getCurStatus(kvHolder->cryptStatus);
                encrypter->encrypt(basePtr + offset, basePtr + offset, size);
                offset += size;
            }
        }
    }
    auto &data = preparedData.first;
    if (data.length() > 0) {
        auto dataSize = CodedInputData(data.getPtr(), data.length()).readUInt32();
        if (dataSize > 0) {
            auto dataPtr = (uint8_t *) data.getPtr() + pbRawVarint32Size(dataSize);
            if (encrypter) {
                encrypter->encrypt(dataPtr, writePtr, dataSize);
            } else {
                memcpy(writePtr, dataPtr, dataSize);
            }
            writePtr += dataSize;
        }
    }
    auto writtenSize = static_cast<size_t>(writePtr - output->curWritePointer());
    assert(writtenSize + sizeHolderSize == preparedData.second);
    output->seek(writtenSize);
}

#    define InvalidCryptPtr ((AESCrypt *) (void *) (1))

#endif // MMKV_DISABLE_CRYPT

static void fullWriteBackWholeData(MMBuffer allData, size_t totalSize, CodedOutputData *output) {
    auto originOutputPtr = output->curWritePointer();
    output->writeRawVarint32(ItemSizeHolder);
    if (allData.length() > 0) {
        auto dataSize = CodedInputData(allData.getPtr(), allData.length()).readUInt32();
        if (dataSize > 0) {
            auto dataPtr = (uint8_t *) allData.getPtr() + pbRawVarint32Size(dataSize);
            memcpy(output->curWritePointer(), dataPtr, dataSize);
            output->seek(dataSize);
        }
    }
    [[maybe_unused]] auto writtenSize = (size_t)(output->curWritePointer() - originOutputPtr);
    assert(writtenSize == totalSize);
}

#ifndef MMKV_DISABLE_CRYPT
bool MMKV::doFullWriteBack(pair<MMBuffer, size_t> prepared, AESCrypt *newCrypter, bool needSync) {
    auto ptr = (uint8_t *) m_file->getMemory();
    auto totalSize = prepared.second;
#    ifdef MMKV_IOS
    auto ret = guardForBackgroundWriting(ptr + Fixed32Size, totalSize);
    if (!ret.first) {
        return false;
    }
#    endif

    uint8_t newIV[AES_KEY_LEN];
    auto encrypter = (newCrypter == InvalidCryptPtr) ? nullptr : (newCrypter ? newCrypter : m_crypter);
    if (encrypter) {
        AESCrypt::fillRandomIV(newIV);
        encrypter->resetIV(newIV, sizeof(newIV));
    }

    delete m_output;
    m_output = new CodedOutputData(ptr + Fixed32Size, m_file->getFileSize() - Fixed32Size);
    if (m_crypter) {
        auto decrypter = m_crypter;
        memmoveDictionary(*m_dicCrypt, m_output, ptr, decrypter, encrypter, prepared);
    } else if (prepared.first.length() != 0) {
        auto &preparedData = prepared.first;
        fullWriteBackWholeData(std::move(preparedData), totalSize, m_output);
        if (encrypter) {
            encrypter->encrypt(ptr + Fixed32Size, ptr + Fixed32Size, totalSize);
        }
    } else {
        memmoveDictionary(*m_dic, m_output, ptr, encrypter, totalSize);
    }

    m_actualSize = totalSize;
    if (encrypter) {
        recaculateCRCDigestWithIV(newIV);
    } else {
        recaculateCRCDigestWithIV(nullptr);
    }
    m_hasFullWriteback = true;
    // make sure lastConfirmedMetaInfo is saved if needed
    if (needSync) {
        sync(MMKV_SYNC);
    }
    return true;
}

#else // MMKV_DISABLE_CRYPT

bool MMKV::doFullWriteBack(pair<MMBuffer, size_t> prepared, AESCrypt *, bool needSync) {
    auto ptr = (uint8_t *) m_file->getMemory();
    auto totalSize = prepared.second;
#    ifdef MMKV_IOS
    auto ret = guardForBackgroundWriting(ptr + Fixed32Size, totalSize);
    if (!ret.first) {
        return false;
    }
#    endif

    delete m_output;
    m_output = new CodedOutputData(ptr + Fixed32Size, m_file->getFileSize() - Fixed32Size);
    if (prepared.first.length() != 0) {
        auto &preparedData = prepared.first;
        fullWriteBackWholeData(std::move(preparedData), totalSize, m_output);
    } else {
        constexpr AESCrypt *encrypter = nullptr;
        memmoveDictionary(*m_dic, m_output, ptr, encrypter, totalSize);
    }

    m_actualSize = totalSize;
    recaculateCRCDigestWithIV(nullptr);
    m_hasFullWriteback = true;
    // make sure lastConfirmedMetaInfo is saved if needed
    if (needSync) {
        sync(MMKV_SYNC);
    }
    return true;
}
#endif // MMKV_DISABLE_CRYPT

#ifndef MMKV_DISABLE_CRYPT
bool MMKV::reKey(const string &cryptKey) {
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_exclusiveProcessLock);
    checkLoadData();
    if (!isFileValid()) {
        MMKVWarning("[%s] file not valid", m_mmapID.c_str());
        return false;
    }

    bool ret = false;
    if (m_crypter) {
        if (cryptKey.length() > 0) {
            string oldKey = this->cryptKey();
            if (cryptKey == oldKey) {
                return true;
            } else {
                // change encryption key
                MMKVInfo("reKey with new aes key");
                auto newCrypt = new AESCrypt(cryptKey.data(), cryptKey.length());
                m_hasFullWriteback = false;
                ret = fullWriteback(newCrypt);
                if (ret) {
                    delete m_crypter;
                    m_crypter = newCrypt;
                } else {
                    delete newCrypt;
                }
            }
        } else {
            // decryption to plain text
            MMKVInfo("reKey to no aes key");
            m_hasFullWriteback = false;
            ret = fullWriteback(InvalidCryptPtr);
            if (ret) {
                delete m_crypter;
                m_crypter = nullptr;
                if (!m_dic) {
                    m_dic = new MMKVMap();
                }
            }
        }
    } else {
        if (cryptKey.length() > 0) {
            // transform plain text to encrypted text
            MMKVInfo("reKey to a aes key");
            m_hasFullWriteback = false;
            auto newCrypt = new AESCrypt(cryptKey.data(), cryptKey.length());
            ret = fullWriteback(newCrypt);
            if (ret) {
                m_crypter = newCrypt;
                if (!m_dicCrypt) {
                    m_dicCrypt = new MMKVMapCrypt();
                }
            } else {
                delete newCrypt;
            }
        } else {
            return true;
        }
    }
    // m_dic or m_dicCrypt is not valid after reKey
    if (ret) {
        clearMemoryCache();
    }
    return ret;
}
#endif

void MMKV::trim() {
    SCOPED_LOCK(m_lock);
    MMKVInfo("prepare to trim %s", m_mmapID.c_str());

    checkLoadData();
    if (!isFileValid()) {
        MMKVWarning("[%s] file not valid", m_mmapID.c_str());
        return;
    }

    if (m_actualSize == 0) {
        clearAll();
        return;
    } else if (m_file->getFileSize() <= m_expectedCapacity) {
        return;
    }
    SCOPED_LOCK(m_exclusiveProcessLock);

    fullWriteback();
    auto oldSize = m_file->getFileSize();
    auto fileSize = oldSize;
    while (fileSize > (m_actualSize + Fixed32Size) * 2) {
        fileSize /= 2;
    }
    fileSize = std::max<size_t>(fileSize, m_expectedCapacity);
    if (oldSize == fileSize) {
        MMKVInfo("there's no need to trim %s with size %zu, actualSize %zu", m_mmapID.c_str(), fileSize, m_actualSize);
        return;
    }

    MMKVInfo("trimming %s from %zu to %zu, actualSize %zu", m_mmapID.c_str(), oldSize, fileSize, m_actualSize);

    if (!m_file->truncate(fileSize)) {
        return;
    }
    fileSize = m_file->getFileSize();
    auto ptr = (uint8_t *) m_file->getMemory();
    delete m_output;
    m_output = new CodedOutputData(ptr + pbFixed32Size(), fileSize - Fixed32Size);
    m_output->seek(m_actualSize);

    MMKVInfo("finish trim %s from %zu to %zu", m_mmapID.c_str(), oldSize, fileSize);
}

void MMKV::clearAll(bool keepSpace) {
    MMKVInfo("cleaning all key-values from [%s]", m_mmapID.c_str());
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_exclusiveProcessLock);

    checkLoadData();
    if (!isFileValid()) {
        MMKVWarning("[%s] file not valid", m_mmapID.c_str());
        return;
    }

    if (m_file->getFileSize() == m_expectedCapacity && m_actualSize == 0) {
        MMKVInfo("nothing to clear for [%s]", m_mmapID.c_str());
        return;
    }

    if (!keepSpace) {
        m_file->truncate(m_expectedCapacity);
    }

#ifndef MMKV_DISABLE_CRYPT
    uint8_t newIV[AES_KEY_LEN];
    AESCrypt::fillRandomIV(newIV);
    if (m_crypter) {
        m_crypter->resetIV(newIV, sizeof(newIV));
    }
    writeActualSize(0, 0, newIV, IncreaseSequence);
#else
    writeActualSize(0, 0, nullptr, IncreaseSequence);
#endif

    m_metaFile->msync(MMKV_SYNC);

    clearMemoryCache(keepSpace);
    loadFromFile();
}

bool MMKV::isFileValid(const string &mmapID, MMKVPath_t *relatePath) {
    MMKVPath_t kvPath = mappedKVPathWithID(mmapID, MMKV_SINGLE_PROCESS, relatePath);
    if (!isFileExist(kvPath)) {
        return true;
    }

    MMKVPath_t crcPath = crcPathWithID(mmapID, MMKV_SINGLE_PROCESS, relatePath);
    if (!isFileExist(crcPath)) {
        return false;
    }

    uint32_t crcFile = 0;
    MMBuffer *data = readWholeFile(crcPath);
    if (data) {
        if (data->getPtr()) {
            MMKVMetaInfo metaInfo;
            metaInfo.read(data->getPtr());
            crcFile = metaInfo.m_crcDigest;
        }
        delete data;
    } else {
        return false;
    }

    uint32_t crcDigest = 0;
    MMBuffer *fileData = readWholeFile(kvPath);
    if (fileData) {
        if (fileData->getPtr() && (fileData->length() >= Fixed32Size)) {
            uint32_t actualSize = 0;
            memcpy(&actualSize, fileData->getPtr(), Fixed32Size);
            if (actualSize > (fileData->length() - Fixed32Size)) {
                delete fileData;
                return false;
            }

            crcDigest = (uint32_t) CRC32(0, (const uint8_t *) fileData->getPtr() + Fixed32Size, (uint32_t) actualSize);
        }
        delete fileData;
        return crcFile == crcDigest;
    } else {
        return false;
    }
}

bool MMKV::removeStorage(const std::string &mmapID, MMKVPath_t *relatePath) {
    auto mmapKey = mmapedKVKey(mmapID, relatePath);
#ifdef MMKV_ANDROID
    auto &realID = mmapKey; // historically Android mistakenly use mmapKey as mmapID
#else
    auto &realID = mmapID;
#endif
    MMKVDebug("mmapKey %s", mmapKey.c_str());

    MMKVPath_t kvPath = mappedKVPathWithID(realID, MMKV_SINGLE_PROCESS, relatePath);
    if (!isFileExist(kvPath)) {
        MMKVWarning("file not exist %s", kvPath.c_str());
        return false;
    }
    MMKVPath_t crcPath = crcPathWithID(realID, MMKV_SINGLE_PROCESS, relatePath);
    if (!isFileExist(crcPath)) {
        MMKVWarning("file not exist %s", crcPath.c_str());
        return false;
    }

    MMKVInfo("remove storage [%s]", mmapID.c_str());
    SCOPED_LOCK(g_instanceLock);

    File crcFile(crcPath, OpenFlag::ReadOnly);
    if (!crcFile.isFileValid()) {
        return false;
    }
    FileLock fileLock(crcFile.getFd());
    InterProcessLock lock(&fileLock, ExclusiveLockType);
    SCOPED_LOCK(&lock);

    auto itr = g_instanceDic->find(mmapKey);
    if (itr != g_instanceDic->end()) {
        itr->second->close();
        // itr is not valid after this
    }

#ifndef MMKV_WIN32
    ::unlink(kvPath.c_str());
    ::unlink(crcPath.c_str());
#else
    DeleteFile(kvPath.c_str());
    DeleteFile(crcPath.c_str());
#endif

    return true;
}

// ---- auto expire ----

uint32_t MMKV::getCurrentTimeInSecond() {
    auto time = ::time(nullptr);
    return static_cast<uint32_t>(time);
}

bool MMKV::doFullWriteBack(MMKVVector &&vec) {
    auto preparedData = prepareEncode(std::move(vec));

    // must clean before write-back and after prepareEncode()
    if (m_crypter) {
        clearDictionary(m_dicCrypt);
    } else {
        clearDictionary(m_dic);
    }

    bool ret = false;
    auto sizeOfDic = preparedData.second;
    auto fileSize = m_file->getFileSize();
    if (sizeOfDic + Fixed32Size <= fileSize) {
        ret = doFullWriteBack(std::move(preparedData), nullptr);
    } else {
        // expandAndWriteBack() will extend file & full rewrite, no need to write back again
        auto newSize = sizeOfDic + Fixed32Size - fileSize;
        ret = expandAndWriteBack(newSize, std::move(preparedData));
    }

    clearMemoryCache();
    return ret;
}

bool MMKV::enableAutoKeyExpire(uint32_t expiredInSeconds) {
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_exclusiveProcessLock);
    checkLoadData();
    if (!isFileValid() || !m_metaFile->isFileValid()) {
        MMKVWarning("[%s] file not valid", m_mmapID.c_str());
        return false;
    }

    if (m_enableCompareBeforeSet) {
        MMKVError("enableCompareBeforeSet will be invalid when Expiration is on");
        m_enableCompareBeforeSet = false;
    }

    if (m_expiredInSeconds != expiredInSeconds) {
        MMKVInfo("expiredInSeconds: %u", expiredInSeconds);
        m_expiredInSeconds = expiredInSeconds;
    }
    m_enableKeyExpire = true;
    if (m_metaInfo->hasFlag(MMKVMetaInfo::EnableKeyExipre)) {
        return true;
    }

    auto autoRecordExpireTime = (m_expiredInSeconds != 0);
    auto time = autoRecordExpireTime ? getCurrentTimeInSecond() + m_expiredInSeconds : 0;
    MMKVInfo("turn on recording expire date for all keys inside [%s] from now %u", m_mmapID.c_str(), time);
    m_metaInfo->setFlag(MMKVMetaInfo::EnableKeyExipre);
    m_metaInfo->m_version = MMKVVersionFlag;

    if (m_file->getFileSize() == m_expectedCapacity && m_actualSize == 0) {
        MMKVInfo("file is new, don't need a full writeback [%s], just update meta file", m_mmapID.c_str());
        writeActualSize(0, 0, nullptr, IncreaseSequence);
        m_metaFile->msync(MMKV_SYNC);
        return true;
    }

    MMKVVector vec;
    auto packKeyValue = [&](const MMKVKey_t &key, const MMBuffer &value) {
        MMBuffer data(value.length() + Fixed32Size);
        auto ptr = (uint8_t *) data.getPtr();
        memcpy(ptr, value.getPtr(), value.length());
        memcpy(ptr + value.length(), &time, Fixed32Size);
        vec.emplace_back(key, std::move(data));
    };

    auto basePtr = (uint8_t *) (m_file->getMemory()) + Fixed32Size;
#ifndef MMKV_DISABLE_CRYPT
    if (m_crypter) {
        for (auto &pair : *m_dicCrypt) {
            auto &key = pair.first;
            auto &value = pair.second;
            auto buffer = value.toMMBuffer(basePtr, m_crypter);
            packKeyValue(key, buffer);
        }
    } else
#endif
    {
        for (auto &pair : *m_dic) {
            auto &key = pair.first;
            auto &value = pair.second;
            auto buffer = value.toMMBuffer(basePtr);
            packKeyValue(key, buffer);
        }
    }

    return doFullWriteBack(std::move(vec));
}

bool MMKV::disableAutoKeyExpire() {
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_exclusiveProcessLock);
    checkLoadData();
    if (!isFileValid() || !m_metaFile->isFileValid()) {
        MMKVWarning("[%s] file not valid", m_mmapID.c_str());
        return false;
    }

    m_expiredInSeconds = 0;
    m_enableKeyExpire = false;
    if (!m_metaInfo->hasFlag(MMKVMetaInfo::EnableKeyExipre)) {
        return true;
    }

    MMKVInfo("erase previous recorded expire date for all keys inside [%s]", m_mmapID.c_str());
    m_metaInfo->unsetFlag(MMKVMetaInfo::EnableKeyExipre);
    m_metaInfo->m_version = MMKVVersionFlag;

    if (m_file->getFileSize() == m_expectedCapacity && m_actualSize == 0) {
        MMKVInfo("file is new, don't need a full write-back [%s], just update meta file", m_mmapID.c_str());
        writeActualSize(0, 0, nullptr, IncreaseSequence);
        m_metaFile->msync(MMKV_SYNC);
        return true;
    }

    MMKVVector vec;
    auto packKeyValue = [&](const MMKVKey_t &key, const MMBuffer &value) {
        assert(value.length() >= Fixed32Size);
        MMBuffer data(value.length() - Fixed32Size);
        auto ptr = (uint8_t *) data.getPtr();
        memcpy(ptr, value.getPtr(), value.length() - Fixed32Size);
        vec.emplace_back(key, std::move(data));
    };

    auto basePtr = (uint8_t *) (m_file->getMemory()) + Fixed32Size;
#ifndef MMKV_DISABLE_CRYPT
    if (m_crypter) {
        for (auto &pair : *m_dicCrypt) {
            auto &key = pair.first;
            auto &value = pair.second;
            auto buffer = value.toMMBuffer(basePtr, m_crypter);
            packKeyValue(key, buffer);
        }
    } else
#endif
    {
        for (auto &pair : *m_dic) {
            auto &key = pair.first;
            auto &value = pair.second;
            auto buffer = value.toMMBuffer(basePtr);
            packKeyValue(key, buffer);
        }
    }

    return doFullWriteBack(std::move(vec));
}

uint32_t MMKV::getExpireTimeForKey(MMKVKey_t key) {
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_sharedProcessLock);
    checkLoadData();

    if (!m_enableKeyExpire || mmkv_key_length(key) == 0) {
        return 0;
    }
    auto raw = getRawDataForKey(key);
    assert(raw.length() == 0 || raw.length() >= Fixed32Size);
    if (raw.length() < Fixed32Size) {
        return 0;
    }
    auto ptr = (const uint8_t *) raw.getPtr() + raw.length() - Fixed32Size;
    auto time = *(const uint32_t *) ptr;
    return time;
}

mmkv::MMBuffer MMKV::getDataWithoutMTimeForKey(MMKVKey_t key) {
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_sharedProcessLock);
    checkLoadData();

    auto raw = getRawDataForKey(key);
    assert(raw.length() == 0 || raw.length() >= Fixed32Size);
    if (raw.length() < Fixed32Size) {
        return raw;
    }
    auto newLength = raw.length() - Fixed32Size;
    if (m_enableKeyExpire) {
        auto ptr = (const uint8_t *) raw.getPtr() + newLength;
        auto time = *(const uint32_t *) ptr;
        if (time != ExpireNever && time <= getCurrentTimeInSecond()) {
#ifdef MMKV_APPLE
            MMKVInfo("deleting expired key [%@] in mmkv [%s], due date %u", key, m_mmapID.c_str(), time);
#else
            MMKVInfo("deleting expired key [%s] in mmkv [%s], due date %u", key.c_str(), m_mmapID.c_str(), time);
#endif
            removeValueForKey(key);
            return MMBuffer();
        }
    }
    return MMBuffer(std::move(raw), newLength);
}

#define NOOP ((void) 0)

size_t MMKV::filterExpiredKeys() {
    if (!m_enableKeyExpire || (m_crypter ? m_dicCrypt->empty() : m_dic->empty())) {
        return 0;
    }
    SCOPED_LOCK(m_sharedProcessLock);

    auto now = getCurrentTimeInSecond();
    MMKVInfo("filtering expired keys inside [%s] now: %u, m_expiredInSeconds: %u", m_mmapID.c_str(), now,
             m_expiredInSeconds);

    size_t count = 0;
    auto basePtr = (uint8_t *) (m_file->getMemory()) + Fixed32Size;
#ifndef MMKV_DISABLE_CRYPT
    if (m_crypter) {
        for (auto itr = m_dicCrypt->begin(); itr != m_dicCrypt->end(); NOOP) {
            auto &kvHolder = itr->second;
            assert(kvHolder.realValueSize() >= Fixed32Size);
            auto buffer = kvHolder.toMMBuffer(basePtr, m_crypter);
            auto ptr = (uint8_t *) buffer.getPtr();
            ptr += buffer.length() - Fixed32Size;
            auto time = *(const uint32_t *) ptr;
            if (time != ExpireNever && time <= now) {
                auto oldKey = itr->first;
                itr = m_dicCrypt->erase(itr);
#    ifdef MMKV_APPLE
                MMKVInfo("deleting expired key [%@], due date %u", oldKey, time);
                [oldKey release];
#    else
                MMKVInfo("deleting expired key [%s], due date %u", oldKey.c_str(), time);
#    endif
                count++;
            } else {
                itr++;
            }
        }
    } else
#endif // !MMKV_DISABLE_CRYPT
    {
        for (auto itr = m_dic->begin(); itr != m_dic->end(); NOOP) {
            auto &kvHolder = itr->second;
            assert(kvHolder.valueSize >= Fixed32Size);
            auto ptr = basePtr + kvHolder.offset + kvHolder.computedKVSize;
            ptr += kvHolder.valueSize - Fixed32Size;
            auto time = *(const uint32_t *) ptr;
            if (time != ExpireNever && time <= now) {
                auto oldKey = itr->first;
                itr = m_dic->erase(itr);
#ifdef MMKV_APPLE
                MMKVInfo("deleting expired key [%@], due date %u", oldKey, time);
                [oldKey release];
#else
                MMKVInfo("deleting expired key [%s], due date %u", oldKey.c_str(), time);
#endif
                count++;
            } else {
                itr++;
            }
        }
    }
    if (count != 0) {
        MMKVInfo("deleted %zu expired keys inside [%s]", count, m_mmapID.c_str());
    }
    return count;
}

bool MMKV::enableCompareBeforeSet() {
    MMKVInfo("enableCompareBeforeSet for [%s]", m_mmapID.c_str());
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_exclusiveProcessLock);

    assert(!m_enableKeyExpire && "enableCompareBeforeSet is invalid when Expiration is on");
    assert(!m_dicCrypt && "enableCompareBeforeSet is invalid when key encryption is on");
    if (m_enableKeyExpire || m_dicCrypt) {
        return false;
    }

    m_enableCompareBeforeSet = true;
    return true;
}

bool MMKV::disableCompareBeforeSet() {
    MMKVInfo("disableCompareBeforeSet for [%s]", m_mmapID.c_str());
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_exclusiveProcessLock);

    assert(!m_enableKeyExpire && "disableCompareBeforeSet is invalid when Expiration is on");
    assert(!m_dicCrypt && "disableCompareBeforeSet is invalid when key encryption is on");
    if (m_enableKeyExpire || m_dicCrypt) {
        return false;
    }

    m_enableCompareBeforeSet = false;
    return true;
}

MMKV_NAMESPACE_END
