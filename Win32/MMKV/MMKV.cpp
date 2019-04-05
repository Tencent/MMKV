// MMKV.cpp : Defines the exported functions for the DLL application.
//

/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2018 THL A29 Limited, a Tencent company.
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

#include "pch.h"

#include "CodedInputData.h"
#include "CodedOutputData.h"
#include "InterProcessLock.h"
#include "MMBuffer.h"
#include "MMKV.h"
#include "MMKVLog.h"
#include "MiniPBCoder.h"
#include "MmapedFile.h"
#include "PBUtility.h"
#include "ScopedLock.hpp"
#include "aes/AESCrypt.h"
#include "aes/openssl/md5.h"
#include "crc32/Checksum.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace std;
using namespace mmkv;

static unordered_map<std::string, MMKV *> *g_instanceDic;
static ThreadLock g_instanceLock;
static std::wstring g_rootDir;
static MMKV::ErrorHandler g_errorHandler;
MMKV::LogHandler g_logHandler;

#define DEFAULT_MMAP_ID "mmkv.default"
#define SPECIAL_CHARACTER_DIRECTORY_NAME L"specialCharacter"
constexpr uint32_t Fixed32Size = pbFixed32Size(0);

static wstring mappedKVPathWithID(const string &mmapID, MMKVMode mode);
static wstring crcPathWithID(const string &mmapID, MMKVMode mode);
static void mkSpecialCharacterFileDirectory();
static string md5(const string &value);
static wstring encodeFilePath(const string &mmapID);

static MMKVRecoverStrategic onMMKVCRCCheckFail(const std::string &mmapID);
static MMKVRecoverStrategic onMMKVFileLengthError(const std::string &mmapID);

enum : bool {
    KeepSequence = false,
    IncreaseSequence = true,
};

MMKV::MMKV(const std::string &mmapID, MMKVMode mode, int size, string *cryptKey)
    : m_mmapID(mmapID)
    , m_path(mappedKVPathWithID(m_mmapID, mode))
    , m_crcPath(crcPathWithID(m_mmapID, mode))
    , m_metaFile(m_crcPath, DEFAULT_MMAP_SIZE)
    , m_crypter(nullptr)
    , m_fileLock(m_metaFile.getFd())
    , m_sharedProcessLock(&m_fileLock, SharedLockType)
    , m_exclusiveProcessLock(&m_fileLock, ExclusiveLockType)
    , m_isInterProcess((mode & MMKV_MULTI_PROCESS) != 0) {
    m_fd = INVALID_HANDLE_VALUE;
    m_ptr = nullptr;
    m_size = 0;
    m_actualSize = 0;
    m_output = nullptr;

    if (cryptKey && cryptKey->length() > 0) {
        m_crypter = new AESCrypt((const unsigned char *) cryptKey->data(), cryptKey->length());
    }

    m_needLoadFromFile = true;
    m_hasFullWriteback = false;

    m_crcDigest = 0;

    m_sharedProcessLock.m_enable = m_isInterProcess;
    m_exclusiveProcessLock.m_enable = m_isInterProcess;

    // sensitive zone
    {
        SCOPEDLOCK(m_sharedProcessLock);
        loadFromFile();
    }
}

MMKV::~MMKV() {
    clearMemoryState();

    if (m_crypter) {
        delete m_crypter;
        m_crypter = nullptr;
    }
}

MMKV *MMKV::defaultMMKV(MMKVMode mode, string *cryptKey) {
    return mmkvWithID(DEFAULT_MMAP_ID, mode, cryptKey);
}

void initialize() {
    g_instanceDic = new unordered_map<std::string, MMKV *>;
    g_instanceLock = ThreadLock();

    MMKVInfo("page size:%zd", DEFAULT_MMAP_SIZE);
}

void MMKV::initializeMMKV(const std::wstring &rootDir) {
    static volatile ThreadOnceToken onceToken = ThreadOnceUninitialized;
    ThreadLock::ThreadOnce(onceToken, initialize);

    g_rootDir = rootDir;
    mkPath(g_rootDir);

    MMKVInfo("root dir: %ws", g_rootDir.c_str());
}

MMKV *MMKV::mmkvWithID(const std::string &mmapID, MMKVMode mode, string *cryptKey) {
    if (mmapID.empty()) {
        return nullptr;
    }
    SCOPEDLOCK(g_instanceLock);

    auto itr = g_instanceDic->find(mmapID);
    if (itr != g_instanceDic->end()) {
        MMKV *kv = itr->second;
        return kv;
    }
    auto kv = new MMKV(mmapID, mode, DEFAULT_MMAP_SIZE, cryptKey);
    (*g_instanceDic)[mmapID] = kv;
    return kv;
}

void MMKV::onExit() {
    SCOPEDLOCK(g_instanceLock);

    for (auto &itr : *g_instanceDic) {
        MMKV *kv = itr.second;
        kv->sync();
        kv->clearMemoryState();
    }
}

const std::string &MMKV::mmapID() {
    return m_mmapID;
}

