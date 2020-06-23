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
#include <cstring>

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

constexpr uint32_t Fixed32Size = pbFixed32Size();

MMKV_NAMESPACE_BEGIN

// really dirty work

void MMKV::loadFromFile() {
    if (m_metaFile->isFileValid()) {
        m_metaInfo->read(m_metaFile->getMemory());
    }
    if (m_crypter) {
        if (m_metaInfo->m_version >= MMKVVersionRandomIV) {
            m_crypter->resetIV(m_metaInfo->m_vector, sizeof(m_metaInfo->m_vector));
        }
    }

    if (!m_file->isFileValid()) {
        m_file->reloadFromFile();
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
                if (m_crypter) {
                    MiniPBCoder::greedyDecodeMap(m_dicCrypt, inputBuffer, m_crypter);
                } else {
                    MiniPBCoder::greedyDecodeMap(m_dic, inputBuffer);
                }
            } else {
                if (m_crypter) {
                    MiniPBCoder::decodeMap(m_dicCrypt, inputBuffer, m_crypter);
                } else {
                    MiniPBCoder::decodeMap(m_dic, inputBuffer);
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
        if (m_crypter) {
            MMKVInfo("loaded [%s] with %zu values", m_mmapID.c_str(), m_dicCrypt.size());
        } else {
            MMKVInfo("loaded [%s] with %zu values", m_mmapID.c_str(), m_dic.size());
        }
    }

    m_needLoadFromFile = false;
}

// read from last m_position
void MMKV::partialLoadFromFile() {
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
                    if (m_crypter) {
                        MiniPBCoder::greedyDecodeMap(m_dicCrypt, inputBuffer, m_crypter, position);
                    } else {
                        MiniPBCoder::greedyDecodeMap(m_dic, inputBuffer, position);
                    }
                    m_output->seek(addedSize);
                    m_hasFullWriteback = false;

                    if (m_crypter) {
                        MMKVDebug("partial loaded [%s] with %zu values", m_mmapID.c_str(), m_dicCrypt.size());
                    } else {
                        MMKVDebug("partial loaded [%s] with %zu values", m_mmapID.c_str(), m_dic.size());
                    }
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

static pair<MMBuffer, size_t> prepareEncode(const mmkv::MMKVMap &dic) {
    // make some room for placeholder
    size_t totalSize = ItemSizeHolderSize;
    for (auto &itr : dic) {
        auto &kvHolder = itr.second;
        totalSize += kvHolder.computedKVSize + kvHolder.valueSize;
    }
    return make_pair(MMBuffer(), totalSize);
}

static pair<MMBuffer, size_t> prepareEncode(const mmkv::MMKVMapCrypt &dic) {
    // make some room for placeholder
    size_t totalSize = ItemSizeHolderSize;
    MMKVVector vec;
    for (auto &itr : dic) {
        auto &kvHolder = itr.second;
        if (kvHolder.type == KeyValueHolderType_Offset) {
            totalSize += kvHolder.pbKeyValueSize + kvHolder.keySize + kvHolder.valueSize;
        } else {
            vec.emplace_back(itr.first, kvHolder.toMMBuffer(nullptr, nullptr));
        }
    }
    if (vec.empty()) {
        return make_pair(MMBuffer(), totalSize);
    }
    auto buffer = MiniPBCoder::encodeDataWithObject(vec);
    // skip the pb size of buffer
    auto sizeOfMap = CodedInputData(buffer.getPtr(), buffer.length()).readUInt32();
    totalSize += sizeOfMap;
    return make_pair(move(buffer), totalSize);
}

// since we use append mode, when -[setData: forKey:] many times, space may not be enough
// try a full rewrite to make space
bool MMKV::ensureMemorySize(size_t newSize) {
    if (!isFileValid()) {
        MMKVWarning("[%s] file not valid", m_mmapID.c_str());
        return false;
    }

    if (newSize >= m_output->spaceLeft() || (m_crypter ? m_dicCrypt.empty() : m_dic.empty())) {
        // try a full rewrite to make space
        auto fileSize = m_file->getFileSize();
        auto preparedData = m_crypter ? prepareEncode(m_dicCrypt) : prepareEncode(m_dic);
        auto sizeOfDic = preparedData.second;
        size_t lenNeeded = sizeOfDic + Fixed32Size + newSize;
        size_t dicCount = m_crypter ? m_dicCrypt.size() : m_dic.size();
        size_t avgItemSize = lenNeeded / std::max<size_t>(1, dicCount);
        size_t futureUsage = avgItemSize * std::max<size_t>(8, (dicCount + 1) / 2);
        // 1. no space for a full rewrite, double it
        // 2. or space is not large enough for future usage, double it to avoid frequently full rewrite
        if (lenNeeded >= fileSize || (lenNeeded + futureUsage) >= fileSize) {
            size_t oldSize = fileSize;
            do {
                fileSize *= 2;
            } while (lenNeeded + futureUsage >= fileSize);
            MMKVInfo("extending [%s] file size from %zu to %zu, incoming size:%zu, future usage:%zu", m_mmapID.c_str(),
                     oldSize, fileSize, newSize, futureUsage);

            // if we can't extend size, rollback to old state
            if (!m_file->truncate(fileSize)) {
                return false;
            }

            // check if we fail to make more space
            if (!isFileValid()) {
                MMKVWarning("[%s] file not valid", m_mmapID.c_str());
                return false;
            }
        }
        return doFullWriteBack(move(preparedData), nullptr);
    }
    return true;
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
    protectFromBackgroundWriting(m_file->getMemory(), Fixed32Size, ^{
        memcpy(m_file->getMemory(), &actualSize, Fixed32Size);
    });
#else
    memcpy(m_file->getMemory(), &actualSize, Fixed32Size);
#endif
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
    if (unlikely(iv)) {
        memcpy(m_metaInfo->m_vector, iv, sizeof(m_metaInfo->m_vector));
        if (m_metaInfo->m_version < MMKVVersionRandomIV) {
            m_metaInfo->m_version = MMKVVersionRandomIV;
        }
        needsFullWrite = true;
    }
    if (unlikely(increaseSequence)) {
        m_metaInfo->m_sequence++;
        m_metaInfo->m_lastConfirmedMetaInfo.lastActualSize = static_cast<uint32_t>(size);
        m_metaInfo->m_lastConfirmedMetaInfo.lastCRCDigest = crcDigest;
        if (m_metaInfo->m_version < MMKVVersionActualSize) {
            m_metaInfo->m_version = MMKVVersionActualSize;
        }
        needsFullWrite = true;
    }
#ifdef MMKV_IOS
    return protectFromBackgroundWriting(m_metaFile->getMemory(), sizeof(MMKVMetaInfo), ^{
        if (unlikely(needsFullWrite)) {
            m_metaInfo->write(m_metaFile->getMemory());
        } else {
            m_metaInfo->writeCRCAndActualSizeOnly(m_metaFile->getMemory());
        }
    });
#else
    if (unlikely(needsFullWrite)) {
        m_metaInfo->write(m_metaFile->getMemory());
    } else {
        m_metaInfo->writeCRCAndActualSizeOnly(m_metaFile->getMemory());
    }
    return true;
#endif
}

MMBuffer MMKV::getDataForKey(MMKVKey_t key) {
    checkLoadData();
    if (m_crypter) {
        auto itr = m_dicCrypt.find(key);
        if (itr != m_dicCrypt.end()) {
            auto basePtr = (uint8_t *) (m_file->getMemory()) + Fixed32Size;
            return itr->second.toMMBuffer(basePtr, m_crypter);
        }
    } else {
        auto itr = m_dic.find(key);
        if (itr != m_dic.end()) {
            auto basePtr = (uint8_t *) (m_file->getMemory()) + Fixed32Size;
            return itr->second.toMMBuffer(basePtr);
        }
    }
    MMBuffer nan;
    return nan;
}

thread_local AESCryptStatus t_status;

bool MMKV::setDataForKey(MMBuffer &&data, MMKVKey_t key, bool isDataHolder) {
    if ((!isDataHolder && data.length() == 0) || isKeyEmpty(key)) {
        return false;
    }
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_exclusiveProcessLock);
    checkLoadData();

    if (m_crypter) {
        if (isDataHolder) {
            auto sizeNeededForData = pbRawVarint32Size((uint32_t) data.length()) + data.length();
            if (!KeyValueHolderCrypt::isValueStoredAsOffset(sizeNeededForData)) {
                data = MiniPBCoder::encodeDataWithObject(data);
                isDataHolder = false;
            }
        }
        auto itr = m_dicCrypt.find(key);
        if (itr != m_dicCrypt.end()) {
#ifdef MMKV_APPLE
            auto ret = appendDataWithKey(data, key, itr->second, isDataHolder);
#else
            auto ret = appendDataWithKey(data, key, isDataHolder);
#endif
            if (!ret.first) {
                return false;
            }
            if (KeyValueHolderCrypt::isValueStoredAsOffset(ret.second.valueSize)) {
                KeyValueHolderCrypt kvHolder(ret.second.keySize, ret.second.valueSize, ret.second.offset);
                memcpy(kvHolder.cryptStatus(), &t_status, sizeof(t_status));
                itr->second = move(kvHolder);
            } else {
                itr->second = KeyValueHolderCrypt(move(data));
            }
        } else {
            auto ret = appendDataWithKey(data, key, isDataHolder);
            if (!ret.first) {
                return false;
            }
            if (KeyValueHolderCrypt::isValueStoredAsOffset(ret.second.valueSize)) {
                auto r = m_dicCrypt.emplace(
                    key, KeyValueHolderCrypt(ret.second.keySize, ret.second.valueSize, ret.second.offset));
                if (r.second) {
                    memcpy(r.first->second.cryptStatus(), &t_status, sizeof(t_status));
                }
            } else {
                m_dicCrypt.emplace(key, KeyValueHolderCrypt(move(data)));
            }
        }
    } else {
        auto itr = m_dic.find(key);
        if (itr != m_dic.end()) {
            auto ret = appendDataWithKey(data, itr->second, isDataHolder);
            if (!ret.first) {
                return false;
            }
            itr->second = std::move(ret.second);
        } else {
            auto ret = appendDataWithKey(data, key, isDataHolder);
            if (!ret.first) {
                return false;
            }
            m_dic.emplace(key, std::move(ret.second));
        }
    }
    m_hasFullWriteback = false;
#ifdef MMKV_APPLE
    [key retain];
#endif
    return true;
}

bool MMKV::removeDataForKey(MMKVKey_t key) {
    if (isKeyEmpty(key)) {
        return false;
    }

    if (m_crypter) {
        auto itr = m_dicCrypt.find(key);
        if (itr != m_dicCrypt.end()) {
            m_hasFullWriteback = false;
            static MMBuffer nan;
#ifdef MMKV_APPLE
            auto ret = appendDataWithKey(nan, key, itr->second);
#else
            auto ret = appendDataWithKey(nan, key);
#endif
            if (ret.first) {
#ifdef MMKV_APPLE
                [itr->first release];
#endif
                m_dicCrypt.erase(itr);
            }
            return ret.first;
        }
    } else {
        auto itr = m_dic.find(key);
        if (itr != m_dic.end()) {
            m_hasFullWriteback = false;
            static MMBuffer nan;
            auto ret = appendDataWithKey(nan, itr->second);
            if (ret.first) {
#ifdef MMKV_APPLE
                [itr->first release];
#endif
                m_dic.erase(itr);
            }
            return ret.first;
        }
    }

    return false;
}

pair<bool, KeyValueHolder>
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

    if (m_crypter) {
        if (KeyValueHolderCrypt::isValueStoredAsOffset(valueLength)) {
            m_crypter->getCurStatus(t_status);
        }
    }

#ifdef MMKV_IOS
    auto ret = protectFromBackgroundWriting(m_output->curWritePointer(), size, ^{
        if (isKeyEncoded) {
            m_output->writeRawData(keyData);
        } else {
            m_output->writeData(keyData);
        }
        if (isDataHolder) {
            m_output->writeRawVarint32((int32_t) valueLength);
        }
        m_output->writeData(data); // note: write size of data
    });
    if (!ret) {
        return make_pair(false, KeyValueHolder());
    }
#else
    if (isKeyEncoded) {
        m_output->writeRawData(keyData);
    } else {
        m_output->writeData(keyData);
    }
    if (isDataHolder) {
        m_output->writeRawVarint32((int32_t) valueLength);
    }
    m_output->writeData(data); // note: write size of data
#endif

    auto offset = static_cast<uint32_t>(m_actualSize);
    auto ptr = (uint8_t *) m_file->getMemory() + Fixed32Size + m_actualSize;
    if (m_crypter) {
        m_crypter->encrypt(ptr, ptr, size);
    }
    m_actualSize += size;
    updateCRCDigest(ptr, size);

    return make_pair(true, KeyValueHolder(originKeyLength, valueLength, offset));
}

