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

#ifndef MMKV_MMKV_H
#define MMKV_MMKV_H
#ifdef __cplusplus
#include "MMKVPredef.h"

#ifdef MMKV_APPLE

#  include "MMBuffer.h"
#  ifdef MMKV_HAS_CPP20
#    include <span>
#  endif

#else
#  include "MiniPBCoder.h"
#endif

#include <cstdint>
#include <type_traits>
#include <cstring>

namespace mmkv {
class CodedOutputData;
class MemoryFile;
class AESCrypt;
struct MMKVMetaInfo;
class FileLock;
class InterProcessLock;
class ThreadLock;
} // namespace mmkv

MMKV_NAMESPACE_BEGIN

enum MMKVMode : uint32_t {
    MMKV_SINGLE_PROCESS = 1 << 0,
    MMKV_MULTI_PROCESS = 1 << 1,
#ifdef MMKV_ANDROID
    CONTEXT_MODE_MULTI_PROCESS = 1 << 2, // in case someone mistakenly pass Context.MODE_MULTI_PROCESS
    MMKV_ASHMEM = 1 << 3,
    MMKV_BACKUP = 1 << 4,
#endif
    MMKV_READ_ONLY = 1 << 5,
};

static inline MMKVMode operator | (MMKVMode one, MMKVMode other) {
    return static_cast<MMKVMode>(static_cast<uint32_t>(one) | static_cast<uint32_t>(other));
}

#define MMKV_OUT

#ifdef MMKV_HAS_CPP20
template <class T>
struct mmkv_is_vector { static constexpr bool value = false; };
template <class T, class A>
struct mmkv_is_vector<std::vector<T, A>> { static constexpr bool value = true; };
template <class T, size_t S>
struct mmkv_is_vector<std::span<T, S>> { static constexpr bool value = true; };
template <class T>
inline constexpr bool mmkv_is_vector_v = mmkv_is_vector<T>::value;

template <class T>
concept MMKV_SUPPORTED_PRIMITIVE_VALUE_TYPE = std::is_integral_v<T> || std::is_floating_point_v<T>;

template <class T>
concept MMKV_SUPPORTED_POD_VALUE_TYPE = std::is_same_v<T, const char*> || std::is_same_v<T, std::string> ||
    std::is_same_v<T, mmkv::MMBuffer>;

template <class T>
concept MMKV_SUPPORTED_VECTOR_VALUE_TYPE = mmkv_is_vector_v<T> &&
    (MMKV_SUPPORTED_PRIMITIVE_VALUE_TYPE<typename T::value_type> || MMKV_SUPPORTED_POD_VALUE_TYPE<typename T::value_type>);

template <class T>
concept MMKV_SUPPORTED_VALUE_TYPE = MMKV_SUPPORTED_PRIMITIVE_VALUE_TYPE<T> || MMKV_SUPPORTED_POD_VALUE_TYPE<T> ||
    MMKV_SUPPORTED_VECTOR_VALUE_TYPE<T>;
#endif // MMKV_HAS_CPP20

class MMKV {
#ifndef MMKV_ANDROID
    std::string m_mmapKey;
    MMKV(const std::string &mmapID, MMKVMode mode, std::string *cryptKey, MMKVPath_t *rootPath, size_t expectedCapacity = 0);
#else // defined(MMKV_ANDROID)
    mmkv::FileLock *m_fileModeLock;
    mmkv::InterProcessLock *m_sharedProcessModeLock;
    mmkv::InterProcessLock *m_exclusiveProcessModeLock;

    MMKV(const std::string &mmapID, int size, MMKVMode mode, std::string *cryptKey, MMKVPath_t *rootPath, size_t expectedCapacity = 0);

    MMKV(const std::string &mmapID, int ashmemFD, int ashmemMetaFd, std::string *cryptKey = nullptr);
#endif

    ~MMKV();

    std::string m_mmapID;
    const MMKVMode m_mode;
    MMKVPath_t m_path;
    MMKVPath_t m_crcPath;
    mmkv::MMKVMap *m_dic;
    mmkv::MMKVMapCrypt *m_dicCrypt;

    size_t m_expectedCapacity;

    mmkv::MemoryFile *m_file;
    size_t m_actualSize;
    mmkv::CodedOutputData *m_output;

