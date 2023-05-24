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

#include "CodedInputData.h"
#include "CodedOutputData.h"
#include "InterProcessLock.h"
#include "KeyValueHolder.h"
#include "MMBuffer.h"
#include "MMKVLog.h"
#include "MMKVMetaInfo.hpp"
#include "MMKV_IO.h"
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
#include <cstdio>
#include <cstring>
#include <unordered_set>
//#include <unistd.h>
#include <cassert>

#if defined(__aarch64__) && defined(__linux)
#    include <asm/hwcap.h>
#    include <sys/auxv.h>
#endif

#ifdef MMKV_APPLE
#    if __has_feature(objc_arc)
#        error This file must be compiled with MRC. Use -fno-objc-arc flag.
#    endif
#endif // MMKV_APPLE

using namespace std;
using namespace mmkv;

unordered_map<string, MMKV *> *g_instanceDic;
ThreadLock *g_instanceLock;
MMKVPath_t g_rootDir;
static mmkv::ErrorHandler g_errorHandler;
size_t mmkv::DEFAULT_MMAP_SIZE;

#ifndef MMKV_WIN32
constexpr auto SPECIAL_CHARACTER_DIRECTORY_NAME = "specialCharacter";
constexpr auto CRC_SUFFIX = ".crc";
#else
constexpr auto SPECIAL_CHARACTER_DIRECTORY_NAME = L"specialCharacter";
constexpr auto CRC_SUFFIX = L".crc";
#endif

MMKV_NAMESPACE_BEGIN

static MMKVPath_t encodeFilePath(const string &mmapID, const MMKVPath_t &rootDir);
bool endsWith(const MMKVPath_t &str, const MMKVPath_t &suffix);
MMKVPath_t filename(const MMKVPath_t &path);

#ifndef MMKV_ANDROID
MMKV::MMKV(const string &mmapID, MMKVMode mode, string *cryptKey, MMKVPath_t *rootPath)
    : m_mmapID(mmapID)
    , m_path(mappedKVPathWithID(m_mmapID, mode, rootPath))
    , m_crcPath(crcPathWithID(m_mmapID, mode, rootPath))
    , m_dic(nullptr)
    , m_dicCrypt(nullptr)
    , m_file(new MemoryFile(m_path))
    , m_metaFile(new MemoryFile(m_crcPath))
    , m_metaInfo(new MMKVMetaInfo())
    , m_crypter(nullptr)
    , m_lock(new ThreadLock())
    , m_fileLock(new FileLock(m_metaFile->getFd()))
    , m_sharedProcessLock(new InterProcessLock(m_fileLock, SharedLockType))
    , m_exclusiveProcessLock(new InterProcessLock(m_fileLock, ExclusiveLockType))
    , m_isInterProcess((mode & MMKV_MULTI_PROCESS) != 0) {
    m_actualSize = 0;
    m_output = nullptr;

#    ifndef MMKV_DISABLE_CRYPT
    if (cryptKey && cryptKey->length() > 0) {
        m_dicCrypt = new MMKVMapCrypt();
        m_crypter = new AESCrypt(cryptKey->data(), cryptKey->length());
    } else {
        m_dic = new MMKVMap();
    }
#    else
    m_dic = new MMKVMap();
#    endif

    m_needLoadFromFile = true;
    m_hasFullWriteback = false;

    m_crcDigest = 0;

    m_lock->initialize();
    m_sharedProcessLock->m_enable = m_isInterProcess;
    m_exclusiveProcessLock->m_enable = m_isInterProcess;

}
#endif

MMKV::~MMKV() {
    clearMemoryCache();

    delete m_dic;
#ifndef MMKV_DISABLE_CRYPT
    delete m_dicCrypt;
    delete m_crypter;
#endif
    delete m_file;
    delete m_metaFile;
    delete m_metaInfo;
    delete m_lock;
    delete m_fileLock;
    delete m_sharedProcessLock;
    delete m_exclusiveProcessLock;
#ifdef MMKV_ANDROID
    delete m_fileModeLock;
    delete m_sharedProcessModeLock;
    delete m_exclusiveProcessModeLock;
#endif

    MMKVInfo("destruct [%s]", m_mmapID.c_str());
}

MMKV *MMKV::defaultMMKV(MMKVMode mode, string *cryptKey) {
#ifndef MMKV_ANDROID
    return mmkvWithID(DEFAULT_MMAP_ID, mode, cryptKey);
#else
    return mmkvWithID(DEFAULT_MMAP_ID, DEFAULT_MMAP_SIZE, mode, cryptKey);
#endif
}

void initialize() {
    g_instanceDic = new unordered_map<string, MMKV *>;
    g_instanceLock = new ThreadLock();
    g_instanceLock->initialize();

    mmkv::DEFAULT_MMAP_SIZE = mmkv::getPageSize();
    MMKVInfo("version %s, page size %d, arch %s", MMKV_VERSION, DEFAULT_MMAP_SIZE, MMKV_ABI);

    // get CPU status of ARMv8 extensions (CRC32, AES)
#if defined(__aarch64__) && defined(__linux__)
    auto hwcaps = getauxval(AT_HWCAP);
#    ifndef MMKV_DISABLE_CRYPT
    if (hwcaps & HWCAP_AES) {
        openssl::AES_set_encrypt_key = openssl_aes_armv8_set_encrypt_key;
        openssl::AES_set_decrypt_key = openssl_aes_armv8_set_decrypt_key;
        openssl::AES_encrypt = openssl_aes_armv8_encrypt;
        openssl::AES_decrypt = openssl_aes_armv8_decrypt;
        MMKVInfo("armv8 AES instructions is supported");
    } else {
        MMKVInfo("armv8 AES instructions is not supported");
    }
#    endif // MMKV_DISABLE_CRYPT
#    ifdef MMKV_USE_ARMV8_CRC32
    if (hwcaps & HWCAP_CRC32) {
        CRC32 = mmkv::armv8_crc32;
        MMKVInfo("armv8 CRC32 instructions is supported");
    } else {
        MMKVInfo("armv8 CRC32 instructions is not supported");
    }
#    endif // MMKV_USE_ARMV8_CRC32
#endif     // __aarch64__ && defined(__linux__)

#if defined(MMKV_DEBUG) && !defined(MMKV_DISABLE_CRYPT)
    // AESCrypt::testAESCrypt();
    // KeyValueHolderCrypt::testAESToMMBuffer();
#endif
}

ThreadOnceToken_t once_control = ThreadOnceUninitialized;