std::string MMKV::cryptKey() {
    SCOPEDLOCK(m_lock);

    if (m_crypter) {
        char key[AES_KEY_LEN];
        m_crypter->getKey(key);
        return string(key, strnlen(key, AES_KEY_LEN));
    }
    return "";
}

// really dirty work

void decryptBuffer(AESCrypt &crypter, MMBuffer &inputBuffer) {
    size_t length = inputBuffer.length();
    MMBuffer tmp(length);

    auto input = (unsigned char *) inputBuffer.getPtr();
    auto output = (unsigned char *) tmp.getPtr();
    crypter.decrypt(input, output, length);

    inputBuffer = std::move(tmp);
}

void MMKV::loadFromFile() {
    m_metaInfo.read(m_metaFile.getMemory());

    m_fd = CreateFile(m_path.c_str(), GENERIC_READ | GENERIC_WRITE,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL, nullptr);

    if (m_fd == INVALID_HANDLE_VALUE) {
        MMKVError("fail to open:%ws, %d", m_path.c_str(), GetLastError());
    } else {
        m_size = 0;
        getfilesize(m_fd, m_size);
        // round up to (n * pagesize)
        if (m_size < DEFAULT_MMAP_SIZE || (m_size % DEFAULT_MMAP_SIZE != 0)) {
            size_t oldSize = m_size;
            m_size = ((m_size / DEFAULT_MMAP_SIZE) + 1) * DEFAULT_MMAP_SIZE;
            if (!ftruncate(m_fd, m_size)) {
                MMKVError("fail to truncate [%s] to size %zu, %d", m_mmapID.c_str(), m_size,
                          GetLastError());
                m_size = oldSize;
            }
            zeroFillFile(m_fd, oldSize, m_size - oldSize);
        }
        m_fileMapping = CreateFileMapping(m_fd, nullptr, PAGE_READWRITE, 0, 0, nullptr);
        if (!m_fileMapping) {
            MMKVError("fail to CreateFileMapping [%s], %d", m_mmapID.c_str(), GetLastError());
        } else {
            m_ptr = static_cast<char *>(MapViewOfFile(m_fileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0));
            if (!m_ptr) {
                MMKVError("fail to MapViewOfFile [%s], %d", m_mmapID.c_str(), GetLastError());
            } else {
                memcpy(&m_actualSize, m_ptr, Fixed32Size);
                MMKVInfo("loading [%s] with %zu size in total, file size is %zu", m_mmapID.c_str(),
                         m_actualSize, m_size);
                bool loadFromFile = false, needFullWriteback = false;
                if (m_actualSize > 0) {
                    if (m_actualSize < m_size && m_actualSize + Fixed32Size <= m_size) {
                        if (checkFileCRCValid()) {
                            loadFromFile = true;
                        } else {
                            auto strategic = onMMKVCRCCheckFail(m_mmapID);
                            if (strategic == OnErrorRecover) {
                                loadFromFile = true;
                                needFullWriteback = true;
                            }
                        }
                    } else {
                        auto strategic = onMMKVFileLengthError(m_mmapID);
                        if (strategic == OnErrorRecover) {
                            loadFromFile = true;
                            needFullWriteback = true;
                            writeAcutalSize(m_size - Fixed32Size);
                        }
                    }
                }
                if (loadFromFile) {
                    MMKVInfo("loading [%s] with crc %u sequence %u", m_mmapID.c_str(),
                             m_metaInfo.m_crcDigest, m_metaInfo.m_sequence);
                    MMBuffer inputBuffer(m_ptr + Fixed32Size, m_actualSize, MMBufferNoCopy);
                    if (m_crypter) {
                        decryptBuffer(*m_crypter, inputBuffer);
                    }
                    m_dic.clear();
                    MiniPBCoder::decodeMap(m_dic, inputBuffer);
                    m_output = new CodedOutputData(m_ptr + Fixed32Size + m_actualSize,
                                                   m_size - Fixed32Size - m_actualSize);
                    if (needFullWriteback) {
                        fullWriteback();
                    }
                } else {
                    SCOPEDLOCK(m_exclusiveProcessLock);

                    if (m_actualSize > 0) {
                        writeAcutalSize(0);
                    }
                    m_output = new CodedOutputData(m_ptr + Fixed32Size, m_size - Fixed32Size);
                    recaculateCRCDigest();
                }
                MMKVInfo("loaded [%s] with %zu values InterProcess:%d", m_mmapID.c_str(),
                         m_dic.size(), m_isInterProcess);
            }
        }
    }

    if (!isFileValid()) {
        MMKVWarning("[%s] file not valid", m_mmapID.c_str());
    }

    m_needLoadFromFile = false;
}