    bool m_needLoadFromFile;
    bool m_hasFullWriteback;

    uint32_t m_crcDigest;
    mmkv::MemoryFile *m_metaFile;
    mmkv::MMKVMetaInfo *m_metaInfo;

    mmkv::AESCrypt *m_crypter;

    mmkv::ThreadLock *m_lock;
    mmkv::FileLock *m_fileLock;
    mmkv::InterProcessLock *m_sharedProcessLock;
    mmkv::InterProcessLock *m_exclusiveProcessLock;

    bool m_enableKeyExpire = false;
    uint32_t m_expiredInSeconds = ExpireNever;

    bool m_enableCompareBeforeSet = false;

#ifdef MMKV_APPLE
    using MMKVKey_t = NSString *__unsafe_unretained;
    static bool isKeyEmpty(MMKVKey_t key) { return key.length <= 0; }
#  define mmkv_key_length(key) key.length
#  define mmkv_retain_key(key) [key retain]
#  define mmkv_release_key(key) [key release]
#else
    using MMKVKey_t = std::string_view;
    static bool isKeyEmpty(MMKVKey_t key) { return key.empty(); }
#  define mmkv_key_length(key) key.length()
#  define mmkv_retain_key(key) ((void) 0)
#  define mmkv_release_key(key) ((void) 0)
#endif // !MMKV_APPLE

    void loadFromFile();

    void partialLoadFromFile();

    void loadMetaInfoAndCheck();

    void checkDataValid(bool &loadFromFile, bool &needFullWriteback);

    void checkLoadData();

    bool isFileValid();

    bool checkFileCRCValid(size_t actualSize, uint32_t crcDigest);

    void recalculateCRCDigestWithIV(const void *iv);
    void recalculateCRCDigestOnly();

    void updateCRCDigest(const uint8_t *ptr, size_t length);

    size_t readActualSize();

    void oldStyleWriteActualSize(size_t actualSize);

    bool writeActualSize(size_t size, uint32_t crcDigest, const void *iv, bool increaseSequence);

    bool ensureMemorySize(size_t newSize);

    bool expandAndWriteBack(size_t newSize, std::pair<mmkv::MMBuffer, size_t> preparedData, bool needSync = true);

    bool fullWriteback(mmkv::AESCrypt *newCrypter = nullptr, bool onlyWhileExpire = false);

    bool doFullWriteBack(std::pair<mmkv::MMBuffer, size_t> preparedData, mmkv::AESCrypt *newCrypter, bool needSync = true);

    bool doFullWriteBack(mmkv::MMKVVector &&vec);

    mmkv::MMBuffer getRawDataForKey(MMKVKey_t key);

    mmkv::MMBuffer getDataForKey(MMKVKey_t key);

    // isDataHolder: avoid memory copying
    bool setDataForKey(mmkv::MMBuffer &&data, MMKVKey_t key, bool isDataHolder = false);
#ifndef MMKV_APPLE
    bool setDataForKey(mmkv::MMBuffer &&data, MMKVKey_t key, uint32_t expireDuration);
#endif

    bool removeDataForKey(MMKVKey_t key);

    using KVHolderRet_t = std::pair<bool, mmkv::KeyValueHolder>;
    // isDataHolder: avoid memory copying
    KVHolderRet_t doAppendDataWithKey(const mmkv::MMBuffer &data, const mmkv::MMBuffer &key, bool isDataHolder, uint32_t keyLength);
    KVHolderRet_t appendDataWithKey(const mmkv::MMBuffer &data, MMKVKey_t key, bool isDataHolder = false);
    KVHolderRet_t appendDataWithKey(const mmkv::MMBuffer &data, const mmkv::KeyValueHolder &kvHolder, bool isDataHolder = false);