void MMKV::initializeMMKV(const MMKVPath_t &rootDir, MMKVLogLevel logLevel, mmkv::LogHandler handler) {
    g_currentLogLevel = logLevel;
    g_logHandler = handler;

    ThreadLock::ThreadOnce(&once_control, initialize);

    g_rootDir = rootDir;
    mkPath(g_rootDir);

    MMKVInfo("root dir: " MMKV_PATH_FORMAT, g_rootDir.c_str());
}

const MMKVPath_t &MMKV::getRootDir() {
    return g_rootDir;
}

#ifndef MMKV_ANDROID
MMKV *MMKV::mmkvWithID(const string &mmapID, MMKVMode mode, string *cryptKey, MMKVPath_t *rootPath) {

    if (mmapID.empty()) {
        return nullptr;
    }
    SCOPED_LOCK(g_instanceLock);

    auto mmapKey = mmapedKVKey(mmapID, rootPath);
    auto itr = g_instanceDic->find(mmapKey);
    if (itr != g_instanceDic->end()) {
        MMKV *kv = itr->second;
        return kv;
    }

    if (rootPath) {
        MMKVPath_t specialPath = (*rootPath) + MMKV_PATH_SLASH + SPECIAL_CHARACTER_DIRECTORY_NAME;
        if (!isFileExist(specialPath)) {
            mkPath(specialPath);
        }
        MMKVInfo("prepare to load %s (id %s) from rootPath %s", mmapID.c_str(), mmapKey.c_str(), rootPath->c_str());
    }

    auto kv = new MMKV(mmapID, mode, cryptKey, rootPath);
    kv->m_mmapKey = mmapKey;
    (*g_instanceDic)[mmapKey] = kv;
    return kv;
}
#endif

void MMKV::onExit() {
    SCOPED_LOCK(g_instanceLock);

    for (auto &pair : *g_instanceDic) {
        MMKV *kv = pair.second;
        kv->sync();
        kv->clearMemoryCache();
        delete kv;
        pair.second = nullptr;
    }

    delete g_instanceDic;
    g_instanceDic = nullptr;
}

const string &MMKV::mmapID() const {
    return m_mmapID;
}

mmkv::ContentChangeHandler g_contentChangeHandler = nullptr;

void MMKV::notifyContentChanged() {
    if (g_contentChangeHandler) {
        g_contentChangeHandler(m_mmapID);
    }
}

void MMKV::checkContentChanged() {
    SCOPED_LOCK(m_lock);
    checkLoadData();
}

void MMKV::registerContentChangeHandler(mmkv::ContentChangeHandler handler) {
    g_contentChangeHandler = handler;
}

void MMKV::unRegisterContentChangeHandler() {
    g_contentChangeHandler = nullptr;
}

void MMKV::clearMemoryCache() {
    SCOPED_LOCK(m_lock);
    if (m_needLoadFromFile) {
        return;
    }
    MMKVInfo("clearMemoryCache [%s]", m_mmapID.c_str());
    m_needLoadFromFile = true;
    m_hasFullWriteback = false;

    clearDictionary(m_dic);
#ifndef MMKV_DISABLE_CRYPT
    clearDictionary(m_dicCrypt);
    if (m_crypter) {
        if (m_metaInfo->m_version >= MMKVVersionRandomIV) {
            m_crypter->resetIV(m_metaInfo->m_vector, sizeof(m_metaInfo->m_vector));
        } else {
            m_crypter->resetIV();
        }
    }
#endif

    delete m_output;
    m_output = nullptr;

    m_file->clearMemoryCache();
    m_metaFile->clearMemoryCache();
    m_actualSize = 0;
    m_metaInfo->m_crcDigest = 0;
}

void MMKV::close() {
    MMKVInfo("close [%s]", m_mmapID.c_str());
    SCOPED_LOCK(g_instanceLock);
    m_lock->lock();

#ifndef MMKV_ANDROID
    auto itr = g_instanceDic->find(m_mmapKey);
#else
    auto itr = g_instanceDic->find(m_mmapID);
#endif
    if (itr != g_instanceDic->end()) {
        g_instanceDic->erase(itr);
    }
    delete this;
}

#ifndef MMKV_DISABLE_CRYPT

string MMKV::cryptKey() const {
    SCOPED_LOCK(m_lock);

    if (m_crypter) {
        char key[AES_KEY_LEN];
        m_crypter->getKey(key);
        return {key, strnlen(key, AES_KEY_LEN)};
    }
    return "";
}