// read from last m_position
void MMKV::partialLoadFromFile() {
    m_metaInfo.read(m_metaFile.getMemory());

    size_t oldActualSize = m_actualSize;
    memcpy(&m_actualSize, m_ptr, Fixed32Size);
    MMKVDebug("loading [%s] with file size %zu, oldActualSize %zu, newActualSize %zu",
              m_mmapID.c_str(), m_size, oldActualSize, m_actualSize);

    if (m_actualSize > 0) {
        if (m_actualSize < m_size && m_actualSize + Fixed32Size <= m_size) {
            if (m_actualSize > oldActualSize) {
                size_t bufferSize = m_actualSize - oldActualSize;
                MMBuffer inputBuffer(m_ptr + Fixed32Size + oldActualSize, bufferSize,
                                     MMBufferNoCopy);
                // incremental update crc digest
                m_crcDigest = (uint32_t) zlib::crc32(
                    m_crcDigest, (const uint8_t *) inputBuffer.getPtr(), inputBuffer.length());
                if (m_crcDigest == m_metaInfo.m_crcDigest) {
                    if (m_crypter) {
                        decryptBuffer(*m_crypter, inputBuffer);
                    }
                    MiniPBCoder::decodeMap(m_dic, inputBuffer, bufferSize);
                    m_output->seek(bufferSize);
                    m_hasFullWriteback = false;

                    MMKVDebug("partial loaded [%s] with %zu values", m_mmapID.c_str(),
                              m_dic.size());
                    return;
                } else {
                    MMKVError("m_crcDigest[%u] != m_metaInfo.m_crcDigest[%u]", m_crcDigest,
                              m_metaInfo.m_crcDigest);
                }
            }
        }
    }
    // something is wrong, do a full load
    clearMemoryState();
    loadFromFile();
}

void MMKV::checkLoadData() {
    if (m_needLoadFromFile) {
        SCOPEDLOCK(m_sharedProcessLock);

        m_needLoadFromFile = false;
        loadFromFile();
        return;
    }
    if (!m_isInterProcess) {
        return;
    }

    // TODO: atomic lock m_metaFile?
    MetaInfo metaInfo;
    metaInfo.read(m_metaFile.getMemory());
    if (m_metaInfo.m_sequence != metaInfo.m_sequence) {
        MMKVInfo("[%s] oldSeq %u, newSeq %u", m_mmapID.c_str(), m_metaInfo.m_sequence,
                 metaInfo.m_sequence);
        SCOPEDLOCK(m_sharedProcessLock);

        clearMemoryState();
        loadFromFile();
    } else if (m_metaInfo.m_crcDigest != metaInfo.m_crcDigest) {
        MMKVDebug("[%s] oldCrc %u, newCrc %u", m_mmapID.c_str(), m_metaInfo.m_crcDigest,
                  metaInfo.m_crcDigest);
        SCOPEDLOCK(m_sharedProcessLock);

        size_t fileSize = 0;
        getfilesize(m_fd, fileSize);
        if (m_size != fileSize) {
            MMKVInfo("file size has changed [%s] from %zu to %zu", m_mmapID.c_str(), m_size,
                     fileSize);
            clearMemoryState();
            loadFromFile();
        } else {
            partialLoadFromFile();
        }
    }
}

void MMKV::clearAll() {
    MMKVInfo("cleaning all key-values from [%s]", m_mmapID.c_str());
    SCOPEDLOCK(m_lock);
    SCOPEDLOCK(m_exclusiveProcessLock);

    if (m_needLoadFromFile) {
        removeFile(m_path);
        loadFromFile();
        return;
    }

    if (m_ptr) {
        // for truncate
        size_t size = std::min<size_t>(static_cast<size_t>(DEFAULT_MMAP_SIZE), m_size);
        memset(m_ptr, 0, size);
        if (!FlushViewOfFile(m_ptr, size)) {
            MMKVError("fail to FlushViewOfFile [%s]:%d", m_mmapID.c_str(), GetLastError());
        }
    }
    if (m_fd >= 0) {
        if (m_size != DEFAULT_MMAP_SIZE) {
            MMKVInfo("truncating [%s] from %zu to %zd", m_mmapID.c_str(), m_size,
                     DEFAULT_MMAP_SIZE);
            if (!ftruncate(m_fd, DEFAULT_MMAP_SIZE)) {
                MMKVError("fail to truncate [%s] to size %zd", m_mmapID.c_str(), DEFAULT_MMAP_SIZE);
            }
        }
    }

    clearMemoryState();
    loadFromFile();
}

void MMKV::clearMemoryState() {
    MMKVInfo("clearMemoryState [%s]", m_mmapID.c_str());
    SCOPEDLOCK(m_lock);
    if (m_needLoadFromFile) {
        return;
    }
    m_needLoadFromFile = true;

    m_dic.clear();
    m_hasFullWriteback = false;

    if (m_crypter) {
        m_crypter->reset();
    }

    if (m_output) {
        delete m_output;
    }
    m_output = nullptr;

    if (m_ptr) {
        if (!UnmapViewOfFile(m_ptr)) {
            MMKVError("fail to munmap [%s], %d", m_mmapID.c_str(), GetLastError());
        }
        m_ptr = nullptr;
    }

    if (m_fileMapping) {
        if (!CloseHandle(m_fileMapping)) {
            MMKVError("fail to CloseHandle [%s], %d", m_mmapID.c_str(), GetLastError());
        }
        m_fileMapping = nullptr;
    }

    if (m_fd != INVALID_HANDLE_VALUE) {
        if (!CloseHandle(m_fd)) {
            MMKVError("fail to close [%s], %d", m_mmapID.c_str(), GetLastError());
        }
        m_fd = INVALID_HANDLE_VALUE;
    }
    m_size = 0;
    m_actualSize = 0;
}