    KVHolderRet_t doOverrideDataWithKey(const mmkv::MMBuffer &data, const mmkv::MMBuffer &key, bool isDataHolder, uint32_t keyLength);
    KVHolderRet_t overrideDataWithKey(const mmkv::MMBuffer &data, const mmkv::KeyValueHolder &kvHolder, bool isDataHolder = false);
    KVHolderRet_t overrideDataWithKey(const mmkv::MMBuffer &data, MMKVKey_t key, bool isDataHolder = false);
    bool checkSizeForOverride(size_t size);
#ifdef MMKV_APPLE
    KVHolderRet_t appendDataWithKey(const mmkv::MMBuffer &data,
                                    MMKVKey_t key,
                                    const mmkv::KeyValueHolderCrypt &kvHolder,
                                    bool isDataHolder = false);
    KVHolderRet_t overrideDataWithKey(const mmkv::MMBuffer &data,
                                      MMKVKey_t key,
                                      const mmkv::KeyValueHolderCrypt &kvHolder,
                                      bool isDataHolder = false);
#endif

    void notifyContentChanged();

#if defined(MMKV_ANDROID) && !defined(MMKV_DISABLE_CRYPT)
    void checkReSetCryptKey(int fd, int metaFD, std::string *cryptKey);
#endif
    static bool backupOneToDirectory(const std::string &mmapKey, const MMKVPath_t &dstPath, const MMKVPath_t &srcPath, bool compareFullPath);
    static size_t backupAllToDirectory(const MMKVPath_t &dstDir, const MMKVPath_t &srcDir, bool isInSpecialDir);
    static bool restoreOneFromDirectory(const std::string &mmapKey, const MMKVPath_t &srcPath, const MMKVPath_t &dstPath, bool compareFullPath);
    static size_t restoreAllFromDirectory(const MMKVPath_t &srcDir, const MMKVPath_t &dstDir, bool isInSpecialDir);

    static uint32_t getCurrentTimeInSecond();
    uint32_t getExpireTimeForKey(MMKVKey_t key);
    mmkv::MMBuffer getDataWithoutMTimeForKey(MMKVKey_t key);
    size_t filterExpiredKeys();

#ifndef MMKV_APPLE
    static constexpr uint32_t ConstFixed32Size = 4;
    void shared_lock();
    void shared_unlock();
#endif

public:
    // call this before getting any MMKV instance
    static void initializeMMKV(const MMKVPath_t &rootDir, MMKVLogLevel logLevel = MMKVLogInfo, mmkv::LogHandler handler = nullptr);

    // a generic purpose instance
    static MMKV *defaultMMKV(MMKVMode mode = MMKV_SINGLE_PROCESS, std::string *cryptKey = nullptr);

#ifndef MMKV_ANDROID

    // mmapID: any unique ID (com.tencent.xin.pay, etc.)
    // if you want a per-user mmkv, you could merge user-id within mmapID
    // cryptKey: 16 bytes at most
    static MMKV *mmkvWithID(const std::string &mmapID,
                            MMKVMode mode = MMKV_SINGLE_PROCESS,
                            std::string *cryptKey = nullptr,
                            MMKVPath_t *rootPath = nullptr,
                            size_t expectedCapacity = 0);

#else // defined(MMKV_ANDROID)

    // mmapID: any unique ID (com.tencent.xin.pay, etc.)
    // if you want a per-user mmkv, you could merge user-id within mmapID
    // cryptKey: 16 bytes at most
    static MMKV *mmkvWithID(const std::string &mmapID,
                            int size = mmkv::DEFAULT_MMAP_SIZE,
                            MMKVMode mode = MMKV_SINGLE_PROCESS,
                            std::string *cryptKey = nullptr,
                            MMKVPath_t *rootPath = nullptr,
                            size_t expectedCapacity = 0);

    static MMKV *mmkvWithAshmemFD(const std::string &mmapID, int fd, int metaFD, std::string *cryptKey = nullptr);

    int ashmemFD();

    int ashmemMetaFD();

    bool checkProcessMode();
#endif // MMKV_ANDROID

    // you can call this on application termination, it's totally fine if you don't call
    static void onExit();

    const std::string &mmapID() const;
#ifndef MMKV_ANDROID
    bool isMultiProcess() const { return  (m_mode & MMKV_MULTI_PROCESS) != 0; }
#else
    bool isMultiProcess() const {
        return (m_mode & MMKV_MULTI_PROCESS) != 0
            || (m_mode & CONTEXT_MODE_MULTI_PROCESS) != 0
            || (m_mode & MMKV_ASHMEM) != 0; // ashmem is always multi-process
    }
#endif
    bool isReadOnly() const { return (m_mode & MMKV_READ_ONLY) != 0; }

#ifndef MMKV_DISABLE_CRYPT
    std::string cryptKey() const;