void MMKV::checkReSetCryptKey(const string *cryptKey) {
    SCOPED_LOCK(m_lock);

    if (m_crypter) {
        if (cryptKey && cryptKey->length() > 0) {
            string oldKey = this->cryptKey();
            if (oldKey != *cryptKey) {
                MMKVInfo("setting new aes key");
                delete m_crypter;
                auto ptr = cryptKey->data();
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
        if (cryptKey && cryptKey->length() > 0) {
            MMKVInfo("setting new aes key");
            auto ptr = cryptKey->data();
            m_crypter = new AESCrypt(ptr, cryptKey->length());

            checkLoadData();
        } else {
            // nothing to do
        }
    }
}

#endif // MMKV_DISABLE_CRYPT

bool MMKV::isFileValid() {
    return m_file->isFileValid();
}

// crc

// assuming m_file is valid
bool MMKV::checkFileCRCValid(size_t actualSize, uint32_t crcDigest) {
    auto ptr = (uint8_t *) m_file->getMemory();
    if (ptr) {
        m_crcDigest = (uint32_t) CRC32(0, (const uint8_t *) ptr + Fixed32Size, (uint32_t) actualSize);

        if (m_crcDigest == crcDigest) {
            return true;
        }
        MMKVError("check crc [%s] fail, crc32:%u, m_crcDigest:%u", m_mmapID.c_str(), crcDigest, m_crcDigest);
    }
    return false;
}

void MMKV::recaculateCRCDigestWithIV(const void *iv) {
    auto ptr = (const uint8_t *) m_file->getMemory();
    if (ptr) {
        m_crcDigest = 0;
        m_crcDigest = (uint32_t) CRC32(0, ptr + Fixed32Size, (uint32_t) m_actualSize);
        writeActualSize(m_actualSize, m_crcDigest, iv, IncreaseSequence);
    }
}

void MMKV::updateCRCDigest(const uint8_t *ptr, size_t length) {
    if (ptr == nullptr) {
        return;
    }
    m_crcDigest = (uint32_t) CRC32(m_crcDigest, ptr, (uint32_t) length);

    writeActualSize(m_actualSize, m_crcDigest, nullptr, KeepSequence);
}

// set & get

bool MMKV::set(bool value, MMKVKey_t key) {
    return set(value, key, m_expiredInSeconds);
}

bool MMKV::set(bool value, MMKVKey_t key, uint32_t expireDuration) {
    if (isKeyEmpty(key)) {
        return false;
    }
    size_t size = unlikely(m_enableKeyExipre) ? Fixed32Size + pbBoolSize() : pbBoolSize();
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeBool(value);
    if (unlikely(m_enableKeyExipre)) {
        auto time = (expireDuration != 0) ? getCurrentTimeInSecond() + expireDuration : 0;
        output.writeRawLittleEndian32(UInt32ToInt32(time));
    } else {
        assert(expireDuration == 0 && "setting expire duration without calling enableAutoKeyExpire() first");
    }

    return setDataForKey(std::move(data), key);
}

bool MMKV::set(int32_t value, MMKVKey_t key) {
    return set(value, key, m_expiredInSeconds);
}

bool MMKV::set(int32_t value, MMKVKey_t key, uint32_t expireDuration) {
    if (isKeyEmpty(key)) {
        return false;
    }
    size_t size = unlikely(m_enableKeyExipre) ? Fixed32Size + pbInt32Size(value) : pbInt32Size(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeInt32(value);
    if (unlikely(m_enableKeyExipre)) {
        auto time = (expireDuration != 0) ? getCurrentTimeInSecond() + expireDuration : 0;
        output.writeRawLittleEndian32(UInt32ToInt32(time));
    } else {
        assert(expireDuration == 0 && "setting expire duration without calling enableAutoKeyExpire() first");
    }

    return setDataForKey(std::move(data), key);
}

bool MMKV::set(uint32_t value, MMKVKey_t key) {
    return set(value, key, m_expiredInSeconds);
}

bool MMKV::set(uint32_t value, MMKVKey_t key, uint32_t expireDuration) {
    if (isKeyEmpty(key)) {
        return false;
    }
    size_t size = unlikely(m_enableKeyExipre) ? Fixed32Size + pbUInt32Size(value) : pbUInt32Size(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeUInt32(value);
    if (unlikely(m_enableKeyExipre)) {
        auto time = (expireDuration != 0) ? getCurrentTimeInSecond() + expireDuration : 0;
        output.writeRawLittleEndian32(UInt32ToInt32(time));
    } else {
        assert(expireDuration == 0 && "setting expire duration without calling enableAutoKeyExpire() first");
    }

    return setDataForKey(std::move(data), key);
}

bool MMKV::set(int64_t value, MMKVKey_t key) {
    return set(value, key, m_expiredInSeconds);
}

bool MMKV::set(int64_t value, MMKVKey_t key, uint32_t expireDuration) {
    if (isKeyEmpty(key)) {
        return false;
    }
    size_t size = unlikely(m_enableKeyExipre) ? Fixed32Size + pbInt64Size(value) : pbInt64Size(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeInt64(value);
    if (unlikely(m_enableKeyExipre)) {
        auto time = (expireDuration != 0) ? getCurrentTimeInSecond() + expireDuration : 0;
        output.writeRawLittleEndian32(UInt32ToInt32(time));
    } else {
        assert(expireDuration == 0 && "setting expire duration without calling enableAutoKeyExpire() first");
    }

    return setDataForKey(std::move(data), key);
}

bool MMKV::set(uint64_t value, MMKVKey_t key) {
    return set(value, key, m_expiredInSeconds);
}

bool MMKV::set(uint64_t value, MMKVKey_t key, uint32_t expireDuration) {
    if (isKeyEmpty(key)) {
        return false;
    }
    size_t size = unlikely(m_enableKeyExipre) ? Fixed32Size + pbUInt64Size(value) : pbUInt64Size(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeUInt64(value);
    if (unlikely(m_enableKeyExipre)) {
        auto time = (expireDuration != 0) ? getCurrentTimeInSecond() + expireDuration : 0;
        output.writeRawLittleEndian32(UInt32ToInt32(time));
    } else {
        assert(expireDuration == 0 && "setting expire duration without calling enableAutoKeyExpire() first");
    }

    return setDataForKey(std::move(data), key);
}

bool MMKV::set(float value, MMKVKey_t key) {
    return set(value, key, m_expiredInSeconds);
}

bool MMKV::set(float value, MMKVKey_t key, uint32_t expireDuration) {
    if (isKeyEmpty(key)) {
        return false;
    }
    size_t size = unlikely(m_enableKeyExipre) ? Fixed32Size + pbFloatSize() : pbFloatSize();
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeFloat(value);
    if (unlikely(m_enableKeyExipre)) {
        auto time = (expireDuration != 0) ? getCurrentTimeInSecond() + expireDuration : 0;
        output.writeRawLittleEndian32(UInt32ToInt32(time));
    } else {
        assert(expireDuration == 0 && "setting expire duration without calling enableAutoKeyExpire() first");
    }

    return setDataForKey(std::move(data), key);
}

bool MMKV::set(double value, MMKVKey_t key) {
    return set(value, key, m_expiredInSeconds);
}

bool MMKV::set(double value, MMKVKey_t key, uint32_t expireDuration) {
    if (isKeyEmpty(key)) {
        return false;
    }
    size_t size = unlikely(m_enableKeyExipre) ? Fixed32Size + pbDoubleSize() : pbDoubleSize();
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeDouble(value);
    if (unlikely(m_enableKeyExipre)) {
        auto time = (expireDuration != 0) ? getCurrentTimeInSecond() + expireDuration : 0;
        output.writeRawLittleEndian32(UInt32ToInt32(time));
    } else {
        assert(expireDuration == 0 && "setting expire duration without calling enableAutoKeyExpire() first");
    }

    return setDataForKey(std::move(data), key);
}

#ifndef MMKV_APPLE

bool MMKV::set(const char *value, MMKVKey_t key) {
    return set(value, key, m_expiredInSeconds);
}

bool MMKV::set(const char *value, MMKVKey_t key, uint32_t expireDuration) {
    if (!value) {
        removeValueForKey(key);
        return true;
    }
    if (likely(!m_enableKeyExipre)) {
        return setDataForKey(MMBuffer((void *) value, strlen(value), MMBufferNoCopy), key, true);
    } else {
        MMBuffer data((void *) value, strlen(value), MMBufferNoCopy);
        if (data.length() > 0) {
            auto tmp = MMBuffer(pbMMBufferSize(data) + Fixed32Size);
            CodedOutputData output(tmp.getPtr(), tmp.length());
            output.writeData(data);
            auto time = (expireDuration != 0) ? getCurrentTimeInSecond() + expireDuration : 0;
            output.writeRawLittleEndian32(UInt32ToInt32(time));
            data = std::move(tmp);
        }
        return setDataForKey(std::move(data), key);
    }
}

bool MMKV::set(const string &value, MMKVKey_t key) {
    return set(value, key, m_expiredInSeconds);
}

bool MMKV::set(const string &value, MMKVKey_t key, uint32_t expireDuration) {
    if (isKeyEmpty(key)) {
        return false;
    }
    if (likely(!m_enableKeyExipre)) {
        return setDataForKey(MMBuffer((void *) value.data(), value.length(), MMBufferNoCopy), key, true);
    } else {
        MMBuffer data((void *) value.data(), value.length(), MMBufferNoCopy);
        if (data.length() > 0) {
            auto tmp = MMBuffer(pbMMBufferSize(data) + Fixed32Size);
            CodedOutputData output(tmp.getPtr(), tmp.length());
            output.writeData(data);
            auto time = (expireDuration != 0) ? getCurrentTimeInSecond() + expireDuration : 0;
            output.writeRawLittleEndian32(UInt32ToInt32(time));
            data = std::move(tmp);
        }
        return setDataForKey(std::move(data), key);
    }
}

bool MMKV::set(const MMBuffer &value, MMKVKey_t key) {
    return set(value, key, m_expiredInSeconds);
}

bool MMKV::set(const MMBuffer &value, MMKVKey_t key, uint32_t expireDuration) {
    if (isKeyEmpty(key)) {
        return false;
    }
    // delay write the size needed for encoding value
    // avoid memory copying
    if (likely(!m_enableKeyExipre)) {
        return setDataForKey(MMBuffer(value.getPtr(), value.length(), MMBufferNoCopy), key, true);
    } else {
        MMBuffer data(value.getPtr(), value.length(), MMBufferNoCopy);
        if (data.length() > 0) {
            auto tmp = MMBuffer(pbMMBufferSize(data) + Fixed32Size);
            CodedOutputData output(tmp.getPtr(), tmp.length());
            output.writeData(data);
            auto time = (expireDuration != 0) ? getCurrentTimeInSecond() + expireDuration : 0;
            output.writeRawLittleEndian32(UInt32ToInt32(time));
            data = std::move(tmp);
        }
        return setDataForKey(std::move(data), key);
    }
}

bool MMKV::set(const vector<string> &value, MMKVKey_t key) {
    return set(value, key, m_expiredInSeconds);
}

bool MMKV::set(const vector<string> &v, MMKVKey_t key, uint32_t expireDuration) {
    if (isKeyEmpty(key)) {
        return false;
    }
    auto data = MiniPBCoder::encodeDataWithObject(v);
    if (unlikely(m_enableKeyExipre) && data.length() > 0) {
        auto tmp = MMBuffer(data.length() + Fixed32Size);
        auto ptr = (uint8_t *)tmp.getPtr();
        memcpy(ptr, data.getPtr(), data.length());
        auto time = (expireDuration != 0) ? getCurrentTimeInSecond() + expireDuration : 0;
        memcpy(ptr + data.length(), &time, Fixed32Size);
        data = std::move(tmp);
    }
    return setDataForKey(std::move(data), key);
}

bool MMKV::getString(MMKVKey_t key, string &result) {
    if (isKeyEmpty(key)) {
        return false;
    }
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_sharedProcessLock);
    auto data = getDataForKey(key);
    if (data.length() > 0) {
        try {
            CodedInputData input(data.getPtr(), data.length());
            result = input.readString();
            return true;
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        }
    }
    return false;
}

bool MMKV::getBytes(MMKVKey_t key, mmkv::MMBuffer &result) {
    if (isKeyEmpty(key)) {
        return false;
    }
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_sharedProcessLock);
    auto data = getDataForKey(key);
    if (data.length() > 0) {
        try {
            CodedInputData input(data.getPtr(), data.length());
            result = std::move(input.readData());
            return true;
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        }
    }
    return false;
}

MMBuffer MMKV::getBytes(MMKVKey_t key) {
    if (isKeyEmpty(key)) {
        return MMBuffer();
    }
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_sharedProcessLock);
    auto data = getDataForKey(key);
    if (data.length() > 0) {
        try {
            CodedInputData input(data.getPtr(), data.length());
            return input.readData();
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        }
    }
    return MMBuffer();
}

bool MMKV::getVector(MMKVKey_t key, vector<string> &result) {
    if (isKeyEmpty(key)) {
        return false;
    }
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_sharedProcessLock);
    auto data = getDataForKey(key);
    if (data.length() > 0) {
        try {
            result = MiniPBCoder::decodeVector(data);
            return true;
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        }
    }
    return false;
}

#endif // MMKV_APPLE

bool MMKV::getBool(MMKVKey_t key, bool defaultValue, bool *hasValue) {
    if (isKeyEmpty(key)) {
        if (hasValue != nullptr) {
            *hasValue = false;
        }
        return defaultValue;
    }
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_sharedProcessLock);
    auto data = getDataForKey(key);
    if (data.length() > 0) {
        try {
            CodedInputData input(data.getPtr(), data.length());
            if (hasValue != nullptr) {
                *hasValue = true;
            }
            return input.readBool();
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        }
    }
    if (hasValue != nullptr) {
        *hasValue = false;
    }
    return defaultValue;
}

int32_t MMKV::getInt32(MMKVKey_t key, int32_t defaultValue, bool *hasValue) {
    if (isKeyEmpty(key)) {
        if (hasValue != nullptr) {
            *hasValue = false;
        }
        return defaultValue;
    }
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_sharedProcessLock);
    auto data = getDataForKey(key);
    if (data.length() > 0) {
        try {
            CodedInputData input(data.getPtr(), data.length());
            if (hasValue != nullptr) {
                *hasValue = true;
            }
            return input.readInt32();
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        }
    }
    if (hasValue != nullptr) {
        *hasValue = false;
    }
    return defaultValue;
}