pair<bool, KeyValueHolder> MMKV::appendDataWithKey(const MMBuffer &data, MMKVKey_t key, bool isDataHolder) {
#ifdef MMKV_APPLE
    auto oData = [key dataUsingEncoding:NSUTF8StringEncoding];
    auto keyData = MMBuffer(oData, MMBufferNoCopy);
#else
    auto keyData = MMBuffer((void *) key.data(), key.size(), MMBufferNoCopy);
#endif
    return doAppendDataWithKey(data, keyData, isDataHolder, static_cast<uint32_t>(keyData.length()));
}

pair<bool, KeyValueHolder>
MMKV::appendDataWithKey(const MMBuffer &data, const KeyValueHolder &kvHolder, bool isDataHolder) {
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

bool MMKV::fullWriteback(AESCrypt *newCrypter) {
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

    if (m_crypter ? m_dicCrypt.empty() : m_dic.empty()) {
        clearAll();
        return true;
    }

    auto preparedData = m_crypter ? prepareEncode(m_dicCrypt) : prepareEncode(m_dic);
    auto sizeOfDic = preparedData.second;
    SCOPED_LOCK(m_exclusiveProcessLock);
    if (sizeOfDic > 0) {
        auto fileSize = m_file->getFileSize();
        if (sizeOfDic + Fixed32Size <= fileSize) {
            return doFullWriteBack(move(preparedData), newCrypter);
        } else {
            assert(0);
            assert(newCrypter == nullptr);
            // ensureMemorySize will extend file & full rewrite, no need to write back again
            return ensureMemorySize(sizeOfDic + Fixed32Size - fileSize);
        }
    }
    return false;
}

// we don't need to really serialize the dictionary, just reuse what's already in the file
static void
memmoveDictionary(MMKVMap &dic, CodedOutputData *output, uint8_t *ptr, AESCrypt *encrypter, size_t totalSize) {
    // hold the fake size of dictionay's serialization result
    auto originOutputPtr = output->curWritePointer();
    output->writeRawVarint32(ItemSizeHolder);
    auto writePtr = output->curWritePointer();
    // reuse what's already in the file
    if (!dic.empty()) {
        // sort by offset
        vector<KeyValueHolder *> vec;
        vec.reserve(dic.size());
        for (auto &itr : dic) {
            vec.push_back(&itr.second);
        }
        sort(vec.begin(), vec.end(), [](auto left, auto right) { return left->offset < right->offset; });

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
            auto offset = static_cast<uint32_t>(output->curWritePointer() - basePtr);
            for (auto kvHolder : vec) {
                kvHolder->offset = offset;
                offset += kvHolder->computedKVSize + kvHolder->valueSize;
            }
        }
    }
    auto writtenSize = static_cast<size_t>(writePtr - originOutputPtr);
    if (encrypter) {
        encrypter->encrypt(originOutputPtr, originOutputPtr, writtenSize);
    }
    assert(writtenSize == totalSize);
    output->seek(writtenSize - ItemSizeHolderSize);
}