    // transform plain text into encrypted text, or vice versa with empty cryptKey
    // you can change existing crypt key with different cryptKey
    bool reKey(const std::string &cryptKey);

    // just reset cryptKey (will not encrypt or decrypt anything)
    // usually you should call this method after other process reKey() the multi-process mmkv
    void checkReSetCryptKey(const std::string *cryptKey);
#endif

    bool set(bool value, MMKVKey_t key);
    bool set(bool value, MMKVKey_t key, uint32_t expireDuration);

    bool set(int32_t value, MMKVKey_t key);
    bool set(int32_t value, MMKVKey_t key, uint32_t expireDuration);

    bool set(uint32_t value, MMKVKey_t key);
    bool set(uint32_t value, MMKVKey_t key, uint32_t expireDuration);

    bool set(int64_t value, MMKVKey_t key);
    bool set(int64_t value, MMKVKey_t key, uint32_t expireDuration);

    bool set(uint64_t value, MMKVKey_t key);
    bool set(uint64_t value, MMKVKey_t key, uint32_t expireDuration);

    bool set(float value, MMKVKey_t key);
    bool set(float value, MMKVKey_t key, uint32_t expireDuration);

    bool set(double value, MMKVKey_t key);
    bool set(double value, MMKVKey_t key, uint32_t expireDuration);

#ifdef MMKV_HAS_CPP20
    // avoid unexpected type conversion (pointer to bool, etc.)
    template <typename T>
    requires(!MMKV_SUPPORTED_VALUE_TYPE<T>)
    bool set(T value, MMKVKey_t key) = delete;

    // avoid unexpected type conversion (pointer to bool, etc.)
    template <typename T>
    requires(!MMKV_SUPPORTED_VALUE_TYPE<T>)
    bool set(T value, MMKVKey_t key, uint32_t expireDuration) = delete;
#else
    // avoid unexpected type conversion (pointer to bool, etc.)
    template <typename T>
    bool set(T value, MMKVKey_t key, uint32_t expireDuration) = delete;
#endif

#ifdef MMKV_APPLE
    bool set(NSObject<NSCoding> *__unsafe_unretained obj, MMKVKey_t key);
    bool set(NSObject<NSCoding> *__unsafe_unretained obj, MMKVKey_t key, uint32_t expireDuration);

    NSObject *getObject(MMKVKey_t key, Class cls);
#else  // !defined(MMKV_APPLE)
    bool set(const char *value, MMKVKey_t key);
    bool set(const char *value, MMKVKey_t key, uint32_t expireDuration);

    bool set(const std::string &value, MMKVKey_t key);
    bool set(const std::string &value, MMKVKey_t key, uint32_t expireDuration);

    bool set(std::string_view value, MMKVKey_t key);
    bool set(std::string_view value, MMKVKey_t key, uint32_t expireDuration);

    bool set(const mmkv::MMBuffer &value, MMKVKey_t key);
    bool set(const mmkv::MMBuffer &value, MMKVKey_t key, uint32_t expireDuration);

    bool set(const std::vector<std::string> &vector, MMKVKey_t key);
    bool set(const std::vector<std::string> &vector, MMKVKey_t key, uint32_t expireDuration);

#ifdef MMKV_HAS_CPP20
    template<MMKV_SUPPORTED_VECTOR_VALUE_TYPE T>
    bool set(const T& value, MMKVKey_t key) {
        return set<T>(value, key, m_expiredInSeconds);
    }

    template<MMKV_SUPPORTED_VECTOR_VALUE_TYPE T>
    bool set(const T& value, MMKVKey_t key, uint32_t expireDuration);

    template<MMKV_SUPPORTED_VECTOR_VALUE_TYPE T>
    bool getVector(MMKVKey_t key, T &result);
#endif

    // inplaceModification is recommended for faster speed
    bool getString(MMKVKey_t key, std::string &result, bool inplaceModification = true);

    mmkv::MMBuffer getBytes(MMKVKey_t key);

    bool getBytes(MMKVKey_t key, mmkv::MMBuffer &result);

    bool getVector(MMKVKey_t key, std::vector<std::string> &result);
#endif // MMKV_APPLE