uint32_t MMKV::getUInt32(MMKVKey_t key, uint32_t defaultValue, bool *hasValue) {
    if (isKeyEmpty(key)) {
        if (hasValue != nullptr) {
            *hasValue = false;
        }
        return defaultValue;
    }
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_sharedProcessLock);
    auto data = getDataForKey(key);
    if (data.length() > 0) {
        try {
            CodedInputData input(data.getPtr(), data.length());
            if (hasValue != nullptr) {
                *hasValue = true;
            }
            return input.readUInt32();
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        }
    }
    if (hasValue != nullptr) {
        *hasValue = false;
    }
    return defaultValue;
}

int64_t MMKV::getInt64(MMKVKey_t key, int64_t defaultValue, bool *hasValue) {
    if (isKeyEmpty(key)) {
        if (hasValue != nullptr) {
            *hasValue = false;
        }
        return defaultValue;
    }
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_sharedProcessLock);
    auto data = getDataForKey(key);
    if (data.length() > 0) {
        try {
            CodedInputData input(data.getPtr(), data.length());
            if (hasValue != nullptr) {
                *hasValue = true;
            }
            return input.readInt64();
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        }
    }
    if (hasValue != nullptr) {
        *hasValue = false;
    }
    return defaultValue;
}

uint64_t MMKV::getUInt64(MMKVKey_t key, uint64_t defaultValue, bool *hasValue) {
    if (isKeyEmpty(key)) {
        if (hasValue != nullptr) {
            *hasValue = false;
        }
        return defaultValue;
    }
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_sharedProcessLock);
    auto data = getDataForKey(key);
    if (data.length() > 0) {
        try {
            CodedInputData input(data.getPtr(), data.length());
            if (hasValue != nullptr) {
                *hasValue = true;
            }
            return input.readUInt64();
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        }
    }
    if (hasValue != nullptr) {
        *hasValue = false;
    }
    return defaultValue;
}