void MMKV::close() {
    MMKVInfo("close [%s]", m_mmapID.c_str());
    SCOPEDLOCK(g_instanceLock);
    SCOPEDLOCK(m_lock);

    auto itr = g_instanceDic->find(m_mmapID);
    if (itr != g_instanceDic->end()) {
        g_instanceDic->erase(itr);
    }
    delete this;
}

void MMKV::trim() {
    SCOPEDLOCK(m_lock);
    MMKVInfo("prepare to trim %s", m_mmapID.c_str());

    checkLoadData();

    if (m_actualSize == 0) {
        clearAll();
        return;
    } else if (m_size <= DEFAULT_MMAP_SIZE) {
        return;
    }
    SCOPEDLOCK(m_exclusiveProcessLock);

    fullWriteback();
    auto oldSize = m_size;
    while (m_size > (m_actualSize * 2)) {
        m_size /= 2;
    }
    if (oldSize == m_size) {
        MMKVInfo("there's no need to trim %s with size %zu, actualSize %zu", m_mmapID.c_str(),
                 m_size, m_actualSize);
        return;
    }

    MMKVInfo("trimming %s from %zu to %zu", m_mmapID.c_str(), oldSize, m_size);

    if (!ftruncate(m_fd, m_size)) {
        MMKVError("fail to truncate [%s] to size %zu", m_mmapID.c_str(), m_size);
        m_size = oldSize;
        return;
    }
    if (!UnmapViewOfFile(m_ptr)) {
        MMKVError("fail to munmap [%s], %d", m_mmapID.c_str(), GetLastError());
    }
    m_ptr = nullptr;

    CloseHandle(m_fileMapping);
    m_fileMapping = CreateFileMapping(m_fd, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (!m_fileMapping) {
        MMKVError("fail to CreateFileMapping [%s], %d", m_mmapID.c_str(), GetLastError());
    } else {
        m_ptr = (char *) MapViewOfFile(m_fileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (!m_ptr) {
            MMKVError("fail to mmap [%s], %d", m_mmapID.c_str(), GetLastError());
        }
    }

    delete m_output;
    m_output = new CodedOutputData(m_ptr + pbFixed32Size(0), m_size - pbFixed32Size(0));
    m_output->seek(m_actualSize);

    MMKVInfo("finish trim %s from to %zu", m_mmapID.c_str(), m_size);
}

// since we use append mode, when -[setData: forKey:] many times, space may not be enough
// try a full rewrite to make space
bool MMKV::ensureMemorySize(size_t newSize) {
    if (!isFileValid()) {
        MMKVWarning("[%s] file not valid", m_mmapID.c_str());
        return false;
    }

    if (newSize >= m_output->spaceLeft()) {
        // try a full rewrite to make space
        static const int offset = pbFixed32Size(0);
        MMBuffer data = MiniPBCoder::encodeDataWithObject(m_dic);
        size_t lenNeeded = data.length() + offset + newSize;
        size_t avgItemSize = lenNeeded / std::max<size_t>(1, m_dic.size());
        size_t futureUsage = avgItemSize * std::max<size_t>(8, (m_dic.size() + 1) / 2);
        // 1. no space for a full rewrite, double it
        // 2. or space is not large enough for future usage, double it to avoid frequently full rewrite
        if (lenNeeded >= m_size || (lenNeeded + futureUsage) >= m_size) {
            size_t oldSize = m_size;
            do {
                m_size *= 2;
            } while (lenNeeded + futureUsage >= m_size);
            MMKVInfo(
                "extending [%s] file size from %zu to %zu, incoming size:%zu, future usage:%zu",
                m_mmapID.c_str(), oldSize, m_size, newSize, futureUsage);

            // if we can't extend size, rollback to old state
            if (!ftruncate(m_fd, m_size)) {
                MMKVError("fail to truncate [%s] to size %zu", m_mmapID.c_str(), m_size);
                m_size = oldSize;
                return false;
            }
            if (!zeroFillFile(m_fd, oldSize, m_size - oldSize)) {
                MMKVError("fail to zeroFile [%s] to size %zu", m_mmapID.c_str(), m_size);
                m_size = oldSize;
                return false;
            }

            if (!UnmapViewOfFile(m_ptr)) {
                MMKVError("fail to munmap [%s], %d", m_mmapID.c_str(), GetLastError());
            }
            m_ptr = nullptr;

            CloseHandle(m_fileMapping);
            m_fileMapping = CreateFileMapping(m_fd, nullptr, PAGE_READWRITE, 0, 0, nullptr);
            if (!m_fileMapping) {
                MMKVError("fail to CreateFileMapping [%s], %d", m_mmapID.c_str(), GetLastError());
            } else {
                m_ptr = (char *) MapViewOfFile(m_fileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
                if (!m_ptr) {
                    MMKVError("fail to mmap [%s], %d", m_mmapID.c_str(), GetLastError());
                }
            }

            // check if we fail to make more space
            if (!isFileValid()) {
                MMKVWarning("[%s] file not valid", m_mmapID.c_str());
                return false;
            }
        }
        if (m_crypter) {
            m_crypter->reset();
            auto ptr = (unsigned char *) data.getPtr();
            m_crypter->encrypt(ptr, ptr, data.length());
        }

        writeAcutalSize(data.length());

        delete m_output;
        m_output = new CodedOutputData(m_ptr + offset, m_size - offset);
        m_output->writeRawData(data);
        recaculateCRCDigest();
        m_hasFullWriteback = true;
    }
    return true;
}

void MMKV::writeAcutalSize(size_t actualSize) {
    assert(m_ptr != nullptr);

    memcpy(m_ptr, &actualSize, Fixed32Size);
    m_actualSize = actualSize;
}

const MMBuffer &MMKV::getDataForKey(const std::string &key) {
    checkLoadData();
    auto itr = m_dic.find(key);
    if (itr != m_dic.end()) {
        return itr->second;
    }
    static MMBuffer nan(0);
    return nan;
}

bool MMKV::setDataForKey(MMBuffer &&data, const std::string &key) {
    if (data.length() == 0 || key.empty()) {
        return false;
    }
    SCOPEDLOCK(m_lock);
    SCOPEDLOCK(m_exclusiveProcessLock);
    checkLoadData();

    // m_dic[key] = std::move(data);
    auto itr = m_dic.find(key);
    if (itr == m_dic.end()) {
        itr = m_dic.emplace(key, std::move(data)).first;
    } else {
        itr->second = std::move(data);
    }
    m_hasFullWriteback = false;

    return appendDataWithKey(itr->second, key);
}

bool MMKV::removeDataForKey(const std::string &key) {
    if (key.empty()) {
        return false;
    }

    auto deleteCount = m_dic.erase(key);
    if (deleteCount > 0) {
        m_hasFullWriteback = false;
        static MMBuffer nan(0);
        return appendDataWithKey(nan, key);
    }

    return false;
}

bool MMKV::appendDataWithKey(const MMBuffer &data, const std::string &key) {
    size_t keyLength = key.length();
    // size needed to encode the key
    size_t size = keyLength + pbRawVarint32Size((int32_t) keyLength);
    // size needed to encode the value
    size += data.length() + pbRawVarint32Size((int32_t) data.length());

    SCOPEDLOCK(m_exclusiveProcessLock);

    bool hasEnoughSize = ensureMemorySize(size);

    if (!hasEnoughSize || !isFileValid()) {
        return false;
    }
    if (m_actualSize == 0) {
        auto allData = MiniPBCoder::encodeDataWithObject(m_dic);
        if (allData.length() > 0) {
            if (m_crypter) {
                m_crypter->reset();
                auto ptr = (unsigned char *) allData.getPtr();
                m_crypter->encrypt(ptr, ptr, allData.length());
            }
            writeAcutalSize(allData.length());
            m_output->writeRawData(allData); // note: don't write size of data
            recaculateCRCDigest();
            return true;
        }
        return false;
    } else {
        writeAcutalSize(m_actualSize + size);
        m_output->writeString(key);
        m_output->writeData(data); // note: write size of data

        auto ptr = (uint8_t *) m_ptr + Fixed32Size + m_actualSize - size;
        if (m_crypter) {
            m_crypter->encrypt(ptr, ptr, size);
        }
        updateCRCDigest(ptr, size, KeepSequence);

        return true;
    }
}

bool MMKV::fullWriteback() {
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

    if (m_dic.empty()) {
        clearAll();
        return true;
    }

    auto allData = MiniPBCoder::encodeDataWithObject(m_dic);
    SCOPEDLOCK(m_exclusiveProcessLock);
    if (allData.length() > 0) {
        if (allData.length() + Fixed32Size <= m_size) {
            if (m_crypter) {
                m_crypter->reset();
                auto ptr = (unsigned char *) allData.getPtr();
                m_crypter->encrypt(ptr, ptr, allData.length());
            }
            writeAcutalSize(allData.length());
            delete m_output;
            m_output = new CodedOutputData(m_ptr + Fixed32Size, m_size - Fixed32Size);
            m_output->writeRawData(allData); // note: don't write size of data
            recaculateCRCDigest();
            m_hasFullWriteback = true;
            return true;
        } else {
            // ensureMemorySize will extend file & full rewrite, no need to write back again
            return ensureMemorySize(allData.length() + Fixed32Size - m_size);
        }
    }
    return false;
}

bool MMKV::reKey(const std::string &cryptKey) {
    SCOPEDLOCK(m_lock);
    checkLoadData();

    if (m_crypter) {
        if (cryptKey.length() > 0) {
            string oldKey = this->cryptKey();
            if (cryptKey == oldKey) {
                return true;
            } else {
                // change encryption key
                MMKVInfo("reKey with new aes key");
                delete m_crypter;
                auto ptr = (const unsigned char *) cryptKey.data();
                m_crypter = new AESCrypt(ptr, cryptKey.length());
                return fullWriteback();
            }
        } else {
            // decryption to plain text
            MMKVInfo("reKey with no aes key");
            delete m_crypter;
            m_crypter = nullptr;
            return fullWriteback();
        }
    } else {
        if (cryptKey.length() > 0) {
            // transform plain text to encrypted text
            MMKVInfo("reKey with aes key");
            auto ptr = (const unsigned char *) cryptKey.data();
            m_crypter = new AESCrypt(ptr, cryptKey.length());
            return fullWriteback();
        } else {
            return true;
        }
    }
    return false;
}

void MMKV::checkReSetCryptKey(const std::string *cryptKey) {
    SCOPEDLOCK(m_lock);

    if (m_crypter) {
        if (cryptKey) {
            string oldKey = this->cryptKey();
            if (oldKey != *cryptKey) {
                MMKVInfo("setting new aes key");
                delete m_crypter;
                auto ptr = (const unsigned char *) cryptKey->data();
                m_crypter = new AESCrypt(ptr, cryptKey->length());

                checkLoadData();
            } else {
                // nothing to do
            }
        } else {
            MMKVInfo("reset aes key");
            delete m_crypter;
            m_crypter = nullptr;

            checkLoadData();
        }
    } else {
        if (cryptKey) {
            MMKVInfo("setting new aes key");
            auto ptr = (const unsigned char *) cryptKey->data();
            m_crypter = new AESCrypt(ptr, cryptKey->length());

            checkLoadData();
        } else {
            // nothing to do
        }
    }
}

void MMKV::checkReSetCryptKey(int fd, int metaFD, std::string *cryptKey) {
    SCOPEDLOCK(m_lock);

    checkReSetCryptKey(cryptKey);
}

bool MMKV::isFileValid() {
    if (m_fd != INVALID_HANDLE_VALUE && m_fileMapping && m_size > 0 && m_output && m_ptr) {
        return true;
    }
    return false;
}

// crc

// assuming m_ptr & m_size is set
bool MMKV::checkFileCRCValid() {
    if (m_ptr) {
        constexpr int offset = pbFixed32Size(0);
        m_crcDigest = (uint32_t) zlib::crc32(0, (const uint8_t *) m_ptr + offset, m_actualSize);
        m_metaInfo.read(m_metaFile.getMemory());
        if (m_crcDigest == m_metaInfo.m_crcDigest) {
            return true;
        }
        MMKVError("check crc [%s] fail, crc32:%u, m_crcDigest:%u", m_mmapID.c_str(),
                  m_metaInfo.m_crcDigest, m_crcDigest);
    }
    return false;
}

void MMKV::recaculateCRCDigest() {
    if (m_ptr) {
        m_crcDigest = 0;
        constexpr int offset = pbFixed32Size(0);
        updateCRCDigest((const uint8_t *) m_ptr + offset, m_actualSize, IncreaseSequence);
    }
}

void MMKV::updateCRCDigest(const uint8_t *ptr, size_t length, bool increaseSequence) {
    if (!ptr) {
        return;
    }
    m_crcDigest = (uint32_t) zlib::crc32(m_crcDigest, ptr, length);

    void *crcPtr = m_metaFile.getMemory();
    if (!crcPtr) {
        return;
    }

    m_metaInfo.m_crcDigest = m_crcDigest;
    if (increaseSequence) {
        m_metaInfo.m_sequence++;
    }
    if (m_metaInfo.m_version == 0) {
        m_metaInfo.m_version = 1;
    }
    m_metaInfo.write(crcPtr);
}

// set & get

bool MMKV::set(const char *value, const std::string &key) {
    if (!value) {
        return false;
    }
    return set(string(value), key);
}

bool MMKV::set(const std::string &value, const std::string &key) {
    if (key.empty()) {
        return false;
    }
    auto data = MiniPBCoder::encodeDataWithObject(value);
    return setDataForKey(std::move(data), key);
}

bool MMKV::set(const MMBuffer &value, const std::string &key) {
    if (key.empty()) {
        return false;
    }
    auto data = MiniPBCoder::encodeDataWithObject(value);
    return setDataForKey(std::move(data), key);
}

bool MMKV::set(bool value, const std::string &key) {
    if (key.empty()) {
        return false;
    }
    size_t size = pbBoolSize(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeBool(value);

    return setDataForKey(std::move(data), key);
}

bool MMKV::set(int32_t value, const std::string &key) {
    if (key.empty()) {
        return false;
    }
    size_t size = pbInt32Size(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeInt32(value);

    return setDataForKey(std::move(data), key);
}

bool MMKV::set(uint32_t value, const std::string &key) {
    if (key.empty()) {
        return false;
    }
    size_t size = pbUInt32Size(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeUInt32(value);

    return setDataForKey(std::move(data), key);
}

bool MMKV::set(int64_t value, const std::string &key) {
    if (key.empty()) {
        return false;
    }
    size_t size = pbInt64Size(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeInt64(value);

    return setDataForKey(std::move(data), key);
}

bool MMKV::set(uint64_t value, const std::string &key) {
    if (key.empty()) {
        return false;
    }
    size_t size = pbUInt64Size(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeUInt64(value);

    return setDataForKey(std::move(data), key);
}

bool MMKV::set(float value, const std::string &key) {
    if (key.empty()) {
        return false;
    }
    size_t size = pbFloatSize(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeFloat(value);

    return setDataForKey(std::move(data), key);
}

bool MMKV::set(double value, const std::string &key) {
    if (key.empty()) {
        return false;
    }
    size_t size = pbDoubleSize(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeDouble(value);

    return setDataForKey(std::move(data), key);
}

bool MMKV::set(const std::vector<std::string> &v, const std::string &key) {
    if (key.empty()) {
        return false;
    }
    auto data = MiniPBCoder::encodeDataWithObject(v);
    return setDataForKey(std::move(data), key);
}

bool MMKV::getString(const std::string &key, std::string &result) {
    if (key.empty()) {
        return false;
    }
    SCOPEDLOCK(m_lock);
    auto &data = getDataForKey(key);
    if (data.length() > 0) {
        result = MiniPBCoder::decodeString(data);
        return true;
    }
    return false;
}

MMBuffer MMKV::getBytes(const std::string &key) {
    if (key.empty()) {
        return MMBuffer(0);
    }
    SCOPEDLOCK(m_lock);
    auto &data = getDataForKey(key);
    if (data.length() > 0) {
        return MiniPBCoder::decodeBytes(data);
    }
    return MMBuffer(0);
}

bool MMKV::getBool(const std::string &key, bool defaultValue) {
    if (key.empty()) {
        return defaultValue;
    }
    SCOPEDLOCK(m_lock);
    auto &data = getDataForKey(key);
    if (data.length() > 0) {
        CodedInputData input(data.getPtr(), data.length());
        return input.readBool();
    }
    return defaultValue;
}

int32_t MMKV::getInt32(const std::string &key, int32_t defaultValue) {
    if (key.empty()) {
        return defaultValue;
    }
    SCOPEDLOCK(m_lock);
    auto &data = getDataForKey(key);
    if (data.length() > 0) {
        CodedInputData input(data.getPtr(), data.length());
        return input.readInt32();
    }
    return defaultValue;
}

uint32_t MMKV::getUInt32(const std::string &key, uint32_t defaultValue) {
    if (key.empty()) {
        return defaultValue;
    }
    SCOPEDLOCK(m_lock);
    auto &data = getDataForKey(key);
    if (data.length() > 0) {
        CodedInputData input(data.getPtr(), data.length());
        return input.readUInt32();
    }
    return defaultValue;
}

int64_t MMKV::getInt64(const std::string &key, int64_t defaultValue) {
    if (key.empty()) {
        return defaultValue;
    }
    SCOPEDLOCK(m_lock);
    auto &data = getDataForKey(key);
    if (data.length() > 0) {
        CodedInputData input(data.getPtr(), data.length());
        return input.readInt64();
    }
    return defaultValue;
}

uint64_t MMKV::getUInt64(const std::string &key, uint64_t defaultValue) {
    if (key.empty()) {
        return defaultValue;
    }
    SCOPEDLOCK(m_lock);
    auto &data = getDataForKey(key);
    if (data.length() > 0) {
        CodedInputData input(data.getPtr(), data.length());
        return input.readUInt64();
    }
    return defaultValue;
}

float MMKV::getFloat(const std::string &key, float defaultValue) {
    if (key.empty()) {
        return defaultValue;
    }
    SCOPEDLOCK(m_lock);
    auto &data = getDataForKey(key);
    if (data.length() > 0) {
        CodedInputData input(data.getPtr(), data.length());
        return input.readFloat();
    }
    return defaultValue;
}

double MMKV::getDouble(const std::string &key, double defaultValue) {
    if (key.empty()) {
        return defaultValue;
    }
    SCOPEDLOCK(m_lock);
    auto &data = getDataForKey(key);
    if (data.length() > 0) {
        CodedInputData input(data.getPtr(), data.length());
        return input.readDouble();
    }
    return defaultValue;
}

bool MMKV::getVector(const std::string &key, std::vector<std::string> &result) {
    if (key.empty()) {
        return false;
    }
    SCOPEDLOCK(m_lock);
    auto &data = getDataForKey(key);
    if (data.length() > 0) {
        result = MiniPBCoder::decodeSet(data);
        return true;
    }
    return false;
}

// enumerate

bool MMKV::containsKey(const std::string &key) {
    SCOPEDLOCK(m_lock);
    checkLoadData();
    return m_dic.find(key) != m_dic.end();
}

size_t MMKV::count() {
    SCOPEDLOCK(m_lock);
    checkLoadData();
    return m_dic.size();
}

size_t MMKV::totalSize() {
    SCOPEDLOCK(m_lock);
    checkLoadData();
    return m_size;
}

std::vector<std::string> MMKV::allKeys() {
    SCOPEDLOCK(m_lock);
    checkLoadData();

    vector<string> keys;
    for (const auto &itr : m_dic) {
        keys.push_back(itr.first);
    };
    return keys;
}

void MMKV::removeValueForKey(const std::string &key) {
    if (key.empty()) {
        return;
    }
    SCOPEDLOCK(m_lock);
    SCOPEDLOCK(m_exclusiveProcessLock);
    checkLoadData();

    removeDataForKey(key);
}

void MMKV::removeValuesForKeys(const std::vector<std::string> &arrKeys) {
    if (arrKeys.empty()) {
        return;
    }
    if (arrKeys.size() == 1) {
        return removeValueForKey(arrKeys[0]);
    }

    SCOPEDLOCK(m_lock);
    SCOPEDLOCK(m_exclusiveProcessLock);
    checkLoadData();
    for (const auto &key : arrKeys) {
        m_dic.erase(key);
    }
    m_hasFullWriteback = false;

    fullWriteback();
}

// file

void MMKV::sync() {
    SCOPEDLOCK(m_lock);
    if (m_needLoadFromFile || !isFileValid()) {
        return;
    }
    SCOPEDLOCK(m_exclusiveProcessLock);
    if (!FlushViewOfFile(m_ptr, m_size)) {
        MMKVError("fail to FlushViewOfFile [%s]:%d", m_mmapID.c_str(), GetLastError());
    }
}

bool MMKV::isFileValid(const std::string &mmapID) {
    auto kvPath = mappedKVPathWithID(mmapID, MMKV_SINGLE_PROCESS);
    if (!isFileExist(kvPath)) {
        return true;
    }

    auto crcPath = crcPathWithID(mmapID, MMKV_SINGLE_PROCESS);
    if (!isFileExist(crcPath)) {
        return false;
    }

    uint32_t crcFile = 0;
    MMBuffer *data = readWholeFile(crcPath);
    if (data) {
        MetaInfo metaInfo;
        metaInfo.read(data->getPtr());
        crcFile = metaInfo.m_crcDigest;
        delete data;
    } else {
        return false;
    }

    const int offset = pbFixed32Size(0);
    size_t actualSize = 0;
    MMBuffer *fileData = readWholeFile(kvPath);
    if (fileData) {
        actualSize = CodedInputData(fileData->getPtr(), fileData->length()).readFixed32();
        if (actualSize > fileData->length() - offset) {
            delete fileData;
            return false;
        }

        auto ptr = (const uint8_t *) fileData->getPtr() + offset;
        auto crcDigest = (uint32_t) zlib::crc32(0, ptr, actualSize);

        delete fileData;
        return crcFile == crcDigest;
    }
    return false;
}

void MMKV::regiserErrorHandler(ErrorHandler handler) {
    SCOPEDLOCK(g_instanceLock);
    g_errorHandler = handler;
}

void MMKV::unRegisetErrorHandler() {
    SCOPEDLOCK(g_instanceLock);
    g_errorHandler = nullptr;
}

void MMKV::regiserLogHandler(LogHandler handler) {
    SCOPEDLOCK(g_instanceLock);
    g_logHandler = handler;
}

void MMKV::unRegisetLogHandler() {
    SCOPEDLOCK(g_instanceLock);
    g_logHandler = nullptr;
}

void MMKV::setLogLevel(MMKVLogLevel level) {
    SCOPEDLOCK(g_instanceLock);
    g_currentLogLevel = level;
}

static void mkSpecialCharacterFileDirectory() {
    wstring path = g_rootDir + L"/" + SPECIAL_CHARACTER_DIRECTORY_NAME;
    mkPath(path);
}

static string md5(const string &value) {
    unsigned char md[MD5_DIGEST_LENGTH] = {0};
    char tmp[3] = {0}, buf[33] = {0};
    openssl::MD5((const unsigned char *) value.c_str(), value.size(), md);
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf_s(tmp, sizeof(tmp), "%2.2x", md[i]);
        strcat_s(buf, sizeof(tmp), tmp);
    }
    return buf;
}

static wstring encodeFilePath(const string &mmapID) {
    const char *specialCharacters = "\\/:*?\"<>|";
    string encodedID;
    bool hasSpecialCharacter = false;
    for (size_t index = 0; index < mmapID.size(); index++) {
        if (strchr(specialCharacters, mmapID[index]) != NULL) {
            encodedID = md5(mmapID);
            hasSpecialCharacter = true;
            break;
        }
    }
    if (hasSpecialCharacter) {
        static volatile ThreadOnceToken onceToken = ThreadOnceUninitialized;
        ThreadLock::ThreadOnce(onceToken, mkSpecialCharacterFileDirectory);
        return wstring(SPECIAL_CHARACTER_DIRECTORY_NAME) + L"/" + string2wstring(encodedID);
    } else {
        return string2wstring(mmapID);
    }
}

static wstring mappedKVPathWithID(const string &mmapID, MMKVMode mode) {
    return g_rootDir + L"\\" + encodeFilePath(mmapID);
}

static wstring crcPathWithID(const string &mmapID, MMKVMode mode) {
    return g_rootDir + L"\\" + encodeFilePath(mmapID) + L".crc";
}

static MMKVRecoverStrategic onMMKVCRCCheckFail(const std::string &mmapID) {
    if (g_errorHandler) {
        return g_errorHandler(mmapID, MMKVErrorType::MMKVCRCCheckFail);
    }
    return OnErrorDiscard;
}

static MMKVRecoverStrategic onMMKVFileLengthError(const std::string &mmapID) {
    if (g_errorHandler) {
        return g_errorHandler(mmapID, MMKVErrorType::MMKVFileLength);
    }
    return OnErrorDiscard;
}
