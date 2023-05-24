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
#ifdef  __cplusplus

#include "MMBuffer.h"
#include <cstdint>

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
};

#define OUT

class MMKV {
#ifndef MMKV_ANDROID
    std::string m_mmapKey;
    MMKV(const std::string &mmapID, MMKVMode mode, std::string *cryptKey, MMKVPath_t *rootPath);
#else // defined(MMKV_ANDROID)
    mmkv::FileLock *m_fileModeLock;
    mmkv::InterProcessLock *m_sharedProcessModeLock;
    mmkv::InterProcessLock *m_exclusiveProcessModeLock;

    MMKV(const std::string &mmapID, int size, MMKVMode mode, std::string *cryptKey, MMKVPath_t *rootPath);

    MMKV(const std::string &mmapID, int ashmemFD, int ashmemMetaFd, std::string *cryptKey = nullptr);
#endif

    ~MMKV();

    std::string m_mmapID;
    MMKVPath_t m_path;
    MMKVPath_t m_crcPath;
    mmkv::MMKVMap *m_dic;
    mmkv::MMKVMapCrypt *m_dicCrypt;

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

    bool m_enableKeyExipre = false;
    uint32_t m_expiredInSeconds = 0;

#ifdef MMKV_APPLE
    using MMKVKey_t = NSString *__unsafe_unretained;
    static bool isKeyEmpty(MMKVKey_t key) { return key.length <= 0; }
#  define key_length(key) key.length
#  define retain_key(key) [key retain]
#  define release_key(key) [key release]
#else
    using MMKVKey_t = const std::string &;
    static bool isKeyEmpty(MMKVKey_t key) { return key.empty(); }
#  define key_length(key) key.length()
#  define retain_key(key) ((void)0)
#  define release_key(key) ((void)0)
#endif

    void loadFromFile();

    void partialLoadFromFile();

    void loadMetaInfoAndCheck();

    void checkDataValid(bool &loadFromFile, bool &needFullWriteback);

    void checkLoadData();

    bool isFileValid();

    bool checkFileCRCValid(size_t actualSize, uint32_t crcDigest);

    void recaculateCRCDigestWithIV(const void *iv);

    void updateCRCDigest(const uint8_t *ptr, size_t length);

    size_t readActualSize();

    void oldStyleWriteActualSize(size_t actualSize);

    bool writeActualSize(size_t size, uint32_t crcDigest, const void *iv, bool increaseSequence);

    bool ensureMemorySize(size_t newSize);

    bool expandAndWriteBack(size_t newSize, std::pair<mmkv::MMBuffer, size_t> preparedData);

    bool fullWriteback(mmkv::AESCrypt *newCrypter = nullptr);

    bool doFullWriteBack(std::pair<mmkv::MMBuffer, size_t> preparedData, mmkv::AESCrypt *newCrypter);

    bool doFullWriteBack(mmkv::MMKVVector &&vec);

    mmkv::MMBuffer getRawDataForKey(MMKVKey_t key);

    mmkv::MMBuffer getDataForKey(MMKVKey_t key);

    // isDataHolder: avoid memory copying
    bool setDataForKey(mmkv::MMBuffer &&data, MMKVKey_t key, bool isDataHolder = false);

    bool removeDataForKey(MMKVKey_t key);