float MMKV::getFloat(MMKVKey_t key, float defaultValue, bool *hasValue) {
    if (isKeyEmpty(key)) {
        if (hasValue != nullptr) {
            *hasValue = false;
        }
        return defaultValue;
    }
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_sharedProcessLock);
    auto data = getDataForKey(key);
    if (data.length() > 0) {
        try {
            CodedInputData input(data.getPtr(), data.length());
            if (hasValue != nullptr) {
                *hasValue = true;
            }
            return input.readFloat();
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        }
    }
    if (hasValue != nullptr) {
        *hasValue = false;
    }
    return defaultValue;
}

double MMKV::getDouble(MMKVKey_t key, double defaultValue, bool *hasValue) {
    if (isKeyEmpty(key)) {
        if (hasValue != nullptr) {
            *hasValue = false;
        }
        return defaultValue;
    }
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_sharedProcessLock);
    auto data = getDataForKey(key);
    if (data.length() > 0) {
        try {
            CodedInputData input(data.getPtr(), data.length());
            if (hasValue != nullptr) {
                *hasValue = true;
            }
            return input.readDouble();
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        }
    }
    if (hasValue != nullptr) {
        *hasValue = false;
    }
    return defaultValue;
}

size_t MMKV::getValueSize(MMKVKey_t key, bool actualSize) {
    if (isKeyEmpty(key)) {
        return 0;
    }
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_sharedProcessLock);
    auto data = getDataForKey(key);
    if (actualSize) {
        try {
            CodedInputData input(data.getPtr(), data.length());
            auto length = input.readInt32();
            if (length >= 0) {
                auto s_length = static_cast<size_t>(length);
                if (pbRawVarint32Size(length) + s_length == data.length()) {
                    return s_length;
                }
            }
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        }
    }
    return data.length();
}

int32_t MMKV::writeValueToBuffer(MMKVKey_t key, void *ptr, int32_t size) {
    if (isKeyEmpty(key) || size < 0) {
        return -1;
    }
    auto s_size = static_cast<size_t>(size);

    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_sharedProcessLock);
    auto data = getDataForKey(key);
    try {
        CodedInputData input(data.getPtr(), data.length());
        auto length = input.readInt32();
        auto offset = pbRawVarint32Size(length);
        if (length >= 0) {
            auto s_length = static_cast<size_t>(length);
            if (offset + s_length == data.length()) {
                if (s_length <= s_size) {
                    memcpy(ptr, (uint8_t *) data.getPtr() + offset, s_length);
                    return length;
                }
            } else {
                if (data.length() <= s_size) {
                    memcpy(ptr, data.getPtr(), data.length());
                    return static_cast<int32_t>(data.length());
                }
            }
        }
    } catch (std::exception &exception) {
        MMKVError("%s", exception.what());
    }
    return -1;
}

// enumerate

bool MMKV::containsKey(MMKVKey_t key) {
    SCOPED_LOCK(m_lock);
    checkLoadData();

    if (likely(!m_enableKeyExipre)) {
        if (m_crypter) {
            return m_dicCrypt->find(key) != m_dicCrypt->end();
        } else {
            return m_dic->find(key) != m_dic->end();
        }
    }
    auto raw = getDataWithoutMTimeForKey(key);
    return raw.length() != 0;
}

size_t MMKV::count() {
    SCOPED_LOCK(m_lock);
    checkLoadData();
    if (m_crypter) {
        return m_dicCrypt->size();
    } else {
        return m_dic->size();
    }
}

size_t MMKV::totalSize() {
    SCOPED_LOCK(m_lock);
    checkLoadData();
    return m_file->getFileSize();
}

size_t MMKV::actualSize() {
    SCOPED_LOCK(m_lock);
    checkLoadData();
    return m_actualSize;
}

void MMKV::removeValueForKey(MMKVKey_t key) {
    if (isKeyEmpty(key)) {
        return;
    }
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_exclusiveProcessLock);
    checkLoadData();

    removeDataForKey(key);
}

#ifndef MMKV_APPLE

vector<string> MMKV::allKeys() {
    SCOPED_LOCK(m_lock);
    checkLoadData();

    vector<string> keys;
    if (m_crypter) {
        for (const auto &itr : *m_dicCrypt) {
            keys.push_back(itr.first);
        }
    } else {
        for (const auto &itr : *m_dic) {
            keys.push_back(itr.first);
        }
    }
    return keys;
}

void MMKV::removeValuesForKeys(const vector<string> &arrKeys) {
    if (arrKeys.empty()) {
        return;
    }
    if (arrKeys.size() == 1) {
        return removeValueForKey(arrKeys[0]);
    }

    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_exclusiveProcessLock);
    checkLoadData();

    size_t deleteCount = 0;
    if (m_crypter) {
        for (const auto &key : arrKeys) {
            auto itr = m_dicCrypt->find(key);
            if (itr != m_dicCrypt->end()) {
                m_dicCrypt->erase(itr);
                deleteCount++;
            }
        }
    } else {
        for (const auto &key : arrKeys) {
            auto itr = m_dic->find(key);
            if (itr != m_dic->end()) {
                m_dic->erase(itr);
                deleteCount++;
            }
        }
    }
    if (deleteCount > 0) {
        m_hasFullWriteback = false;

        fullWriteback();
    }
}

