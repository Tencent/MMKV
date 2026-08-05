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
#include "MMKV_OSX.h"
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
#include <limits>
#include <unordered_set>
#include <cassert>

#if defined(__aarch64__) && defined(__linux__) && !defined (MMKV_OHOS)
#    include <asm/hwcap.h>
#    include <sys/auxv.h>
#endif

#ifdef MMKV_APPLE
#    if __has_feature(objc_arc)
#        error This file must be compiled with MRC. Use -fno-objc-arc flag.
#    endif
#    include "MMKV_OSX.h"
#endif // MMKV_APPLE

using namespace std;
using namespace mmkv;

unordered_map<string, MMKV *> *g_instanceDic;
ThreadLock *g_instanceLock;
MMKVPath_t g_rootDir;
MMKVPath_t g_realRootDir;
static ThreadLock *g_namespaceLock;
static unordered_map<MMKVPath_t, MMKVPath_t> g_realRootMap;
size_t mmkv::DEFAULT_MMAP_SIZE;

MMKV_NAMESPACE_BEGIN

static MMKVPath_t encodeFilePath(const string &mmapID, const MMKVPath_t &rootDir);
static MMKVPath_t encodeFilePathWithoutCreating(const string &mmapID);
static MMKVPath_t mappedKVPathWithIDWithoutCreating(const string &mmapID, const MMKVPath_t &rootPath);
static bool mmapIDHasSpecialCharacter(const string &mmapID);
bool endsWith(const MMKVPath_t &str, const MMKVPath_t &suffix);
MMKVPath_t filename(const MMKVPath_t &path);