    using KVHolderRet_t = std::pair<bool, mmkv::KeyValueHolder>;
    // isDataHolder: avoid memory copying
    KVHolderRet_t doAppendDataWithKey(const mmkv::MMBuffer &data, const mmkv::MMBuffer &key, bool isDataHolder, uint32_t keyLength);
    KVHolderRet_t appendDataWithKey(const mmkv::MMBuffer &data, MMKVKey_t key, bool isDataHolder = false);
    KVHolderRet_t appendDataWithKey(const mmkv::MMBuffer &data, const mmkv::KeyValueHolder &kvHolder, bool isDataHolder = false);
#ifdef MMKV_APPLE
    KVHolderRet_t appendDataWithKey(const mmkv::MMBuffer &data,
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

public:
    // call this before getting any MMKV instance
    static void initializeMMKV(const MMKVPath_t &rootDir, MMKVLogLevel logLevel = MMKVLogInfo, mmkv::LogHandler handler = nullptr);

#ifdef MMKV_APPLE
    // protect from some old code that don't call initializeMMKV()
    static void minimalInit(MMKVPath_t defaultRootDir);
#endif

    // a generic purpose instance
    static MMKV *defaultMMKV(MMKVMode mode = MMKV_SINGLE_PROCESS, std::string *cryptKey = nullptr);

#ifndef MMKV_ANDROID

    // mmapID: any unique ID (com.tencent.xin.pay, etc)
    // if you want a per-user mmkv, you could merge user-id within mmapID
    // cryptKey: 16 bytes at most
    static MMKV *mmkvWithID(const std::string &mmapID,
                            MMKVMode mode = MMKV_SINGLE_PROCESS,
                            std::string *cryptKey = nullptr,
                            MMKVPath_t *rootPath = nullptr);

#else // defined(MMKV_ANDROID)

    // mmapID: any unique ID (com.tencent.xin.pay, etc)
    // if you want a per-user mmkv, you could merge user-id within mmapID
    // cryptKey: 16 bytes at most
    static MMKV *mmkvWithID(const std::string &mmapID,
                            int size = mmkv::DEFAULT_MMAP_SIZE,
                            MMKVMode mode = MMKV_SINGLE_PROCESS,
                            std::string *cryptKey = nullptr,
                            MMKVPath_t *rootPath = nullptr);

    static MMKV *mmkvWithAshmemFD(const std::string &mmapID, int fd, int metaFD, std::string *cryptKey = nullptr);

    int ashmemFD();

    int ashmemMetaFD();

    bool checkProcessMode();
#endif // MMKV_ANDROID

    // you can call this on application termination, it's totally fine if you don't call
    static void onExit();

    const std::string &mmapID() const;

    const bool m_isInterProcess;

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

    // avoid unexpected type conversion (pointer to bool, etc)
    template <typename T>
    bool set(T value, MMKVKey_t key) = delete;
    template <typename T>
    bool set(T value, MMKVKey_t key, uint32_t expireDuration) = delete;

#ifdef MMKV_APPLE
    bool set(NSObject<NSCoding> *__unsafe_unretained obj, MMKVKey_t key);
    bool set(NSObject<NSCoding> *__unsafe_unretained obj, MMKVKey_t key, uint32_t expireDuration);

    NSObject *getObject(MMKVKey_t key, Class cls);
#else  // !defined(MMKV_APPLE)
    bool set(const char *value, MMKVKey_t key);
    bool set(const char *value, MMKVKey_t key, uint32_t expireDuration);

    bool set(const std::string &value, MMKVKey_t key);
    bool set(const std::string &value, MMKVKey_t key, uint32_t expireDuration);

    bool set(const mmkv::MMBuffer &value, MMKVKey_t key);
    bool set(const mmkv::MMBuffer &value, MMKVKey_t key, uint32_t expireDuration);

    bool set(const std::vector<std::string> &vector, MMKVKey_t key);
    bool set(const std::vector<std::string> &vector, MMKVKey_t key, uint32_t expireDuration);

    bool getString(MMKVKey_t key, std::string &result);

    mmkv::MMBuffer getBytes(MMKVKey_t key);

    bool getBytes(MMKVKey_t key, mmkv::MMBuffer &result);

    bool getVector(MMKVKey_t key, std::vector<std::string> &result);
#endif // MMKV_APPLE

    bool getBool(MMKVKey_t key, bool defaultValue = false, OUT bool *hasValue = nullptr);

    int32_t getInt32(MMKVKey_t key, int32_t defaultValue = 0, OUT bool *hasValue = nullptr);

    uint32_t getUInt32(MMKVKey_t key, uint32_t defaultValue = 0, OUT bool *hasValue = nullptr);

    int64_t getInt64(MMKVKey_t key, int64_t defaultValue = 0, OUT bool *hasValue = nullptr);

    uint64_t getUInt64(MMKVKey_t key, uint64_t defaultValue = 0, OUT bool *hasValue = nullptr);

    float getFloat(MMKVKey_t key, float defaultValue = 0, OUT bool *hasValue = nullptr);

    double getDouble(MMKVKey_t key, double defaultValue = 0, OUT bool *hasValue = nullptr);

    // return the actual size consumption of the key's value
    // pass actualSize = true to get value's length
    size_t getValueSize(MMKVKey_t key, bool actualSize);

    // return size written into buffer
    // return -1 on any error
    int32_t writeValueToBuffer(MMKVKey_t key, void *ptr, int32_t size);

    bool containsKey(MMKVKey_t key);

    size_t count();

    size_t totalSize();

    size_t actualSize();

    // all keys created (or last modified) longger than expiredInSeconds will be deleted on next full-write-back
    // expiredInSeconds = 0 means no common expiration duration for all keys, aka each key will have it's own expiration duration
    bool enableAutoKeyExpire(uint32_t expiredInSeconds = 0);

    bool disableAutoKeyExpire();

#ifdef MMKV_APPLE
    NSArray *allKeys();

    void removeValuesForKeys(NSArray *arrKeys);

    typedef void (^EnumerateBlock)(NSString *key, BOOL *stop);
    void enumerateKeys(EnumerateBlock block);

#    ifdef MMKV_IOS
    static void setIsInBackground(bool isInBackground);
    static bool isInBackground();
#    endif
#else  // !defined(MMKV_APPLE)
    std::vector<std::string> allKeys();

    void removeValuesForKeys(const std::vector<std::string> &arrKeys);
#endif // MMKV_APPLE

    void removeValueForKey(MMKVKey_t key);

    void clearAll();

    // MMKV's size won't reduce after deleting key-values
    // call this method after lots of deleting if you care about disk usage
    // note that `clearAll` has the similar effect of `trim`
    void trim();

    // call this method if the instance is no longer needed in the near future
    // any subsequent call to the instance is undefined behavior
    void close();

    // call this method if you are facing memory-warning
    // any subsequent call to the instance will load all key-values from file again
    void clearMemoryCache();

    // you don't need to call this, really, I mean it
    // unless you worry about running out of battery
    void sync(SyncFlag flag = MMKV_SYNC);

    // get exclusive access
    void lock();
    void unlock();
    bool try_lock();

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

    // just forbid it for possibly misuse
    explicit MMKV(const MMKV &other) = delete;
    MMKV &operator=(const MMKV &other) = delete;
};

MMKV_NAMESPACE_END

#endif
#endif // MMKV_MMKV_H