#endif // MMKV_APPLE

// file

void MMKV::sync(SyncFlag flag) {
    SCOPED_LOCK(m_lock);
    if (m_needLoadFromFile || !isFileValid()) {
        return;
    }
    SCOPED_LOCK(m_exclusiveProcessLock);

    m_file->msync(flag);
    m_metaFile->msync(flag);
}

void MMKV::lock() {
    SCOPED_LOCK(m_lock);
    m_exclusiveProcessLock->lock();
}
void MMKV::unlock() {
    SCOPED_LOCK(m_lock);
    m_exclusiveProcessLock->unlock();
}
bool MMKV::try_lock() {
    SCOPED_LOCK(m_lock);
    return m_exclusiveProcessLock->try_lock();
}

// backup

static bool backupOneToDirectoryByFilePath(const string &mmapKey, const MMKVPath_t &srcPath, const MMKVPath_t &dstPath) {
    File crcFile(srcPath, OpenFlag::ReadOnly);
    if (!crcFile.isFileValid()) {
        return false;
    }

    bool ret = false;
    {
#ifdef MMKV_WIN32
        MMKVInfo("backup one mmkv[%s] from [%ls] to [%ls]", mmapKey.c_str(), srcPath.c_str(), dstPath.c_str());
#else
        MMKVInfo("backup one mmkv[%s] from [%s] to [%s]", mmapKey.c_str(), srcPath.c_str(), dstPath.c_str());
#endif
        FileLock fileLock(crcFile.getFd());
        InterProcessLock lock(&fileLock, SharedLockType);
        SCOPED_LOCK(&lock);

        ret = copyFile(srcPath, dstPath);
        if (ret) {
            auto srcCRCPath = srcPath + CRC_SUFFIX;
            auto dstCRCPath = dstPath + CRC_SUFFIX;
            ret = copyFile(srcCRCPath, dstCRCPath);
        }
        MMKVInfo("finish backup one mmkv[%s]", mmapKey.c_str());
    }
    return ret;
}

bool MMKV::backupOneToDirectory(const string &mmapKey, const MMKVPath_t &dstPath, const MMKVPath_t &srcPath, bool compareFullPath) {
    // we have to lock the creation of MMKV instance, regardless of in cache or not
    SCOPED_LOCK(g_instanceLock);
    MMKV *kv = nullptr;
    if (!compareFullPath) {
        auto itr = g_instanceDic->find(mmapKey);
        if (itr != g_instanceDic->end()) {
            kv = itr->second;
        }
    } else {
        // mmapKey is actually filename, we can't simply call find()
        for (auto &pair : *g_instanceDic) {
            if (pair.second->m_path == srcPath) {
                kv = pair.second;
                break;
            }
        }
    }
    // get one in cache, do it the easy way
    if (kv) {
#ifdef MMKV_WIN32
        MMKVInfo("backup one cached mmkv[%s] from [%ls] to [%ls]", mmapKey.c_str(), srcPath.c_str(), dstPath.c_str());
#else
        MMKVInfo("backup one cached mmkv[%s] from [%s] to [%s]", mmapKey.c_str(), srcPath.c_str(), dstPath.c_str());
#endif
        SCOPED_LOCK(kv->m_lock);
        SCOPED_LOCK(kv->m_sharedProcessLock);

        kv->sync();
        auto ret = copyFile(kv->m_path, dstPath);
        if (ret) {
            auto dstCRCPath = dstPath + CRC_SUFFIX;
            ret = copyFile(kv->m_crcPath, dstCRCPath);
        }
        MMKVInfo("finish backup one mmkv[%s], ret: %d", mmapKey.c_str(), ret);
        return ret;
    }

    // no luck with cache, do it the hard way
    bool ret = backupOneToDirectoryByFilePath(mmapKey, srcPath, dstPath);
    return ret;
}

bool MMKV::backupOneToDirectory(const string &mmapID, const MMKVPath_t &dstDir, const MMKVPath_t *srcDir) {
    auto rootPath = srcDir ? srcDir : &g_rootDir;
    if (*rootPath == dstDir) {
        return true;
    }
    mkPath(dstDir);
    auto encodePath = encodeFilePath(mmapID, dstDir);
    auto dstPath = dstDir + MMKV_PATH_SLASH + encodePath;
    auto mmapKey = mmapedKVKey(mmapID, rootPath);
#ifdef MMKV_ANDROID
    // historically Android mistakenly use mmapKey as mmapID
    auto srcPath = *rootPath + MMKV_PATH_SLASH + encodeFilePath(mmapKey, *rootPath);
#else
    auto srcPath = *rootPath + MMKV_PATH_SLASH + encodePath;
#endif
    return backupOneToDirectory(mmapKey, dstPath, srcPath, false);
}

bool endsWith(const MMKVPath_t &str, const MMKVPath_t &suffix) {
    if (str.length() >= suffix.length()) {
        return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    } else {
        return false;
    }
}

MMKVPath_t filename(const MMKVPath_t &path) {
    auto startPos = path.rfind(MMKV_PATH_SLASH);
    startPos++; // don't need to check for npos, because npos+1 == 0
    auto filename = path.substr(startPos);
    return filename;
}

size_t MMKV::backupAllToDirectory(const MMKVPath_t &dstDir, const MMKVPath_t &srcDir, bool isInSpecialDir) {
    unordered_set<MMKVPath_t> mmapIDSet;
    unordered_set<MMKVPath_t> mmapIDCRCSet;
    walkInDir(srcDir, WalkFile, [&](const MMKVPath_t &filePath, WalkType) {
        if (endsWith(filePath, CRC_SUFFIX)) {
            mmapIDCRCSet.insert(filePath);
        } else {
            mmapIDSet.insert(filePath);
        }
    });

    size_t count = 0;
    if (!mmapIDSet.empty()) {
        mkPath(dstDir);
        auto compareFullPath = isInSpecialDir;
        for (auto &srcPath : mmapIDSet) {
            auto srcCRCPath = srcPath + CRC_SUFFIX;
            if (mmapIDCRCSet.find(srcCRCPath) == mmapIDCRCSet.end()) {
#ifdef MMKV_WIN32
                MMKVWarning("crc not exist [%ls]", srcCRCPath.c_str());
#else
                MMKVWarning("crc not exist [%s]", srcCRCPath.c_str());
#endif
                continue;
            }
            auto basename = filename(srcPath);
            const auto &strBasename = MMKVPath_t2String(basename);
            auto mmapKey = isInSpecialDir ? strBasename : mmapedKVKey(strBasename, &srcDir);
            auto dstPath = dstDir + MMKV_PATH_SLASH + basename;
            if (backupOneToDirectory(mmapKey, dstPath, srcPath, compareFullPath)) {
                count++;
            }
        }
    }
    return count;
}