static void memmoveDictionary(MMKVMapCrypt &dic,
                              CodedOutputData *output,
                              uint8_t *ptr,
                              AESCrypt *decrypter,
                              AESCrypt *encrypter,
                              pair<MMBuffer, size_t> &preparedData) {
    // hold the fake size of dictionay's serialization result
    output->writeRawVarint32(ItemSizeHolder);
    auto writePtr = output->curWritePointer();
    if (encrypter) {
        encrypter->encrypt(writePtr - ItemSizeHolderSize, writePtr - ItemSizeHolderSize, ItemSizeHolderSize);
    }
    // reuse what's already in the file
    if (!dic.empty()) {
        // sort by offset
        vector<KeyValueHolderCrypt *> vec;
        vec.reserve(dic.size());
        for (auto &itr : dic) {
            if (itr.second.type == KeyValueHolderType_Offset) {
                vec.push_back(&itr.second);
            }
        }
        if (vec.empty()) {
            goto WRITE_DATA;
        }
        sort(vec.begin(), vec.end(), [](auto left, auto right) { return left->offset < right->offset; });

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
            auto offset = static_cast<uint32_t>(output->curWritePointer() - basePtr);
            for (auto kvHolder : vec) {
                kvHolder->offset = offset;
                auto size = kvHolder->pbKeyValueSize + kvHolder->keySize + kvHolder->valueSize;
                encrypter->getCurStatus(*kvHolder->cryptStatus());
                encrypter->encrypt(basePtr + offset, basePtr + offset, size);
                offset += size;
            }
        }
    }