#ifndef MMKV_ANDROID
MMKV::MMKV(const string &mmapID, const MMKVConfig &config)
    : m_mmapID(mmapID)
    , m_mode(config.mode)
    , m_path(mappedKVPathWithID(m_mmapID, config.rootPath, true))
    , m_crcPath(crcPathWithPath(m_path))
    , m_dic(nullptr)
    , m_dicCrypt(nullptr)
    , m_expectedCapacity(std::max<size_t>(DEFAULT_MMAP_SIZE, roundUp<size_t>(config.expectedCapacity, DEFAULT_MMAP_SIZE)))
    , m_file(new MemoryFile(m_path, m_expectedCapacity, isReadOnly(), true))
    , m_metaFile(new MemoryFile(m_crcPath, 0, isReadOnly(), !isMultiProcess()))
    , m_metaInfo(new MMKVMetaInfo())
    , m_crypter(nullptr)
    , m_lock(new ThreadLock())
    , m_fileLock(new FileLock(isMultiProcess() ? m_metaFile->getFd() : MMKVFileHandleInvalidValue))
    , m_sharedProcessLock(new InterProcessLock(m_fileLock, SharedLockType))
    , m_exclusiveProcessLock(new InterProcessLock(m_fileLock, ExclusiveLockType))
{
    m_actualSize = 0;
    m_output = nullptr;

#    ifndef MMKV_DISABLE_CRYPT
    auto cryptKey = config.cryptKey;
    if (cryptKey && !cryptKey->empty()) {
        m_dicCrypt = new MMKVMapCrypt();
        m_crypter = new AESCrypt(cryptKey->data(), cryptKey->length(), nullptr, 0, config.aes256);
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
    m_sharedProcessLock->m_enable = isMultiProcess();
    m_exclusiveProcessLock->m_enable = isMultiProcess();

    m_recoverStrategic = config.recover;
    m_itemSizeLimit = config.itemSizeLimit;

    if (config.enableKeyExpire.has_value()) {
        configAutoExipreIfNeeded(config);
    }

    if (config.enableCompareBeforeSet) {
        enableCompareBeforeSet();
    }
}
#endif

MMKV::~MMKV() {
    clearMemoryCache();

    delete m_dic;
#ifndef MMKV_DISABLE_CRYPT
    delete m_dicCrypt;
    delete m_crypter;
#endif
    delete m_metaInfo;
    delete m_lock;
    delete m_fileLock;
    delete m_sharedProcessLock;
    delete m_exclusiveProcessLock;
#ifdef MMKV_ANDROID
#ifndef MMKV_OHOS
    delete m_sharedProcessModeLock;
    delete m_exclusiveProcessModeLock;
    delete m_fileModeLock;
#endif // !MMKV_OHOS
    delete m_sharedMigrationLock;
    delete m_fileMigrationLock;
#endif // MMKV_ANDROID
    delete m_metaFile;
    delete m_file;

    MMKVInfo("destruct [%s]", m_mmapID.c_str());
}

MMKV *MMKV::defaultMMKV(MMKVMode mode, const string *cryptKey, bool aes256) {
    auto config = MMKVConfig();
    config.mode = mode;
#ifndef MMKV_DISABLE_CRYPT
    config.aes256 = aes256;
    config.cryptKey = cryptKey;
#else
    (void) cryptKey;
    (void) aes256;
#endif
    return mmkvWithID(DEFAULT_MMAP_ID, config);
}

MMKV *MMKV::defaultMMKV(const MMKVConfig &config) {
    return mmkvWithID(DEFAULT_MMAP_ID, config);
}

static void initialize() {
    g_instanceDic = new unordered_map<string, MMKV *>;
    g_instanceLock = new ThreadLock();
    g_instanceLock->initialize();

    mmkv::DEFAULT_MMAP_SIZE = mmkv::getPageSize();
    MMKVInfo("version %s, page size %d, arch %s", MMKV_VERSION, DEFAULT_MMAP_SIZE, MMKV_ABI);

    // get CPU status of ARMv8 extensions (CRC32, AES)
#if defined(__aarch64__) && defined(__linux__) && !defined (MMKV_OHOS)
    auto hwcaps = getauxval(AT_HWCAP);
#    ifndef MMKV_DISABLE_CRYPT
    if (hwcaps & HWCAP_AES) {
        openssl::AES_set_encrypt_key = openssl_aes_arm_set_encrypt_key;
        openssl::AES_set_decrypt_key = openssl_aes_arm_set_decrypt_key;
        openssl::AES_encrypt = openssl_aes_arm_encrypt;
        openssl::AES_decrypt = openssl_aes_arm_decrypt;
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
#endif     // __aarch64__ && defined(__linux__) && !defined (MMKV_OHOS)

#if defined(MMKV_DEBUG) && !defined(MMKV_DISABLE_CRYPT)
    // AESCrypt::testAESCrypt();
    // KeyValueHolderCrypt::testAESToMMBuffer();
#endif
}

static void ensureMinimalInitialize() {
    static ThreadOnceToken_t once_control = ThreadOnceUninitialized;
    ThreadLock::ThreadOnce(&once_control, initialize);
}

void MMKV::initializeMMKV(const MMKVPath_t &rootDir, MMKVLogLevel logLevel, mmkv::MMKVHandler *handler) {
    g_currentLogLevel = logLevel;
    g_handler = handler;

    ensureMinimalInitialize();

#ifdef MMKV_APPLE
    // crc32 instruction requires A10 chip, aka iPhone 7 or iPad 6th generation
    int device = 0, version = 0;
    GetAppleMachineInfo(device, version);
    MMKVInfo("Apple Device: %d, version: %d", device, version);
#endif

    if (g_rootDir.empty()) {
        g_rootDir = rootDir;
        // avoid operating g_realRootMap directly
        g_realRootDir = nameSpace(rootDir).getRootDir();
        mkPath(g_realRootDir);
    }
    const auto &rootDirStr = MMKVPath_t2String(g_realRootDir);
    MMKVInfo("root dir: %s", rootDirStr.c_str());
}

const MMKVPath_t &MMKV::getRootDir() {
    // for backword consistency we can't return g_realRootDir
    return g_rootDir;
}

#ifndef MMKV_ANDROID
MMKV *MMKV::getMMKVWithID(const std::string &mmapID, const MMKVConfig &config) {
    if (mmapID.empty() || !g_instanceLock) {
        return nullptr;
    }
    SCOPED_LOCK(g_instanceLock);

    auto rootPath = config.rootPath;
    auto mmapKey = mmapedKVKey(mmapID, rootPath, true);
    auto itr = g_instanceDic->find(mmapKey);
    if (itr != g_instanceDic->end()) {
        MMKV *kv = itr->second;
        return kv;
    }

    if (rootPath && (rootPath != &g_realRootDir) && !(config.mode & MMKV_READ_ONLY)) {
        MMKVPath_t specialPath = (*rootPath) + MMKV_PATH_SLASH + SPECIAL_CHARACTER_DIRECTORY_NAME;
        if (!isFileExist(specialPath)) {
            mkPath(specialPath);
        }
    }
    auto theRootDir = rootPath ? rootPath : &g_realRootDir;
    const auto &theRoot = MMKVPath_t2String(*theRootDir);
    MMKVInfo("prepare to load %s (id %s) from rootPath %s", mmapID.c_str(), mmapKey.c_str(), theRoot.c_str());

    auto kv = new MMKV(mmapID, config);
    kv->m_mmapKey = mmapKey;
    (*g_instanceDic)[mmapKey] = kv;
    return kv;
}
#endif

MMKV *MMKV::mmkvWithID(const string &mmapID, MMKVMode mode, const string *cryptKey, const MMKVPath_t *rootPath, size_t expectedCapacity, bool aes256) {
    MMKVConfig config;
    config.mode = mode;
#ifndef MMKV_DISABLE_CRYPT
    config.aes256 = aes256;
    config.cryptKey = cryptKey;
#endif
    config.rootPath = rootPath;
    config.expectedCapacity = expectedCapacity;

    return mmkvWithID(mmapID, config);
}

MMKV *MMKV::mmkvWithID(const std::string &mmapID, const MMKVConfig &config) {
    if (mmapID.empty() || !g_instanceLock) {
        return nullptr;
    }
    auto ns = config.rootPath ? nameSpace(*config.rootPath) : defaultNameSpace();

    auto newConfig = config;
    newConfig.rootPath = &ns.m_rootDir;
    return getMMKVWithID(mmapID, newConfig);
}

void MMKV::onExit() {
    if (!g_instanceLock) {
        return;
    }
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

void MMKV::notifyContentChanged() {
    if (g_handler) {
        g_handler->onContentChangedByOuterProcess(m_mmapID);
    }
}

void MMKV::notifyContentLoaded() {
    if (g_handler) {
        g_handler->onMMKVContentLoadSuccessfully(m_mmapID);
    }
}

void MMKV::checkContentChanged() {
    SCOPED_LOCK(m_lock);
    checkLoadData();
}

void MMKV::clearMemoryCache(bool keepSpace) {
    SCOPED_LOCK(m_lock);
    if (m_needLoadFromFile && !keepSpace) {
        return;
    }
    MMKVInfo("clearMemoryCache [%s]", m_mmapID.c_str());
    m_needLoadFromFile = true;
    m_hasFullWriteback = false;

    clearDictionary(m_dic);
#ifndef MMKV_DISABLE_CRYPT
    clearDictionary(m_dicCrypt);
    if (m_crypter) {
        // if read-only, cannot garrentee we have random iv
        if (m_metaInfo->m_version >= MMKVVersionRandomIV) {
            m_crypter->resetIV(m_metaInfo->m_vector, sizeof(m_metaInfo->m_vector));
        } else {
            m_crypter->resetIV();
        }
    }
#endif

    delete m_output;
    m_output = nullptr;

    if (!keepSpace) {
        m_file->clearMemoryCache();
    }
    // inter-process lock rely on MetaFile's fd, never close it
    // m_metaFile->clearMemoryCache();
    m_actualSize = 0;
    m_metaInfo->m_crcDigest = 0;
}

void MMKV::close() {
    MMKVInfo("close [%s]", m_mmapID.c_str());
    SCOPED_LOCK(g_instanceLock);
    m_lock->lock();

    auto itr = g_instanceDic->find(m_mmapKey);
    if (itr != g_instanceDic->end()) {
        g_instanceDic->erase(itr);
    }
    // close() requires the caller to guarantee that no other operation or
    // alias will use this instance. ThreadLock's destructor releases the
    // recursive lock ownership held by this close path before destroying the
    // underlying platform lock.
    delete this;
}

#ifndef MMKV_DISABLE_CRYPT

string MMKV::cryptKey() const {
    SCOPED_LOCK(m_lock);

    if (m_crypter) {
        char key[AES256_KEY_LEN] = {};
        m_crypter->getKey(key);
        return {key, m_crypter->getKeyLength()};
    }
    return "";
}

void MMKV::checkReSetCryptKey(const string *cryptKey, bool aes256) {
    SCOPED_LOCK(m_lock);

    const auto hasNewKey = cryptKey && !cryptKey->empty();
    if ((!m_crypter && !hasNewKey) ||
        (m_crypter && hasNewKey && m_crypter->isSameKey(cryptKey->data(), cryptKey->length(), aes256))) {
        return;
    }

    AESCrypt *newCrypter = nullptr;
    MMKVMap *newPlainDictionary = nullptr;
    MMKVMapCrypt *newEncryptedDictionary = nullptr;
    try {
        if (hasNewKey) {
            MMKVInfo("setting new aes key");
            newCrypter = new AESCrypt(cryptKey->data(), cryptKey->length(), nullptr, 0, aes256);
            if (!m_dicCrypt) {
                newEncryptedDictionary = new MMKVMapCrypt();
            }
        } else {
            MMKVInfo("reset aes key");
            if (!m_dic) {
                newPlainDictionary = new MMKVMap();
            }
        }
    } catch (const exception &error) {
        MMKVError("[%s] cannot allocate replacement crypt state: %s", m_mmapID.c_str(), error.what());
        delete newEncryptedDictionary;
        delete newPlainDictionary;
        delete newCrypter;
        return;
    } catch (...) {
        MMKVError("[%s] cannot allocate replacement crypt state", m_mmapID.c_str());
        delete newEncryptedDictionary;
        delete newPlainDictionary;
        delete newCrypter;
        return;
    }

    // The active dictionary type must always match m_crypter. Reset loaded state before switching,
    // and preallocate the target dictionary before changing any active state.
    clearMemoryCache();
    if (newEncryptedDictionary) {
        m_dicCrypt = newEncryptedDictionary;
    }
    if (newPlainDictionary) {
        m_dic = newPlainDictionary;
    }
    delete m_crypter;
    m_crypter = newCrypter;
    checkLoadData();
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

bool MMKV::recalculateCRCDigestWithIV(const void *iv) {
    auto ptr = (const uint8_t *) m_file->getMemory();
    if (!ptr) {
        return false;
    }
    m_crcDigest = (uint32_t) CRC32(0, ptr + Fixed32Size, (uint32_t) m_actualSize);
    return writeActualSize(m_actualSize, m_crcDigest, iv, IncreaseSequence);
}

void MMKV::recalculateCRCDigestOnly() {
    auto ptr = (const uint8_t *) m_file->getMemory();
    if (ptr) {
        m_crcDigest = 0;
        m_crcDigest = (uint32_t) CRC32(0, ptr + Fixed32Size, (uint32_t) m_actualSize);
        writeActualSize(m_actualSize, m_crcDigest, nullptr, KeepSequence);
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
    size_t size = mmkv_unlikely(m_enableKeyExpire) ? Fixed32Size + pbBoolSize() : pbBoolSize();
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeBool(value);
    if (mmkv_unlikely(m_enableKeyExpire)) {
        auto time = (expireDuration != ExpireNever) ? safeExpirationPlusCurrentTime(expireDuration) : ExpireNever;
        output.writeRawLittleEndian32(UInt32ToInt32(time));
    } else {
        assert(expireDuration == ExpireNever && "setting expire duration without calling enableAutoKeyExpire() first");
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
    size_t size = mmkv_unlikely(m_enableKeyExpire) ? Fixed32Size + pbInt32Size(value) : pbInt32Size(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeInt32(value);
    if (mmkv_unlikely(m_enableKeyExpire)) {
        auto time = (expireDuration != ExpireNever) ? safeExpirationPlusCurrentTime(expireDuration) : ExpireNever;
        output.writeRawLittleEndian32(UInt32ToInt32(time));
    } else {
        assert(expireDuration == ExpireNever && "setting expire duration without calling enableAutoKeyExpire() first");
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
    size_t size = mmkv_unlikely(m_enableKeyExpire) ? Fixed32Size + pbUInt32Size(value) : pbUInt32Size(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeUInt32(value);
    if (mmkv_unlikely(m_enableKeyExpire)) {
        auto time = (expireDuration != ExpireNever) ? safeExpirationPlusCurrentTime(expireDuration) : ExpireNever;
        output.writeRawLittleEndian32(UInt32ToInt32(time));
    } else {
        assert(expireDuration == ExpireNever && "setting expire duration without calling enableAutoKeyExpire() first");
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
    size_t size = mmkv_unlikely(m_enableKeyExpire) ? Fixed32Size + pbInt64Size(value) : pbInt64Size(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeInt64(value);
    if (mmkv_unlikely(m_enableKeyExpire)) {
        auto time = (expireDuration != ExpireNever) ? safeExpirationPlusCurrentTime(expireDuration) : ExpireNever;
        output.writeRawLittleEndian32(UInt32ToInt32(time));
    } else {
        assert(expireDuration == ExpireNever && "setting expire duration without calling enableAutoKeyExpire() first");
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
    size_t size = mmkv_unlikely(m_enableKeyExpire) ? Fixed32Size + pbUInt64Size(value) : pbUInt64Size(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeUInt64(value);
    if (mmkv_unlikely(m_enableKeyExpire)) {
        auto time = (expireDuration != ExpireNever) ? safeExpirationPlusCurrentTime(expireDuration) : ExpireNever;
        output.writeRawLittleEndian32(UInt32ToInt32(time));
    } else {
        assert(expireDuration == ExpireNever && "setting expire duration without calling enableAutoKeyExpire() first");
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
    size_t size = mmkv_unlikely(m_enableKeyExpire) ? Fixed32Size + pbFloatSize() : pbFloatSize();
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeFloat(value);
    if (mmkv_unlikely(m_enableKeyExpire)) {
        auto time = (expireDuration != ExpireNever) ? safeExpirationPlusCurrentTime(expireDuration) : ExpireNever;
        output.writeRawLittleEndian32(UInt32ToInt32(time));
    } else {
        assert(expireDuration == ExpireNever && "setting expire duration without calling enableAutoKeyExpire() first");
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
    size_t size = mmkv_unlikely(m_enableKeyExpire) ? Fixed32Size + pbDoubleSize() : pbDoubleSize();
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeDouble(value);
    if (mmkv_unlikely(m_enableKeyExpire)) {
        auto time = (expireDuration != ExpireNever) ? safeExpirationPlusCurrentTime(expireDuration) : ExpireNever;
        output.writeRawLittleEndian32(UInt32ToInt32(time));
    } else {
        assert(expireDuration == ExpireNever && "setting expire duration without calling enableAutoKeyExpire() first");
    }

    return setDataForKey(std::move(data), key);
}

bool MMKV::setDataForKey(mmkv::MMBuffer &&data, MMKV::MMKVKey_t key, uint32_t expireDuration) {
    if (mmkv_likely(!m_enableKeyExpire)) {
        assert(expireDuration == ExpireNever && "setting expire duration without calling enableAutoKeyExpire() first");
        return setDataForKey(std::move(data), key, true);
    } else {
        if (data.length() > numeric_limits<uint32_t>::max()) {
            MMKVError("[%s] reject value too large to encode: %zu", m_mmapID.c_str(), data.length());
            return false;
        }
        auto dataLength = static_cast<uint32_t>(data.length());
        auto encodedLength = static_cast<uint64_t>(dataLength) + pbRawVarint32Size(dataLength) + Fixed32Size;
        if (encodedLength > numeric_limits<uint32_t>::max()) {
            MMKVError("[%s] reject expiring value too large to encode: %zu", m_mmapID.c_str(), data.length());
            return false;
        }
        auto tmp = MMBuffer(static_cast<size_t>(encodedLength));
        CodedOutputData output(tmp.getPtr(), tmp.length());
        output.writeData(data);
        auto time = (expireDuration != ExpireNever) ? safeExpirationPlusCurrentTime(expireDuration) : ExpireNever;
        output.writeRawLittleEndian32(UInt32ToInt32(time));
        return setDataForKey(std::move(tmp), key);
    }
}

bool MMKV::set(const char *value, MMKVKey_t key) {
    return set(value, key, m_expiredInSeconds);
}

bool MMKV::set(const char *value, MMKVKey_t key, uint32_t expireDuration) {
    if (!value) {
        removeValueForKey(key);
        return true;
    }
    return setDataForKey(MMBuffer((void *) value, strlen(value), MMBufferNoCopy), key, expireDuration);
}

bool MMKV::set(const string &value, MMKVKey_t key) {
    return set(value, key, m_expiredInSeconds);
}

bool MMKV::set(const string &value, MMKVKey_t key, uint32_t expireDuration) {
    if (isKeyEmpty(key)) {
        return false;
    }
    return setDataForKey(MMBuffer((void *) value.data(), value.length(), MMBufferNoCopy), key, expireDuration);
}

bool MMKV::set(string_view value, MMKVKey_t key) {
    return set(value, key, m_expiredInSeconds);
}

bool MMKV::set(string_view value, MMKVKey_t key, uint32_t expireDuration) {
    if (isKeyEmpty(key)) {
        return false;
    }
    return setDataForKey(MMBuffer((void *) value.data(), value.length(), MMBufferNoCopy), key, expireDuration);
}

bool MMKV::set(const MMBuffer &value, MMKVKey_t key) {
    return set(value, key, m_expiredInSeconds);
}

bool MMKV::set(const MMBuffer &value, MMKVKey_t key, uint32_t expireDuration) {
    if (isKeyEmpty(key)) {
        return false;
    }
    return setDataForKey(MMBuffer(value.getPtr(), value.length(), MMBufferNoCopy), key, expireDuration);
}

bool MMKV::set(const vector<string> &value, MMKVKey_t key) {
    return set(value, key, m_expiredInSeconds);
}

bool MMKV::set(const vector<string> &v, MMKVKey_t key, uint32_t expireDuration) {
    if (isKeyEmpty(key)) {
        return false;
    }
#ifdef MMKV_HAS_CPP20
    auto data = MiniPBCoder::encodeDataWithObject(std::span(v));
#else
    auto data = MiniPBCoder::encodeDataWithObject(v);
#endif
    if (mmkv_unlikely(m_enableKeyExpire) && data.length() > 0) {
        auto tmp = MMBuffer(data.length() + Fixed32Size);
        auto ptr = (uint8_t *) tmp.getPtr();
        memcpy(ptr, data.getPtr(), data.length());
        auto time = (expireDuration != ExpireNever) ? safeExpirationPlusCurrentTime(expireDuration) : ExpireNever;
        memcpy(ptr + data.length(), &time, Fixed32Size);
        data = std::move(tmp);
    }
    return setDataForKey(std::move(data), key);
}

bool MMKV::getString(MMKVKey_t key, string &result, bool inplaceModification) {
    if (isKeyEmpty(key)) {
        return false;
    }
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_sharedProcessLock);
    auto data = getDataForKey(key);
    if (data.length() > 0) {
        try {
            CodedInputData input(data.getPtr(), data.length());
            if (inplaceModification) {
                input.readString(result);
            } else {
                result = input.readString();
            }
            return true;
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        } catch (...) {
            MMKVError("decode fail");
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
            result = input.readData();
            return true;
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        } catch (...) {
            MMKVError("decode fail");
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
        } catch (...) {
            MMKVError("decode fail");
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
        } catch (...) {
            MMKVError("decode fail");
        }
    }
    return false;
}

void MMKV::shared_lock() {
    m_lock->lock();
    m_sharedProcessLock->lock();
}

void MMKV::shared_unlock() {
    m_sharedProcessLock->unlock();
    m_lock->unlock();
}

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
        } catch (...) {
            MMKVError("decode fail");
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
        } catch (...) {
            MMKVError("decode fail");
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
        } catch (...) {
            MMKVError("decode fail");
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
        } catch (...) {
            MMKVError("decode fail");
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
        } catch (...) {
            MMKVError("decode fail");
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
        } catch (...) {
            MMKVError("decode fail");
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
        } catch (...) {
            MMKVError("decode fail");
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
        } catch (...) {
            MMKVError("decode fail");
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
    } catch (...) {
        MMKVError("encode fail");
    }
    return -1;
}

// enumerate

bool MMKV::containsKey(MMKVKey_t key) {
    SCOPED_LOCK(m_lock);
    checkLoadData();

    if (mmkv_likely(!m_enableKeyExpire)) {
        if (m_crypter) {
            return m_dicCrypt->find(key) != m_dicCrypt->end();
        } else {
            return m_dic->find(key) != m_dic->end();
        }
    }
    auto raw = getDataWithoutMTimeForKey(key);
    return raw.length() != 0;
}

size_t MMKV::count(bool filterExpire) {
    SCOPED_LOCK(m_lock);
    checkLoadData();

    if (mmkv_unlikely(filterExpire && m_enableKeyExpire)) {
        SCOPED_LOCK(m_exclusiveProcessLock);
        fullWriteback(nullptr, true);
    }

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

bool MMKV::removeValueForKey(MMKVKey_t key) {
    if (isKeyEmpty(key)) {
        return false;
    }
    if (isReadOnly()) {
        MMKVWarning("[%s] file readonly", m_mmapID.c_str());
        return false;
    }
    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_exclusiveProcessLock);
    checkLoadData();

    return removeDataForKey(key);
}

#ifndef MMKV_APPLE

vector<string> MMKV::allKeys(bool filterExpire) {
    SCOPED_LOCK(m_lock);
    checkLoadData();

    if (mmkv_unlikely(filterExpire && m_enableKeyExpire)) {
        SCOPED_LOCK(m_exclusiveProcessLock);
        fullWriteback(nullptr, true);
    }

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

bool MMKV::removeValuesForKeys(const vector<string> &arrKeys) {
    if (isReadOnly()) {
        MMKVWarning("[%s] file readonly", m_mmapID.c_str());
        return false;
    }
    if (arrKeys.empty()) {
        return true;
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
        auto ret = fullWriteback();
        if (!ret) {
            // The map was edited before the fallible rewrite. Reload whichever state is represented
            // by the data/meta mappings so a retry cannot incorrectly succeed as a no-op.
            clearMemoryCache();
            loadFromFile();
        }
        return ret;
    }
    return true;
}

#endif // MMKV_APPLE

// file

bool MMKV::syncWithResult(SyncFlag flag) {
    MMKVInfo("MMKV::sync, SyncFlag = %d", flag);
    SCOPED_LOCK(m_lock);
    if (m_needLoadFromFile || !isFileValid()) {
        return false;
    }
    SCOPED_LOCK(m_exclusiveProcessLock);

    // Flush data before metadata for failures reported synchronously by this process.
    // Ordering alone cannot make in-place MAP_SHARED updates crash-atomic.
    if (!m_file->msync(flag)) {
        return false;
    }
    return m_metaFile->msync(flag);
}

void MMKV::sync(SyncFlag flag) {
    (void) syncWithResult(flag);
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

#ifndef MMKV_WIN32
void MMKV::lock_thread() {
    m_lock->lock();
}
void MMKV::unlock_thread() {
    m_lock->unlock();
}
bool MMKV::try_lock_thread() {
    return m_lock->try_lock();
}
#endif

// backup

class ScopedFileHandle {
    MMKVFileHandle_t m_handle;

public:
    explicit ScopedFileHandle(MMKVFileHandle_t handle) : m_handle(handle) {}
    ~ScopedFileHandle() { closeFileHandle(m_handle); }

    bool isValid() const { return m_handle != MMKVFileHandleInvalidValue; }
    MMKVFileHandle_t get() const { return m_handle; }

    explicit ScopedFileHandle(const ScopedFileHandle &) = delete;
    ScopedFileHandle &operator=(const ScopedFileHandle &) = delete;
};

static bool syncMappedPairToPinnedHandles(MemoryFile *dataFile,
                                          MemoryFile *metaFile,
                                          MMKVFileHandle_t dataFD,
                                          MMKVFileHandle_t metaFD,
                                          bool requireDurability = true) {
    // Flush each mapping first, then make that exact pinned file durable.
    // In particular, do not let Win32 MMKV_SYNC reopen a mayfly path that may
    // now name a different file. Preserve data-before-metadata publication.
    if (dataFile->isFileValid() && !dataFile->msync(MMKV_ASYNC)) {
        return false;
    }
    if (requireDurability && !syncFile(dataFD)) {
        return false;
    }
    if (metaFile->isFileValid() && !metaFile->msync(MMKV_ASYNC)) {
        return false;
    }
    return !requireDurability || syncFile(metaFD);
}

#ifdef MMKV_WIN32
static bool isWindowsPathSeparator(MMKVPath_t::value_type ch) {
    return ch == L'\\' || ch == L'/';
}

static bool equalsWindowsPathASCII(MMKVPath_t::value_type left, MMKVPath_t::value_type right) {
    if (left >= L'a' && left <= L'z') {
        left -= L'a' - L'A';
    }
    if (right >= L'a' && right <= L'z') {
        right -= L'a' - L'A';
    }
    return left == right;
}

static size_t findWindowsPathSeparator(const MMKVPath_t &path, size_t start) {
    for (auto index = start; index < path.size(); index++) {
        if (isWindowsPathSeparator(path[index])) {
            return index;
        }
    }
    return MMKVPath_t::npos;
}

static size_t windowsPathRootLength(const MMKVPath_t &path) {
    if (path.size() >= 3 && path[1] == L':' && isWindowsPathSeparator(path[2])) {
        return 3;
    }
    if (path.size() < 2 || !isWindowsPathSeparator(path[0]) || !isWindowsPathSeparator(path[1])) {
        return 0;
    }

    if (path.size() >= 4 && path[2] == L'?' && isWindowsPathSeparator(path[3])) {
        if (path.size() >= 7 && path[5] == L':' && isWindowsPathSeparator(path[6])) {
            return 7;
        }
        const MMKVPath_t unc = L"UNC";
        auto isExtendedUNC = path.size() >= 8 && equalsWindowsPathASCII(path[4], unc[0]) &&
                             equalsWindowsPathASCII(path[5], unc[1]) &&
                             equalsWindowsPathASCII(path[6], unc[2]) && isWindowsPathSeparator(path[7]);
        auto componentStart = isExtendedUNC ? 8U : 4U;
        auto firstEnd = findWindowsPathSeparator(path, componentStart);
        if (firstEnd == MMKVPath_t::npos) {
            return 0;
        }
        if (!isExtendedUNC) {
            // Device roots such as \\?\Volume{GUID}\ end after one component.
            return firstEnd + 1;
        }
        auto secondEnd = findWindowsPathSeparator(path, firstEnd + 1);
        return secondEnd == MMKVPath_t::npos ? 0 : secondEnd + 1;
    }

    auto serverEnd = findWindowsPathSeparator(path, 2);
    if (serverEnd == MMKVPath_t::npos) {
        return 0;
    }
    auto shareEnd = findWindowsPathSeparator(path, serverEnd + 1);
    return shareEnd == MMKVPath_t::npos ? 0 : shareEnd + 1;
}
#endif

static MMKVPath_t parentDirectory(const MMKVPath_t &path) {
#ifdef MMKV_WIN32
    auto end = path.find_last_of(L"\\/");
#else
    auto end = path.rfind(MMKV_PATH_SLASH[0]);
#endif
    if (end == MMKVPath_t::npos) {
        return string2MMKVPath_t(".");
    }
#ifdef MMKV_WIN32
    auto rootLength = windowsPathRootLength(path);
    if (rootLength > 0 && end < rootLength) {
        return path.substr(0, rootLength);
    }
#endif
    return path.substr(0, std::max<size_t>(1, end));
}

static bool isValidDirectoryPath(const MMKVPath_t &path) {
    return !path.empty() && path.find(MMKVPath_t::value_type{}) == MMKVPath_t::npos;
}

static bool cachedPathMatchesPinnedChild(const MMKVPath_t &cachedPath,
                                         MMKVFileHandle_t parentFD,
                                         const MMKVPath_t &childName) {
    if (parentFD == MMKVFileHandleInvalidValue || filename(cachedPath) != childName) {
        return false;
    }
    ScopedFileHandle cachedParent(openDirectoryHandle(parentDirectory(cachedPath)));
    return cachedParent.isValid() && isSameFile(cachedParent.get(), parentFD);
}

#ifdef MMKV_ANDROID
static bool regularFileExistsInDir(MMKVFileHandle_t dirFD,
                                   const MMKVPath_t &dirPath,
                                   const MMKVPath_t &fileName) {
    auto fileFD = openRegularFileInDir(dirFD, dirPath, fileName);
    if (fileFD == MMKVFileHandleInvalidValue) {
        return false;
    }
    closeFileHandle(fileFD);
    return true;
}

static bool regularFilePairExistsInDir(MMKVFileHandle_t dirFD,
                                       const MMKVPath_t &dirPath,
                                       const MMKVPath_t &fileName) {
    MMKVFileHandle_t dataFD = MMKVFileHandleInvalidValue;
    MMKVFileHandle_t crcFD = MMKVFileHandleInvalidValue;
    if (!openRegularFilePairInDir(dirFD, dirPath, fileName, fileName + CRC_SUFFIX, dataFD, crcFD)) {
        return false;
    }
    closeFileHandle(dataFD);
    closeFileHandle(crcFD);
    return true;
}
#endif

bool MMKV::backupOneToDirectoryWithHandles(const string &mmapKey,
                                           const MMKVPath_t &dstPath,
                                           const MMKVPath_t &srcPath,
                                           bool compareFullPath,
                                           MMKVFileHandle_t srcDirFD,
                                           const MMKVPath_t &srcDirPath,
                                           const MMKVPath_t &srcName,
                                           MMKVFileHandle_t dstDirFD,
                                           const MMKVPath_t &dstDirPath,
                                           const MMKVPath_t &dstName) {
    if (!g_instanceLock) {
        return false;
    }

    MMKVFileHandle_t srcFD = MMKVFileHandleInvalidValue;
    MMKVFileHandle_t srcCRCFD = MMKVFileHandleInvalidValue;
    if (!openRegularFilePairInDir(srcDirFD, srcDirPath, srcName, srcName + CRC_SUFFIX, srcFD, srcCRCFD)) {
        return false;
    }
    ScopedFileHandle srcFile(srcFD);
    ScopedFileHandle srcCRCFile(srcCRCFD);

    // we have to lock the creation of MMKV instance, regardless of in cache or not
    SCOPED_LOCK(g_instanceLock);
    auto matchesCachedHint = [&](const auto &entry) {
        return entry.second &&
               (!compareFullPath
                    ? entry.first == mmapKey
                    : cachedPathMatchesPinnedChild(entry.second->m_path, srcDirFD, srcName));
    };
    auto hasCachedHint = any_of(g_instanceDic->begin(), g_instanceDic->end(), matchesCachedHint);
    // Cache keys and paths are only lexical hints. Android legacy names,
    // aliases, and directory renames can all make them disagree with the
    // actual mapped files. Scan the cache and compare the pinned source pair
    // with identities captured from the exact handles used for mmap().
    for (const auto &entry : *g_instanceDic) {
        // A lexical hit names the caller's intended live instance. If its
        // files were replaced by another cached instance's pair, selecting
        // that second instance by identity would turn backup(A) into backup(B).
        if (hasCachedHint && !matchesCachedHint(entry)) {
            continue;
        }
        auto kv = entry.second;
        if (!kv) {
            continue;
        }
        SCOPED_LOCK(kv->m_lock);
        auto dataMatches = kv->m_file->isMappedFile(srcFile.get());
        if (!dataMatches || !kv->m_metaFile->isMappedFile(srcCRCFile.get())) {
            continue;
        }
        SCOPED_LOCK(kv->m_sharedProcessLock);
        const auto &srcUTF8Path = MMKVPath_t2String(srcPath);
        const auto &dstUTF8Path = MMKVPath_t2String(dstPath);
        MMKVInfo("backup one cached mmkv[%s] from [%s] to [%s]", mmapKey.c_str(), srcUTF8Path.c_str(),
                 dstUTF8Path.c_str());
        MMKVFileHandle_t writableSrcFD = MMKVFileHandleInvalidValue;
        MMKVFileHandle_t writableSrcCRCFD = MMKVFileHandleInvalidValue;
        auto syncDataFD = srcFile.get();
        auto syncMetaFD = srcCRCFile.get();
        if (!kv->isReadOnly()) {
            if (!openRegularFilePairInDir(srcDirFD, srcDirPath, srcName, srcName + CRC_SUFFIX,
                                          writableSrcFD, writableSrcCRCFD, true)) {
                return false;
            }
        }
        ScopedFileHandle writableSrcFile(writableSrcFD);
        ScopedFileHandle writableSrcCRCFile(writableSrcCRCFD);
        if (!kv->isReadOnly() &&
            (!isSameFile(srcFile.get(), writableSrcFile.get()) ||
             !isSameFile(srcCRCFile.get(), writableSrcCRCFile.get()))) {
            return false;
        }
        if (!kv->isReadOnly()) {
            syncDataFD = writableSrcFile.get();
            syncMetaFD = writableSrcCRCFile.get();
        }
        if (!syncMappedPairToPinnedHandles(kv->m_file, kv->m_metaFile, syncDataFD, syncMetaFD,
                                           !kv->isReadOnly())) {
            return false;
        }
        auto ret = copyFilePair(srcFile.get(), srcCRCFile.get(), dstDirFD, dstDirPath, dstName,
                                dstName + CRC_SUFFIX);
        MMKVInfo("finish backup one mmkv[%s], ret: %d", mmapKey.c_str(), ret);
        return ret;
    }

    if (hasCachedHint) {
        MMKVError("refuse to back up replacement pair for live mmkv[%s]", mmapKey.c_str());
        return false;
    }

    FileLock fileLock(srcCRCFile.get());
    InterProcessLock lock(&fileLock, SharedLockType);
    SCOPED_LOCK(&lock);
    return copyFilePair(srcFile.get(), srcCRCFile.get(), dstDirFD, dstDirPath, dstName, dstName + CRC_SUFFIX);
}

bool MMKV::backupOneToDirectory(const string &mmapKey,
                                const MMKVPath_t &dstPath,
                                const MMKVPath_t &srcPath,
                                bool compareFullPath) {
    auto srcDirPath = parentDirectory(srcPath);
    auto dstDirPath = parentDirectory(dstPath);
    ScopedFileHandle srcDir(openDirectoryHandle(srcDirPath));
    ScopedFileHandle dstDir(openOrCreateDirectoryHandle(dstDirPath));
    if (!srcDir.isValid() || !dstDir.isValid()) {
        return false;
    }
    if (isSameFile(srcDir.get(), dstDir.get()) && filename(srcPath) == filename(dstPath)) {
        return true;
    }
    return backupOneToDirectoryWithHandles(mmapKey, dstPath, srcPath, compareFullPath, srcDir.get(), srcDirPath,
                                           filename(srcPath), dstDir.get(), dstDirPath, filename(dstPath));
}

bool MMKV::backupOneToDirectory(const string &mmapID, const MMKVPath_t &dstDir, const MMKVPath_t *srcDir) {
    auto rootPath = srcDir ? srcDir : &g_realRootDir;
    if (!isValidDirectoryPath(*rootPath) || !isValidDirectoryPath(dstDir)) {
        return false;
    }
    ScopedFileHandle requestedRoot(openDirectoryHandle(*rootPath));
    if (!requestedRoot.isValid()) {
        return false;
    }
    auto ns = nameSpace(*rootPath);
    rootPath = &ns.getRootDir();
    ScopedFileHandle resolvedRoot(openDirectoryHandle(*rootPath));
    if (!resolvedRoot.isValid() || !isSameFile(requestedRoot.get(), resolvedRoot.get())) {
        return false;
    }
    ScopedFileHandle dstRoot(openOrCreateDirectoryHandle(dstDir));
    if (!dstRoot.isValid()) {
        return false;
    }
    if (isSameFile(resolvedRoot.get(), dstRoot.get())) {
        return true;
    }

    // Keep the caller's lexical destination path. absolutePath() follows
    // symlinks before the handle-relative no-follow traversal gets a chance
    // to reject them.
    auto dstPath = mappedKVPathWithIDWithoutCreating(mmapID, dstDir);
    string mmapKey = mmapedKVKey(mmapID, rootPath, true);
    auto srcInSpecialDirectory = mmapIDHasSpecialCharacter(mmapID);
    auto srcPath = mappedKVPathWithIDWithoutCreating(mmapID, *rootPath);
    ScopedFileHandle srcSpecialDir(srcInSpecialDirectory
                                       ? openDirectoryInDir(resolvedRoot.get(), *rootPath,
                                                            SPECIAL_CHARACTER_DIRECTORY_NAME, false)
                                       : MMKVFileHandleInvalidValue);
    auto srcParentFD = srcInSpecialDirectory ? srcSpecialDir.get() : resolvedRoot.get();
#ifdef MMKV_ANDROID
    auto currentSourceExists = (!srcInSpecialDirectory || srcSpecialDir.isValid()) &&
                               regularFileExistsInDir(srcParentFD, parentDirectory(srcPath), filename(srcPath));
    if (!currentSourceExists) {
        auto legacyID = legacyMmapedKVKey(mmapID, rootPath);
        auto legacyPath = mappedKVPathWithIDWithoutCreating(legacyID, *rootPath);
        if (!regularFilePairExistsInDir(resolvedRoot.get(), *rootPath, filename(legacyPath))) {
            MMKVWarning("file with ID [%s] not exist in path [%s]", mmapID.c_str(), rootPath->c_str());
            return false;
        }
        srcPath = std::move(legacyPath);
        srcInSpecialDirectory = false;
        srcParentFD = resolvedRoot.get();
    }
#else
    if (srcInSpecialDirectory && !srcSpecialDir.isValid()) {
        return false;
    }
#endif
    auto dstInSpecialDirectory = mmapIDHasSpecialCharacter(mmapID);
    auto srcDirPath = parentDirectory(srcPath);
    auto dstDirPath = parentDirectory(dstPath);
    ScopedFileHandle dstSpecialDir(dstInSpecialDirectory
                                       ? openDirectoryInDir(dstRoot.get(), dstDir, SPECIAL_CHARACTER_DIRECTORY_NAME,
                                                            true)
                                       : MMKVFileHandleInvalidValue);
    if (dstInSpecialDirectory && !dstSpecialDir.isValid()) {
        return false;
    }
    auto dstParentFD = dstInSpecialDirectory ? dstSpecialDir.get() : dstRoot.get();
    return backupOneToDirectoryWithHandles(mmapKey, dstPath, srcPath, false, srcParentFD, srcDirPath,
                                           filename(srcPath), dstParentFD, dstDirPath, filename(dstPath));
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

static MMKVPath_t pathByAppendingComponent(const MMKVPath_t &dirPath, const MMKVPath_t &component) {
    auto path = dirPath;
    const auto slash = MMKV_PATH_SLASH[0];
    if (path.empty() || path.back() != slash) {
        path.push_back(slash);
    }
    path += component;
    return path;
}

size_t MMKV::backupAllToDirectoryWithHandles(const MMKVPath_t &dstDir,
                                             const MMKVPath_t &srcDir,
                                             bool isInSpecialDir,
                                             MMKVFileHandle_t srcDirFD,
                                             MMKVFileHandle_t dstDirFD) {
    if (!isValidDirectoryPath(srcDir) || !isValidDirectoryPath(dstDir) ||
        srcDirFD == MMKVFileHandleInvalidValue || dstDirFD == MMKVFileHandleInvalidValue) {
        return 0;
    }
    if (isSameFile(srcDirFD, dstDirFD)) {
        return true;
    }
    unordered_set<MMKVPath_t> regularBasenames;
    auto walked = walkInOpenedDir(srcDirFD, srcDir, WalkFile, [&](const MMKVPath_t &basename, WalkType) {
        regularBasenames.insert(basename);
    });
    if (!walked) {
        return 0;
    }

    size_t count = 0;
    if (!regularBasenames.empty()) {
        auto compareFullPath = isInSpecialDir;
        for (const auto &basename : regularBasenames) {
            auto crcBasename = basename + CRC_SUFFIX;
            if (regularBasenames.find(crcBasename) == regularBasenames.end()) {
                // A metadata basename is also a valid data basename for an
                // mmapID ending in CRC_SUFFIX. Treat it as data only when its
                // own metadata sibling exists; otherwise it is the metadata
                // belonging to the basename without the suffix.
                if (!endsWith(basename, CRC_SUFFIX)) {
                    auto srcCRCPath = pathByAppendingComponent(srcDir, crcBasename);
                    const auto &utf8SrcCRCPath = MMKVPath_t2String(srcCRCPath);
                    MMKVWarning("crc not exist [%s]", utf8SrcCRCPath.c_str());
                }
                continue;
            }

            auto srcPath = pathByAppendingComponent(srcDir, basename);
            const auto &strBasename = MMKVPath_t2String(basename);
            auto mmapKey = isInSpecialDir ? strBasename : mmapedKVKey(strBasename, &srcDir);
            auto dstPath = pathByAppendingComponent(dstDir, basename);
            if (backupOneToDirectoryWithHandles(mmapKey, dstPath, srcPath, compareFullPath, srcDirFD, srcDir,
                                                basename, dstDirFD, dstDir, basename)) {
                count++;
            }
        }
    }
    return count;
}

size_t MMKV::backupAllToDirectory(const MMKVPath_t &dstDir, const MMKVPath_t *srcDir) {
    auto rootPath = srcDir ? srcDir : &g_realRootDir;
    if (!isValidDirectoryPath(*rootPath) || !isValidDirectoryPath(dstDir)) {
        return 0;
    }
    ScopedFileHandle srcRoot(openDirectoryHandle(*rootPath));
    if (!srcRoot.isValid()) {
        return 0;
    }
    ScopedFileHandle dstRoot(openOrCreateDirectoryHandle(dstDir));
    if (!dstRoot.isValid()) {
        return 0;
    }
    if (isSameFile(srcRoot.get(), dstRoot.get())) {
        return true;
    }
    auto count = backupAllToDirectoryWithHandles(dstDir, *rootPath, false, srcRoot.get(), dstRoot.get());

    ScopedFileHandle specialSrcRoot(
        openDirectoryInDir(srcRoot.get(), *rootPath, SPECIAL_CHARACTER_DIRECTORY_NAME, false));
    if (specialSrcRoot.isValid()) {
        ScopedFileHandle specialDstRoot(
            openDirectoryInDir(dstRoot.get(), dstDir, SPECIAL_CHARACTER_DIRECTORY_NAME, true));
        if (specialDstRoot.isValid()) {
            auto specialSrcDir = pathByAppendingComponent(*rootPath, SPECIAL_CHARACTER_DIRECTORY_NAME);
            auto specialDstDir = pathByAppendingComponent(dstDir, SPECIAL_CHARACTER_DIRECTORY_NAME);
            count += backupAllToDirectoryWithHandles(specialDstDir, specialSrcDir, true, specialSrcRoot.get(),
                                                     specialDstRoot.get());
        }
    }
    return count;
}

// restore

// We can't simply replace the existing file, because other processes might have already open it.
// They won't know a difference when the file has been replaced.
// We have to let them know by overriding the existing file with new content.
bool MMKV::restoreOneFromDirectoryWithHandles(const string &mmapKey,
                                              const MMKVPath_t &srcPath,
                                              const MMKVPath_t &dstPath,
                                              bool compareFullPath,
                                              MMKVFileHandle_t srcDirFD,
                                              const MMKVPath_t &srcDirPath,
                                              const MMKVPath_t &srcName,
                                              MMKVFileHandle_t dstDirFD,
                                              const MMKVPath_t &dstDirPath,
                                              const MMKVPath_t &dstName) {
    if (!g_instanceLock) {
        return false;
    }
    MMKVFileHandle_t srcFD = MMKVFileHandleInvalidValue;
    MMKVFileHandle_t srcCRCFD = MMKVFileHandleInvalidValue;
    if (!openRegularFilePairInDir(srcDirFD, srcDirPath, srcName, srcName + CRC_SUFFIX, srcFD, srcCRCFD)) {
        return false;
    }
    ScopedFileHandle srcFile(srcFD);
    ScopedFileHandle srcCRCFile(srcCRCFD);

    // Snapshot both sources before touching a live destination. Pinned handles
    // prevent name replacement. The CRC file is MMKV's interprocess lock file;
    // holding its shared lock closes the data/metadata mutation window while
    // both source snapshots are taken.
    ScopedFileHandle stagedSrcFile(createTemporaryFileInDir(dstDirFD, dstDirPath));
    ScopedFileHandle stagedSrcCRCFile(createTemporaryFileInDir(dstDirFD, dstDirPath));
    if (!stagedSrcFile.isValid() || !stagedSrcCRCFile.isValid()) {
        return false;
    }
    FileLock sourceFileLock(srcCRCFile.get());
    InterProcessLock sourceLock(&sourceFileLock, SharedLockType);
    {
        SCOPED_LOCK(&sourceLock);
        if (!copyFileContent(srcFile.get(), stagedSrcFile.get()) ||
            !copyFileContent(srcCRCFile.get(), stagedSrcCRCFile.get())) {
            return false;
        }
    }

    MMKVMetaInfo sourceMetaInfo;
    if (!readFileContent(stagedSrcCRCFile.get(), &sourceMetaInfo, sizeof(sourceMetaInfo))) {
        return false;
    }

    MMKVFileHandle_t dstFD = MMKVFileHandleInvalidValue;
    MMKVFileHandle_t dstCRCFD = MMKVFileHandleInvalidValue;
    bool dstCreated = false;
    bool dstCRCCreated = false;
    if (!openOrCreateRegularFilePairInDir(dstDirFD, dstDirPath, dstName, dstName + CRC_SUFFIX, dstFD, dstCRCFD,
                                          dstCreated, dstCRCCreated)) {
        return false;
    }
    ScopedFileHandle dstFile(dstFD);
    ScopedFileHandle dstCRCFile(dstCRCFD);

    // we have to lock the creation of MMKV instance, regardless of in cache or not
    SCOPED_LOCK(g_instanceLock);
    auto matchesCachedHint = [&](const auto &entry) {
        return entry.second &&
               (!compareFullPath
                    ? entry.first == mmapKey
                    : cachedPathMatchesPinnedChild(entry.second->m_path, dstDirFD, dstName));
    };
    auto hasCachedHint = any_of(g_instanceDic->begin(), g_instanceDic->end(), matchesCachedHint);
    // Recognize live destinations by the exact mapped data/CRC identities,
    // independent of cache-key spelling or subsequent path/root renames.
    for (const auto &entry : *g_instanceDic) {
        if (hasCachedHint && !matchesCachedHint(entry)) {
            continue;
        }
        auto kv = entry.second;
        if (!kv) {
            continue;
        }
        SCOPED_LOCK(kv->m_lock);
        auto dataMatches = kv->m_file->isMappedFile(dstFile.get());
        if (!dataMatches || !kv->m_metaFile->isMappedFile(dstCRCFile.get())) {
            continue;
        }
        SCOPED_LOCK(kv->m_exclusiveProcessLock);
        const auto &srcUTF8Path = MMKVPath_t2String(srcPath);
        const auto &dstUTF8Path = MMKVPath_t2String(dstPath);
        MMKVInfo("restore one cached mmkv[%s] from [%s] to [%s]", mmapKey.c_str(), srcUTF8Path.c_str(),
                 dstUTF8Path.c_str());
        if (!kv->m_metaFile->isFileValid() ||
            !syncMappedPairToPinnedHandles(kv->m_file, kv->m_metaFile, dstFile.get(), dstCRCFile.get())) {
            return false;
        }

        ScopedFileHandle savedData(createTemporaryFileInDir(dstDirFD, dstDirPath));
        ScopedFileHandle savedCRC(createTemporaryFileInDir(dstDirFD, dstDirPath));
        if (!savedData.isValid() || !savedCRC.isValid() ||
            !copyFileContent(dstFile.get(), savedData.get()) ||
            !copyFileContent(dstCRCFile.get(), savedCRC.get())) {
            return false;
        }
        MMKVMetaInfo savedMetaInfo;
        memcpy(&savedMetaInfo, kv->m_metaFile->getMemory(), sizeof(savedMetaInfo));

        auto ret = copyFileContent(stagedSrcFile.get(), dstFile.get());
        auto metaAttempted = false;
        if (ret) {
            // The data bytes must be durable before metadata can publish
            // the source digest/size as the current committed contents.
            ret = syncFile(dstFile.get());
        }
        if (ret) {
            // Copy every source CRC byte that exists, while retaining the
            // live mapping's established file size for short legacy CRC
            // files. Mark the metadata as attempted before the fallible
            // copy so a partial write is always rolled back.
            metaAttempted = true;
            ret = copyFileContent(stagedSrcCRCFile.get(), dstCRCFile.get(), false);
        }
        if (ret) {
            memcpy(kv->m_metaFile->getMemory(), &sourceMetaInfo, sizeof(sourceMetaInfo));
            ret = kv->m_metaFile->msync(MMKV_ASYNC) && syncFile(dstCRCFile.get());
        }
        if (!ret) {
            auto dataRestored = copyFileContent(savedData.get(), dstFile.get());
            auto dataSynced = dataRestored && syncFile(dstFile.get());
            auto metaRestored = true;
            if (metaAttempted) {
                // Preserve the complete old CRC file, including reserved
                // or future trailing bytes. Flush it only after the data
                // rollback has been synchronously persisted.
                metaRestored = dataSynced && copyFileContent(savedCRC.get(), dstCRCFile.get());
                if (metaRestored) {
                    // Keep the live mapping coherent even on platforms
                    // that do not promise immediate WriteFile/mmap view
                    // coherence for writes through a second handle.
                    memcpy(kv->m_metaFile->getMemory(), &savedMetaInfo, sizeof(savedMetaInfo));
                    metaRestored = kv->m_metaFile->msync(MMKV_ASYNC) && syncFile(dstCRCFile.get());
                }
            }
            if (!dataSynced || !metaRestored) {
                MMKVError("failed to roll back cached restore for mmkv[%s]", mmapKey.c_str());
            }
        }
        // Reload from the already pinned destination handle. Reopening m_path
        // here would let a concurrent path/root replacement redirect the live
        // instance after an otherwise handle-bound transaction.
        kv->clearMemoryCache();
        auto reloaded = kv->m_file->reloadFromFileHandle(dstFile.get(), kv->m_expectedCapacity);
        if (reloaded) {
            try {
                kv->loadFromFile();
            } catch (const exception &error) {
                ret = false;
                MMKVError("failed to decode cached restore for mmkv[%s]: %s", mmapKey.c_str(), error.what());
                // loadFromFile() may have partially rebuilt its dictionary or
                // writer. Keep the pinned mapping, clear derived state, and
                // leave a lazy retry that cannot be redirected by m_path.
                kv->clearMemoryCache(true);
            } catch (...) {
                ret = false;
                MMKVError("failed to decode cached restore for mmkv[%s]", mmapKey.c_str());
                kv->clearMemoryCache(true);
            }
        } else {
            ret = false;
            // reloadFromFileHandle() retained the expected file identity. A
            // lazy retry may reopen only that same file, never a replacement.
            MMKVError("failed to reload cached restore from pinned destination for mmkv[%s]", mmapKey.c_str());
        }
        kv->m_file->cleanMayflyFD(true);
        if (ret && kv->isMultiProcess()) {
            kv->notifyContentChanged();
        }

        MMKVInfo("finish restore one mmkv[%s], ret: %d", mmapKey.c_str(), ret);
        return ret;
    }

    if (hasCachedHint) {
        MMKVError("refuse to restore replacement pair over live mmkv[%s]", mmapKey.c_str());
        return false;
    }

    FileLock fileLock(dstCRCFile.get());
    InterProcessLock lock(&fileLock, ExclusiveLockType);
    SCOPED_LOCK(&lock);
    return copyFileContentPair(stagedSrcFile.get(), stagedSrcCRCFile.get(), dstFile.get(), dstCRCFile.get(),
                               dstDirFD, dstDirPath, dstName, dstName + CRC_SUFFIX, dstCreated, dstCRCCreated);
}

bool MMKV::restoreOneFromDirectory(const string &mmapKey,
                                   const MMKVPath_t &srcPath,
                                   const MMKVPath_t &dstPath,
                                   bool compareFullPath) {
    auto srcDirPath = parentDirectory(srcPath);
    auto dstDirPath = parentDirectory(dstPath);
    ScopedFileHandle srcDir(openDirectoryHandle(srcDirPath));
    ScopedFileHandle dstDir(openOrCreateDirectoryHandle(dstDirPath));
    if (!srcDir.isValid() || !dstDir.isValid()) {
        return false;
    }
    if (isSameFile(srcDir.get(), dstDir.get()) && filename(srcPath) == filename(dstPath)) {
        return true;
    }
    return restoreOneFromDirectoryWithHandles(mmapKey, srcPath, dstPath, compareFullPath, srcDir.get(), srcDirPath,
                                              filename(srcPath), dstDir.get(), dstDirPath, filename(dstPath));
}

bool MMKV::restoreOneFromDirectory(const string &mmapID, const MMKVPath_t &srcDir, const MMKVPath_t *dstDir) {
    auto rootPath = dstDir ? dstDir : &g_realRootDir;
    if (!isValidDirectoryPath(srcDir) || !isValidDirectoryPath(*rootPath)) {
        return false;
    }
    ScopedFileHandle srcRoot(openDirectoryHandle(srcDir));
    if (!srcRoot.isValid()) {
        return false;
    }
    ScopedFileHandle requestedRoot(openOrCreateDirectoryHandle(*rootPath));
    if (!requestedRoot.isValid()) {
        return false;
    }
    auto ns = nameSpace(*rootPath);
    rootPath = &ns.getRootDir();
    ScopedFileHandle resolvedRoot(openDirectoryHandle(*rootPath));
    if (!resolvedRoot.isValid() || !isSameFile(requestedRoot.get(), resolvedRoot.get())) {
        return false;
    }
    if (isSameFile(srcRoot.get(), resolvedRoot.get())) {
        return true;
    }
    auto mmapKey = mmapedKVKey(mmapID, rootPath, true);
    auto srcInSpecialDirectory = mmapIDHasSpecialCharacter(mmapID);
    auto srcPath = mappedKVPathWithIDWithoutCreating(mmapID, srcDir);
    ScopedFileHandle srcSpecialDir(srcInSpecialDirectory
                                       ? openDirectoryInDir(srcRoot.get(), srcDir, SPECIAL_CHARACTER_DIRECTORY_NAME,
                                                            false)
                                       : MMKVFileHandleInvalidValue);
    if (srcInSpecialDirectory && !srcSpecialDir.isValid()) {
        return false;
    }
    auto srcParentFD = srcInSpecialDirectory ? srcSpecialDir.get() : srcRoot.get();

    bool dstInSpecialDirectory = mmapIDHasSpecialCharacter(mmapID);
    auto dstPath = mappedKVPathWithIDWithoutCreating(mmapID, *rootPath);
    ScopedFileHandle existingDstSpecialDir(dstInSpecialDirectory
                                               ? openDirectoryInDir(resolvedRoot.get(), *rootPath,
                                                                    SPECIAL_CHARACTER_DIRECTORY_NAME, false)
                                               : MMKVFileHandleInvalidValue);
#ifdef MMKV_ANDROID
    auto currentDstParentFD = dstInSpecialDirectory ? existingDstSpecialDir.get() : resolvedRoot.get();
    auto currentDestinationExists = (!dstInSpecialDirectory || existingDstSpecialDir.isValid()) &&
                                    regularFileExistsInDir(currentDstParentFD, parentDirectory(dstPath),
                                                           filename(dstPath));
    if (!currentDestinationExists) {
        auto legacyID = legacyMmapedKVKey(mmapID, rootPath);
        auto legacyPath = mappedKVPathWithIDWithoutCreating(legacyID, *rootPath);
        if (regularFilePairExistsInDir(resolvedRoot.get(), *rootPath, filename(legacyPath))) {
            dstPath = std::move(legacyPath);
            dstInSpecialDirectory = false;
        }
    }
#endif
    auto srcDirPath = parentDirectory(srcPath);
    auto dstDirPath = parentDirectory(dstPath);
    ScopedFileHandle createdDstSpecialDir(dstInSpecialDirectory && !existingDstSpecialDir.isValid()
                                              ? openDirectoryInDir(resolvedRoot.get(), *rootPath,
                                                                   SPECIAL_CHARACTER_DIRECTORY_NAME, true)
                                              : MMKVFileHandleInvalidValue);
    if (dstInSpecialDirectory && !existingDstSpecialDir.isValid() && !createdDstSpecialDir.isValid()) {
        return false;
    }
    auto dstParentFD = dstInSpecialDirectory
                           ? (existingDstSpecialDir.isValid() ? existingDstSpecialDir.get() : createdDstSpecialDir.get())
                           : resolvedRoot.get();
    return restoreOneFromDirectoryWithHandles(mmapKey, srcPath, dstPath, false, srcParentFD, srcDirPath,
                                              filename(srcPath), dstParentFD, dstDirPath, filename(dstPath));
}

size_t MMKV::restoreAllFromDirectoryWithHandles(const MMKVPath_t &srcDir,
                                                const MMKVPath_t &dstDir,
                                                bool isInSpecialDir,
                                                MMKVFileHandle_t srcDirFD,
                                                MMKVFileHandle_t dstDirFD) {
    if (!isValidDirectoryPath(srcDir) || !isValidDirectoryPath(dstDir) ||
        srcDirFD == MMKVFileHandleInvalidValue || dstDirFD == MMKVFileHandleInvalidValue) {
        return 0;
    }
    if (isSameFile(srcDirFD, dstDirFD)) {
        return true;
    }
    unordered_set<MMKVPath_t> regularBasenames;
    auto walked = walkInOpenedDir(srcDirFD, srcDir, WalkFile, [&](const MMKVPath_t &basename, WalkType) {
        regularBasenames.insert(basename);
    });
    if (!walked) {
        return 0;
    }

    size_t count = 0;
    if (!regularBasenames.empty()) {
        auto compareFullPath = isInSpecialDir;
        for (const auto &basename : regularBasenames) {
            auto crcBasename = basename + CRC_SUFFIX;
            if (regularBasenames.find(crcBasename) == regularBasenames.end()) {
                if (!endsWith(basename, CRC_SUFFIX)) {
                    auto srcCRCPath = pathByAppendingComponent(srcDir, crcBasename);
                    const auto &utf8SrcCRCPath = MMKVPath_t2String(srcCRCPath);
                    MMKVWarning("crc not exist [%s]", utf8SrcCRCPath.c_str());
                }
                continue;
            }

            auto srcPath = pathByAppendingComponent(srcDir, basename);
            const auto &strBasename = MMKVPath_t2String(basename);
            auto mmapKey = isInSpecialDir ? strBasename : mmapedKVKey(strBasename, &dstDir);
            auto dstPath = pathByAppendingComponent(dstDir, basename);
            if (restoreOneFromDirectoryWithHandles(mmapKey, srcPath, dstPath, compareFullPath, srcDirFD, srcDir,
                                                   basename, dstDirFD, dstDir, basename)) {
                count++;
            }
        }
    }
    return count;
}

size_t MMKV::restoreAllFromDirectory(const MMKVPath_t &srcDir, const MMKVPath_t *dstDir) {
    auto rootPath = dstDir ? dstDir : &g_realRootDir;
    if (!isValidDirectoryPath(srcDir) || !isValidDirectoryPath(*rootPath)) {
        return 0;
    }
    ScopedFileHandle srcRoot(openDirectoryHandle(srcDir));
    if (!srcRoot.isValid()) {
        return 0;
    }
    ScopedFileHandle dstRoot(openOrCreateDirectoryHandle(*rootPath));
    if (!dstRoot.isValid()) {
        return 0;
    }
    if (isSameFile(srcRoot.get(), dstRoot.get())) {
        return true;
    }
    // Match cached instances by their destination path, then verify the pinned
    // data/meta identities before using them. On Android a live legacy instance
    // can have a cache key derived from the original mmapID while its basename
    // is legacyMmapedKVKey(), so a basename-derived cache lookup can miss it and
    // overwrite its files without reloading the live instance.
    auto count = restoreAllFromDirectoryWithHandles(srcDir, *rootPath, true, srcRoot.get(), dstRoot.get());

    ScopedFileHandle specialSrcRoot(openDirectoryInDir(srcRoot.get(), srcDir, SPECIAL_CHARACTER_DIRECTORY_NAME,
                                                       false));
    if (specialSrcRoot.isValid()) {
        ScopedFileHandle specialDstRoot(
            openDirectoryInDir(dstRoot.get(), *rootPath, SPECIAL_CHARACTER_DIRECTORY_NAME, true));
        if (specialDstRoot.isValid()) {
            auto specialSrcDir = pathByAppendingComponent(srcDir, SPECIAL_CHARACTER_DIRECTORY_NAME);
            auto specialDstDir = pathByAppendingComponent(*rootPath, SPECIAL_CHARACTER_DIRECTORY_NAME);
            count += restoreAllFromDirectoryWithHandles(specialSrcDir, specialDstDir, true, specialSrcRoot.get(),
                                                        specialDstRoot.get());
        }
    }
    return count;
}

// callbacks

void MMKV::registerHandler(mmkv::MMKVHandler *handler) {
    if (!g_instanceLock) {
        return;
    }
    SCOPED_LOCK(g_instanceLock);
    g_handler = handler;
}

void MMKV::unRegisterHandler() {
    if (!g_instanceLock) {
        return;
    }
    SCOPED_LOCK(g_instanceLock);
    g_handler = nullptr;
}

void MMKV::setLogLevel(MMKVLogLevel level) {
    if (!g_instanceLock) {
        return;
    }
    SCOPED_LOCK(g_instanceLock);
    g_currentLogLevel = level;
}

static void mkSpecialCharacterFileDirectory() {
    MMKVPath_t path = g_realRootDir + MMKV_PATH_SLASH + SPECIAL_CHARACTER_DIRECTORY_NAME;
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

static bool mmapIDHasSpecialCharacter(const string &mmapID) {
    const char *specialCharacters = "\\/:*?\"<>|";
    for (auto ch : mmapID) {
        if (strchr(specialCharacters, ch) != nullptr) {
            return true;
        }
    }
    return false;
}

static MMKVPath_t encodeFilePathWithoutCreating(const string &mmapID) {
    if (mmapIDHasSpecialCharacter(mmapID)) {
        return MMKVPath_t(SPECIAL_CHARACTER_DIRECTORY_NAME) + MMKV_PATH_SLASH + string2MMKVPath_t(md5(mmapID));
    }
    return string2MMKVPath_t(mmapID);
}

static MMKVPath_t mappedKVPathWithIDWithoutCreating(const string &mmapID, const MMKVPath_t &rootPath) {
    return rootPath + MMKV_PATH_SLASH + encodeFilePathWithoutCreating(mmapID);
}

static MMKVPath_t encodeFilePath(const string &mmapID) {
    if (mmapIDHasSpecialCharacter(mmapID)) {
        static ThreadOnceToken_t once = ThreadOnceUninitialized;
        ThreadLock::ThreadOnce(&once, mkSpecialCharacterFileDirectory);
    }
    return encodeFilePathWithoutCreating(mmapID);
}

static MMKVPath_t encodeFilePath(const string &mmapID, const MMKVPath_t &rootDir) {
    if (mmapIDHasSpecialCharacter(mmapID)) {
        MMKVPath_t path = rootDir + MMKV_PATH_SLASH + SPECIAL_CHARACTER_DIRECTORY_NAME;
        mkPath(path);
    }
    return encodeFilePathWithoutCreating(mmapID);
}

string mmapedKVKey(const string &mmapID, const MMKVPath_t *rootPath, bool alreadyAbsolute) {
    MMKVPath_t path;
    // compare by pointer to speedup a bit, it's OK false detecting
    if (rootPath && (rootPath != &g_realRootDir)) {
        auto tmp = *rootPath + MMKV_PATH_SLASH + string2MMKVPath_t(mmapID);
        if (alreadyAbsolute) {
            path = std::move(tmp);
        } else {
            path = absolutePath(tmp);
        }
    } else {
        path = g_realRootDir + MMKV_PATH_SLASH + string2MMKVPath_t(mmapID);
    }
    return md5(path);
}

string legacyMmapedKVKey(const string &mmapID, const MMKVPath_t *rootPath) {
    if (rootPath && (*rootPath != g_rootDir)) {
        return md5(*rootPath + MMKV_PATH_SLASH + string2MMKVPath_t(mmapID));
    }
    return mmapID;
}

#ifndef MMKV_ANDROID
MMKVPath_t mappedKVPathWithID(const string &mmapID, const MMKVPath_t *rootPath, bool alreadyAbsolute) {
    if (rootPath && (rootPath != &g_realRootDir)) {
        auto path = *rootPath + MMKV_PATH_SLASH + encodeFilePath(mmapID, *rootPath);
        if (alreadyAbsolute) {
            return path;
        } else {
            return absolutePath(path);
        }
    }
    auto path = g_realRootDir + MMKV_PATH_SLASH + encodeFilePath(mmapID);
    return path;
}
#else
MMKVPath_t mappedKVPathWithID(const string &mmapID, const MMKVPath_t *rootPath, MMKVMode mode, bool alreadyAbsolute) {
    if (mode & MMKV_ASHMEM) {
        return ashmemMMKVPathWithID(encodeFilePath(mmapID));
    } else if (rootPath && (rootPath != &g_realRootDir)) {
        auto path = *rootPath + MMKV_PATH_SLASH + encodeFilePath(mmapID, *rootPath);
        if (alreadyAbsolute) {
            return path;
        } else {
            return absolutePath(path);
        }
    }
    auto path = g_realRootDir + MMKV_PATH_SLASH + encodeFilePath(mmapID);
    return path;
}
#endif

MMKVPath_t crcPathWithPath(const MMKVPath_t &kvPath) {
    return kvPath + CRC_SUFFIX;
}

MMKVRecoverStrategic onMMKVCRCCheckFail(const string &mmapID) {
    if (g_handler) {
        return g_handler->onMMKVCRCCheckFail(mmapID);
    }
    return OnErrorDiscard;
}

MMKVRecoverStrategic onMMKVFileLengthError(const string &mmapID) {
    if (g_handler) {
        return g_handler->onMMKVFileLengthError(mmapID);
    }
    return OnErrorDiscard;
}

// NameSpace

NameSpace MMKV::nameSpace(const MMKVPath_t &rootDir) {
    if (!g_instanceLock) {
        ensureMinimalInitialize();
    }

    static ThreadOnceToken_t once = ThreadOnceUninitialized;
    ThreadLock::ThreadOnce(&once, []{
        g_namespaceLock = new ThreadLock;
        g_namespaceLock->initialize();
    });
    SCOPED_LOCK(g_namespaceLock);

    auto itr = g_realRootMap.find(rootDir);
    if (itr == g_realRootMap.end()) {
        auto realRoot = absolutePath(rootDir);
        if (realRoot.ends_with(MMKV_PATH_SLASH)) {
            realRoot.erase(realRoot.size() - 1);
        }
        itr = g_realRootMap.emplace(rootDir, realRoot).first;
    }
    return NameSpace(itr->second);
}

NameSpace MMKV::defaultNameSpace() {
    if (g_rootDir.empty()) {
        MMKVWarning("MMKV has not been initialized, there's no default NameSpace.");
        return NameSpace(MMKVPath_t());
    }
    return NameSpace(g_realRootDir);
}

MMKV *NameSpace::mmkvWithID(const string &mmapID, MMKVMode mode, const string *cryptKey, size_t expectedCapacity, bool aes256) {
    MMKVConfig config;
    config.mode = mode;
#ifndef MMKV_DISABLE_CRYPT
    config.aes256 = aes256;
    config.cryptKey = cryptKey;
#endif
    config.rootPath = &m_rootDir;
    config.expectedCapacity = expectedCapacity;
    return MMKV::getMMKVWithID(mmapID, config);
}

MMKV *NameSpace::mmkvWithID(const string &mmapID, const MMKVConfig &config) {
    if (!config.rootPath || *config.rootPath != m_rootDir) {
        auto newConfig = config;
        newConfig.rootPath = &m_rootDir;
        return MMKV::getMMKVWithID(mmapID, newConfig);
    }
    return MMKV::getMMKVWithID(mmapID, config);
}

bool NameSpace::backupOneToDirectory(const std::string &mmapID, const MMKVPath_t &dstDir) {
    return MMKV::backupOneToDirectory(mmapID, dstDir, &m_rootDir);
}

bool NameSpace::restoreOneFromDirectory(const std::string &mmapID, const MMKVPath_t &srcDir) {
    return MMKV::restoreOneFromDirectory(mmapID, srcDir, &m_rootDir);
}

size_t NameSpace::backupAllToDirectory(const MMKVPath_t &dstDir) {
    return MMKV::backupAllToDirectory(dstDir, &m_rootDir);
}

size_t NameSpace::restoreAllFromDirectory(const MMKVPath_t &srcDir) {
    return MMKV::restoreAllFromDirectory(srcDir, &m_rootDir);
}

bool NameSpace::isFileValid(const std::string &mmapID) {
    return MMKV::isFileValid(mmapID, &m_rootDir);
}

bool NameSpace::removeStorage(const std::string &mmapID) {
    return MMKV::removeStorage(mmapID, &m_rootDir);
}

bool NameSpace::checkExist(const std::string &mmapID) {
    return MMKV::checkExist(mmapID, &m_rootDir);
}

MMKV_NAMESPACE_END