size_t MMKV::backupAllToDirectory(const MMKVPath_t &dstDir, const MMKVPath_t *srcDir) {
    auto rootPath = srcDir ? srcDir : &g_rootDir;
    if (*rootPath == dstDir) {
        return true;
    }
    auto count = backupAllToDirectory(dstDir, *rootPath, false);

    auto specialSrcDir = *rootPath + MMKV_PATH_SLASH + SPECIAL_CHARACTER_DIRECTORY_NAME;
    if (isFileExist(specialSrcDir)) {
        auto specialDstDir = dstDir + MMKV_PATH_SLASH + SPECIAL_CHARACTER_DIRECTORY_NAME;
        count += backupAllToDirectory(specialDstDir, specialSrcDir, true);
    }
    return count;
}

// restore

static bool restoreOneFromDirectoryByFilePath(const string &mmapKey, const MMKVPath_t &srcPath, const MMKVPath_t &dstPath) {
    auto dstCRCPath = dstPath + CRC_SUFFIX;
    File dstCRCFile(std::move(dstCRCPath), OpenFlag::ReadWrite | OpenFlag::Create);
    if (!dstCRCFile.isFileValid()) {
        return false;
    }

    bool ret = false;
    {
#ifdef MMKV_WIN32
        MMKVInfo("restore one mmkv[%s] from [%ls] to [%ls]", mmapKey.c_str(), srcPath.c_str(), dstPath.c_str());
#else
        MMKVInfo("restore one mmkv[%s] from [%s] to [%s]", mmapKey.c_str(), srcPath.c_str(), dstPath.c_str());
#endif
        FileLock fileLock(dstCRCFile.getFd());
        InterProcessLock lock(&fileLock, ExclusiveLockType);
        SCOPED_LOCK(&lock);

        ret = copyFileContent(srcPath, dstPath);
        if (ret) {
            auto srcCRCPath = srcPath + CRC_SUFFIX;
            ret = copyFileContent(srcCRCPath, dstCRCFile.getFd());
        }
        MMKVInfo("finish restore one mmkv[%s]", mmapKey.c_str());
    }
    return ret;
}

// We can't simply replace the existing file, because other processes might have already open it.
// They won't know a difference when the file has been replaced.
// We have to let them know by overriding the existing file with new content.
bool MMKV::restoreOneFromDirectory(const string &mmapKey, const MMKVPath_t &srcPath, const MMKVPath_t &dstPath, bool compareFullPath) {
    // we have to lock the creation of MMKV instance, regardless of in cache or not
    SCOPED_LOCK(g_instanceLock);
    MMKV *kv = nullptr;
    if (!compareFullPath) {
        auto itr = g_instanceDic->find(mmapKey);
        if (itr != g_instanceDic->end()) {
            kv = itr->second;
        }
    } else {
        // mmapKey is actually filename, we can't simply call find()
        for (auto &pair : *g_instanceDic) {
            if (pair.second->m_path == dstPath) {
                kv = pair.second;
                break;
            }
        }
    }
    // get one in cache, do it the easy way
    if (kv) {
#ifdef MMKV_WIN32
        MMKVInfo("restore one cached mmkv[%s] from [%ls] to [%ls]", mmapKey.c_str(), srcPath.c_str(), dstPath.c_str());
#else
        MMKVInfo("restore one cached mmkv[%s] from [%s] to [%s]", mmapKey.c_str(), srcPath.c_str(), dstPath.c_str());
#endif
        SCOPED_LOCK(kv->m_lock);
        SCOPED_LOCK(kv->m_exclusiveProcessLock);

        kv->sync();
        auto ret = copyFileContent(srcPath, kv->m_file->getFd());
        if (ret) {
            auto srcCRCPath = srcPath + CRC_SUFFIX;
            ret = copyFileContent(srcCRCPath, kv->m_metaFile->getFd());
        }

        // reload data after restore
        kv->clearMemoryCache();
        kv->loadFromFile();
        if (kv->m_isInterProcess) {
            kv->notifyContentChanged();
        }

        MMKVInfo("finish restore one mmkv[%s], ret: %d", mmapKey.c_str(), ret);
        return ret;
    }

    // no luck with cache, do it the hard way
    bool ret = restoreOneFromDirectoryByFilePath(mmapKey, srcPath, dstPath);
    return ret;
}

bool MMKV::restoreOneFromDirectory(const string &mmapID, const MMKVPath_t &srcDir, const MMKVPath_t *dstDir) {
    auto rootPath = dstDir ? dstDir : &g_rootDir;
    if (*rootPath == srcDir) {
        return true;
    }
    mkPath(*rootPath);
    auto encodePath = encodeFilePath(mmapID, *rootPath);
    auto srcPath = srcDir + MMKV_PATH_SLASH + encodePath;
    auto mmapKey = mmapedKVKey(mmapID, rootPath);
#ifdef MMKV_ANDROID
    // historically Android mistakenly use mmapKey as mmapID
    auto dstPath = *rootPath + MMKV_PATH_SLASH + encodeFilePath(mmapKey, *rootPath);
#else
    auto dstPath = *rootPath + MMKV_PATH_SLASH + encodePath;
#endif
    return restoreOneFromDirectory(mmapKey, srcPath, dstPath, false);
}

size_t MMKV::restoreAllFromDirectory(const MMKVPath_t &srcDir, const MMKVPath_t &dstDir, bool isInSpecialDir) {
    unordered_set<MMKVPath_t> mmapIDSet;
    unordered_set<MMKVPath_t> mmapIDCRCSet;
    walkInDir(srcDir, WalkFile, [&](const MMKVPath_t &filePath, WalkType) {
        if (endsWith(filePath, CRC_SUFFIX)) {
            mmapIDCRCSet.insert(filePath);
        } else {
            mmapIDSet.insert(filePath);
        }
    });

    size_t count = 0;
    if (!mmapIDSet.empty()) {
        mkPath(dstDir);
        auto compareFullPath = isInSpecialDir;
        for (auto &srcPath : mmapIDSet) {
            auto srcCRCPath = srcPath + CRC_SUFFIX;
            if (mmapIDCRCSet.find(srcCRCPath) == mmapIDCRCSet.end()) {
#ifdef MMKV_WIN32
                MMKVWarning("crc not exist [%ls]", srcCRCPath.c_str());
#else
                MMKVWarning("crc not exist [%s]", srcCRCPath.c_str());
#endif
                continue;
            }
            auto basename = filename(srcPath);
            const auto &strBasename = MMKVPath_t2String(basename);
            auto mmapKey = isInSpecialDir ? strBasename : mmapedKVKey(strBasename, &dstDir);
            auto dstPath = dstDir + MMKV_PATH_SLASH + basename;
            if (restoreOneFromDirectory(mmapKey, srcPath, dstPath, compareFullPath)) {
                count++;
            }
        }
    }
    return count;
}