WRITE_DATA:
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
    auto totalSize = preparedData.second;
    assert(writtenSize + ItemSizeHolderSize == totalSize);
    output->seek(writtenSize);
}

#define InvalidCryptPtr ((AESCrypt *) (void *) (1))

bool MMKV::doFullWriteBack(pair<MMBuffer, size_t> preparedData, AESCrypt *newCrypter) {
#ifdef MMKV_IOS
    AESCryptStatus oldStatus;
    if (m_crypter) {
        m_crypter->getCurStatus(oldStatus);
    }
#endif
    uint8_t newIV[AES_KEY_LEN];
    auto decrypter = m_crypter;
    auto encrypter = (newCrypter == InvalidCryptPtr) ? nullptr : (newCrypter ? newCrypter : m_crypter);
    if (encrypter) {
        AESCrypt::fillRandomIV(newIV);
        encrypter->resetIV(newIV, sizeof(newIV));
    }

    auto totalSize = preparedData.second;
    auto ptr = (uint8_t *) m_file->getMemory();
    delete m_output;
    m_output = new CodedOutputData(ptr + Fixed32Size, m_file->getFileSize() - Fixed32Size);
#ifdef MMKV_IOS
    pair<MMBuffer, size_t> *preparedDataPtr = &preparedData;
    auto ret = protectFromBackgroundWriting(m_output->curWritePointer(), totalSize, ^{
        if (m_crypter) {
            memmoveDictionary(m_dicCrypt, m_output, ptr, decrypter, encrypter, *preparedDataPtr);
        } else {
            memmoveDictionary(m_dic, m_output, ptr, encrypter, totalSize);
        }
    });
    if (!ret) {
        // revert everything
        if (m_crypter) {
            m_crypter->resetStatus(oldStatus);
        }
        delete m_output;
        m_output = new CodedOutputData(ptr + Fixed32Size, m_file->getFileSize() - Fixed32Size);
        m_output->seek(m_actualSize);
        return false;
    }
#else
    if (m_crypter) {
        memmoveDictionary(m_dicCrypt, m_output, ptr, decrypter, encrypter, preparedData);
    } else {
        memmoveDictionary(m_dic, m_output, ptr, encrypter, totalSize);
    }
#endif

    m_actualSize = totalSize;
    if (encrypter) {
        recaculateCRCDigestWithIV(newIV);
    } else {
        recaculateCRCDigestWithIV(nullptr);
    }
    m_hasFullWriteback = true;
    // make sure lastConfirmedMetaInfo is saved
    sync(MMKV_SYNC);
    return true;
}