    bool getBool(MMKVKey_t key, bool defaultValue = false, MMKV_OUT bool *hasValue = nullptr);

    int32_t getInt32(MMKVKey_t key, int32_t defaultValue = 0, MMKV_OUT bool *hasValue = nullptr);

    uint32_t getUInt32(MMKVKey_t key, uint32_t defaultValue = 0, MMKV_OUT bool *hasValue = nullptr);

    int64_t getInt64(MMKVKey_t key, int64_t defaultValue = 0, MMKV_OUT bool *hasValue = nullptr);

    uint64_t getUInt64(MMKVKey_t key, uint64_t defaultValue = 0, MMKV_OUT bool *hasValue = nullptr);

    float getFloat(MMKVKey_t key, float defaultValue = 0, MMKV_OUT bool *hasValue = nullptr);

    double getDouble(MMKVKey_t key, double defaultValue = 0, MMKV_OUT bool *hasValue = nullptr);

    // return the actual size consumption of the key's value
    // pass actualSize = true to get value's length
    size_t getValueSize(MMKVKey_t key, bool actualSize);

    // return size written into buffer
    // return -1 on any error
    int32_t writeValueToBuffer(MMKVKey_t key, void *ptr, int32_t size);

    bool containsKey(MMKVKey_t key);

    // filterExpire: return count of all non-expired keys, keep in mind it comes with cost
    size_t count(bool filterExpire = false);

    size_t totalSize();

    size_t actualSize();

    static constexpr uint32_t ExpireNever = 0;

    // all keys created (or last modified) longer than expiredInSeconds will be deleted on next full-write-back
    // expiredInSeconds = MMKV::ExpireNever (0) means no common expiration duration for all keys, aka each key will have it's own expiration duration
    bool enableAutoKeyExpire(uint32_t expiredInSeconds = 0);

    bool disableAutoKeyExpire();

    // compare value for key before set, to reduce the possibility of file expanding
    bool enableCompareBeforeSet();
    bool disableCompareBeforeSet();

    bool isExpirationEnabled() const { return m_enableKeyExpire; }
    bool isEncryptionEnabled() const { return m_dicCrypt; }
    bool isCompareBeforeSetEnabled() const { return m_enableCompareBeforeSet && !m_enableKeyExpire && !m_dicCrypt; }

#ifdef MMKV_APPLE
    // filterExpire: return all non-expired keys, keep in mind it comes with cost
    NSArray *allKeys(bool filterExpire = false);

    bool removeValuesForKeys(NSArray *arrKeys);

    typedef void (^EnumerateBlock)(NSString *key, BOOL *stop);
    void enumerateKeys(EnumerateBlock block);

#    ifdef MMKV_IOS
    static void setIsInBackground(bool isInBackground);
    static bool isInBackground();
#    endif
#else  // !defined(MMKV_APPLE)
    // filterExpire: return all non-expired keys, keep in mind it comes with cost
    std::vector<std::string> allKeys(bool filterExpire = false);

    bool removeValuesForKeys(const std::vector<std::string> &arrKeys);
#endif // MMKV_APPLE

    bool removeValueForKey(MMKVKey_t key);

    // keepSpace: remove all keys but keep the file size not changed, running faster
    void clearAll(bool keepSpace = false);

    // MMKV's size won't reduce after deleting key-values
    // call this method after lots of deleting if you care about disk usage
    // note that `clearAll` has the similar effect of `trim`
    void trim();

    // call this method if the instance is no longer needed in the near future
    // any subsequent call to the instance is undefined behavior
    void close();

    // call this method if you are facing memory-warning
    // any subsequent call to the instance will load all key-values from file again
    // keepSpace: remove all keys but keep the file size not changed, running faster
    void clearMemoryCache(bool keepSpace = false);

    // you don't need to call this, really, I mean it
    // unless you worry about running out of battery
    void sync(SyncFlag flag = MMKV_SYNC);

    // get exclusive access
    void lock();
    void unlock();
    bool try_lock();

    // get thread lock
#ifndef MMKV_WIN32
    void lock_thread();
    void unlock_thread();
    bool try_lock_thread();
#endif

    static const MMKVPath_t &getRootDir();