size_t MMKV::restoreAllFromDirectory(const MMKVPath_t &srcDir, const MMKVPath_t *dstDir) {
    auto rootPath = dstDir ? dstDir : &g_rootDir;
    if (*rootPath == srcDir) {
        return true;
    }
    auto count = restoreAllFromDirectory(srcDir, *rootPath, true);

    auto specialSrcDir = srcDir + MMKV_PATH_SLASH + SPECIAL_CHARACTER_DIRECTORY_NAME;
    if (isFileExist(specialSrcDir)) {
        auto specialDstDir = *rootPath + MMKV_PATH_SLASH + SPECIAL_CHARACTER_DIRECTORY_NAME;
        count += restoreAllFromDirectory(specialSrcDir, specialDstDir, false);
    }
    return count;
}

// callbacks

void MMKV::registerErrorHandler(ErrorHandler handler) {
    SCOPED_LOCK(g_instanceLock);
    g_errorHandler = handler;
}

void MMKV::unRegisterErrorHandler() {
    SCOPED_LOCK(g_instanceLock);
    g_errorHandler = nullptr;
}

void MMKV::registerLogHandler(LogHandler handler) {
    SCOPED_LOCK(g_instanceLock);
    g_logHandler = handler;
}

void MMKV::unRegisterLogHandler() {
    SCOPED_LOCK(g_instanceLock);
    g_logHandler = nullptr;
}

void MMKV::setLogLevel(MMKVLogLevel level) {
    SCOPED_LOCK(g_instanceLock);
    g_currentLogLevel = level;
}

static void mkSpecialCharacterFileDirectory() {
    MMKVPath_t path = g_rootDir + MMKV_PATH_SLASH + SPECIAL_CHARACTER_DIRECTORY_NAME;
    mkPath(path);
}

template <typename T>
static string md5(const basic_string<T> &value) {
    uint8_t md[MD5_DIGEST_LENGTH] = {};
    char tmp[3] = {}, buf[33] = {};
    openssl::MD5((const uint8_t *) value.c_str(), value.size() * (sizeof(T) / sizeof(uint8_t)), md);
    for (auto ch : md) {
        snprintf(tmp, sizeof(tmp), "%2.2x", ch);
        strcat(buf, tmp);
    }
    return {buf};
}

static MMKVPath_t encodeFilePath(const string &mmapID) {
    const char *specialCharacters = "\\/:*?\"<>|";
    string encodedID;
    bool hasSpecialCharacter = false;
    for (auto ch : mmapID) {
        if (strchr(specialCharacters, ch) != nullptr) {
            encodedID = md5(mmapID);
            hasSpecialCharacter = true;
            break;
        }
    }
    if (hasSpecialCharacter) {
        static ThreadOnceToken_t once_control = ThreadOnceUninitialized;
        ThreadLock::ThreadOnce(&once_control, mkSpecialCharacterFileDirectory);
        return MMKVPath_t(SPECIAL_CHARACTER_DIRECTORY_NAME) + MMKV_PATH_SLASH + string2MMKVPath_t(encodedID);
    } else {
        return string2MMKVPath_t(mmapID);
    }
}

static MMKVPath_t encodeFilePath(const string &mmapID, const MMKVPath_t &rootDir) {
    const char *specialCharacters = "\\/:*?\"<>|";
    string encodedID;
    bool hasSpecialCharacter = false;
    for (auto ch : mmapID) {
        if (strchr(specialCharacters, ch) != nullptr) {
            encodedID = md5(mmapID);
            hasSpecialCharacter = true;
            break;
        }
    }
    if (hasSpecialCharacter) {
        MMKVPath_t path = rootDir + MMKV_PATH_SLASH + SPECIAL_CHARACTER_DIRECTORY_NAME;
        mkPath(path);

        return MMKVPath_t(SPECIAL_CHARACTER_DIRECTORY_NAME) + MMKV_PATH_SLASH + string2MMKVPath_t(encodedID);
    } else {
        return string2MMKVPath_t(mmapID);
    }
}

string mmapedKVKey(const string &mmapID, const MMKVPath_t *rootPath) {
    if (rootPath && g_rootDir != (*rootPath)) {
        return md5(*rootPath + MMKV_PATH_SLASH + string2MMKVPath_t(mmapID));
    }
    return mmapID;
}

MMKVPath_t mappedKVPathWithID(const string &mmapID, MMKVMode mode, const MMKVPath_t *rootPath) {
#ifndef MMKV_ANDROID
    if (rootPath) {
#else
    if (mode & MMKV_ASHMEM) {
        return ashmemMMKVPathWithID(encodeFilePath(mmapID));
    } else if (rootPath) {
#endif
        return *rootPath + MMKV_PATH_SLASH + encodeFilePath(mmapID);
    }
    return g_rootDir + MMKV_PATH_SLASH + encodeFilePath(mmapID);
}

MMKVPath_t crcPathWithID(const string &mmapID, MMKVMode mode, const MMKVPath_t *rootPath) {
#ifndef MMKV_ANDROID
    if (rootPath) {
#else
    if (mode & MMKV_ASHMEM) {
        return ashmemMMKVPathWithID(encodeFilePath(mmapID)) + CRC_SUFFIX;
    } else if (rootPath) {
#endif
        return *rootPath + MMKV_PATH_SLASH + encodeFilePath(mmapID) + CRC_SUFFIX;
    }
    return g_rootDir + MMKV_PATH_SLASH + encodeFilePath(mmapID) + CRC_SUFFIX;
}

MMKVRecoverStrategic onMMKVCRCCheckFail(const string &mmapID) {
    if (g_errorHandler) {
        return g_errorHandler(mmapID, MMKVErrorType::MMKVCRCCheckFail);
    }
    return OnErrorDiscard;
}

MMKVRecoverStrategic onMMKVFileLengthError(const string &mmapID) {
    if (g_errorHandler) {
        return g_errorHandler(mmapID, MMKVErrorType::MMKVFileLength);
    }
    return OnErrorDiscard;
}

MMKV_NAMESPACE_END