bool MMKV::reKey(const string &cryptKey) {
    SCOPED_LOCK(m_lock);
    checkLoadData();

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
                ret = fullWriteback(newCrypt);
                if (ret) {
                    delete m_crypter;
                    m_crypter = newCrypt;
                }
            }
        } else {
            // decryption to plain text
            MMKVInfo("reKey to no aes key");
            ret = fullWriteback(InvalidCryptPtr);
            if (ret) {
                delete m_crypter;
                m_crypter = nullptr;
            }
        }
    } else {
        if (cryptKey.length() > 0) {
            // transform plain text to encrypted text
            MMKVInfo("reKey to a aes key");
            auto newCrypt = new AESCrypt(cryptKey.data(), cryptKey.length());
            ret = fullWriteback(newCrypt);
            if (ret) {
                m_crypter = newCrypt;
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

void MMKV::trim() {
    SCOPED_LOCK(m_lock);
    MMKVInfo("prepare to trim %s", m_mmapID.c_str());

    checkLoadData();

    if (m_actualSize == 0) {
        clearAll();
        return;
    } else if (m_file->getFileSize() <= DEFAULT_MMAP_SIZE) {
        return;
    }
    SCOPED_LOCK(m_exclusiveProcessLock);

    fullWriteback();
    auto oldSize = m_file->getFileSize();
    auto fileSize = oldSize;
    while (fileSize > (m_actualSize + Fixed32Size) * 2) {
        fileSize /= 2;
    }
    fileSize = std::max<size_t>(fileSize, DEFAULT_MMAP_SIZE);
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

void MMKV::clearAll() {
    MMKVInfo("cleaning all key-values from [%s]", m_mmapID.c_str());
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_exclusiveProcessLock);

    if (m_needLoadFromFile) {
        m_file->reloadFromFile();
    }

    if (m_file->getFileSize() == DEFAULT_MMAP_SIZE && m_actualSize == 0) {
        MMKVInfo("nothing to clear for [%s]", m_mmapID.c_str());
        return;
    }
    m_file->truncate(DEFAULT_MMAP_SIZE);

    uint8_t newIV[AES_KEY_LEN];
    AESCrypt::fillRandomIV(newIV);
    if (m_crypter) {
        m_crypter->resetIV(newIV, sizeof(newIV));
    }
    writeActualSize(0, 0, newIV, IncreaseSequence);
    m_metaFile->msync(MMKV_SYNC);

    clearMemoryCache();
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

MMKV_NAMESPACE_END