    // backup one MMKV instance from srcDir to dstDir
    // if srcDir is null, then backup from the root dir of MMKV
    static bool backupOneToDirectory(const std::string &mmapID, const MMKVPath_t &dstDir, const MMKVPath_t *srcDir = nullptr);

    // restore one MMKV instance from srcDir to dstDir
    // if dstDir is null, then restore to the root dir of MMKV
    static bool restoreOneFromDirectory(const std::string &mmapID, const MMKVPath_t &srcDir, const MMKVPath_t *dstDir = nullptr);

    // backup all MMKV instance from srcDir to dstDir
    // if srcDir is null, then backup from the root dir of MMKV
    // return count of MMKV successfully backuped
    static size_t backupAllToDirectory(const MMKVPath_t &dstDir, const MMKVPath_t *srcDir = nullptr);

    // restore all MMKV instance from srcDir to dstDir
    // if dstDir is null, then restore to the root dir of MMKV
    // return count of MMKV successfully restored
    static size_t restoreAllFromDirectory(const MMKVPath_t &srcDir, const MMKVPath_t *dstDir = nullptr);

    // check if content been changed by other process
    void checkContentChanged();

    // called when content is changed by other process
    // doesn't guarantee real-time notification
    static void registerContentChangeHandler(mmkv::ContentChangeHandler handler);
    static void unRegisterContentChangeHandler();

    // by default MMKV will discard all datas on failure
    // return `OnErrorRecover` to recover any data from file
    static void registerErrorHandler(mmkv::ErrorHandler handler);
    static void unRegisterErrorHandler();

    // MMKVLogInfo by default
    // pass MMKVLogNone to disable all logging
    static void setLogLevel(MMKVLogLevel level);

    // by default MMKV will print log to the console
    // implement this method to redirect MMKV's log
    static void registerLogHandler(mmkv::LogHandler handler);
    static void unRegisterLogHandler();

    // detect if the MMKV file is valid or not
    // Note: Don't use this to check the existence of the instance, the return value is undefined if the file was never created.
    static bool isFileValid(const std::string &mmapID, MMKVPath_t *relatePath = nullptr);

    // remove the storage of the MMKV, including the data file & meta file (.crc)
    // Note: the existing instance (if any) will be closed & destroyed
    static bool removeStorage(const std::string &mmapID, MMKVPath_t *relatePath = nullptr);

    // just forbid it for possibly misuse
    explicit MMKV(const MMKV &other) = delete;
    MMKV &operator=(const MMKV &other) = delete;
};

#if defined(MMKV_HAS_CPP20) && !defined(MMKV_APPLE)
template<MMKV_SUPPORTED_VECTOR_VALUE_TYPE T>
bool MMKV::set(const T& value, MMKVKey_t key, uint32_t expireDuration) {
    if (isKeyEmpty(key)) {
        return false;
    }
    mmkv::MMBuffer data;
    if constexpr (std::is_same_v<T, std::vector<bool>>) {
        data = mmkv::MiniPBCoder::encodeDataWithObject(value);
    } else {
        data = mmkv::MiniPBCoder::encodeDataWithObject(std::span(value));
    }
    if (mmkv_unlikely(m_enableKeyExpire) && data.length() > 0) {
        auto tmp = mmkv::MMBuffer(data.length() + ConstFixed32Size);
        auto ptr = (uint8_t *) tmp.getPtr();
        memcpy(ptr, data.getPtr(), data.length());
        auto time = (expireDuration != ExpireNever) ? getCurrentTimeInSecond() + expireDuration : ExpireNever;
        memcpy(ptr + data.length(), &time, ConstFixed32Size);
        data = std::move(tmp);
    }
    return setDataForKey(std::move(data), key);
}

template<MMKV_SUPPORTED_VECTOR_VALUE_TYPE T>
bool MMKV::getVector(MMKVKey_t key, T &result) {
    if (isKeyEmpty(key)) {
        return false;
    }
    shared_lock();

    bool ret = false;
    auto data = getDataForKey(key);
    if (data.length() > 0) {
        ret = mmkv::MiniPBCoder::decodeVector(data, result);
    }

    shared_unlock();
    return ret;
}
#endif // MMKV_HAS_CPP20 && !MMKV_APPLE

MMKV_NAMESPACE_END

#endif
#endif // MMKV_MMKV_H
