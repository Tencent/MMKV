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

#include "InterProcessLock.h"
#include "MMKVMetaInfo.hpp"
#include "MmapedFile.h"
#include "ThreadLock.h"
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class CodedOutputData;
class MMBuffer;
class AESCrypt;

enum MMKVMode : uint32_t {
    MMKV_SINGLE_PROCESS = 0x1,
    MMKV_MULTI_PROCESS = 0x2,
    MMKV_ASHMEM = 0x4,
};

class MMKV {
    std::unordered_map<std::string, MMBuffer> m_dic;
    std::string m_mmapID;
    std::string m_path;
    std::string m_crcPath;
    int m_fd;
    char *m_ptr;
    size_t m_size;
    size_t m_actualSize;
    CodedOutputData *m_output;
    MmapedFile *m_ashmemFile;

    bool m_needLoadFromFile;
    bool m_hasFullWriteback;

    uint32_t m_crcDigest;
    MmapedFile m_metaFile;
    MMKVMetaInfo m_metaInfo;

    AESCrypt *m_crypter;

    ThreadLock m_lock;
    FileLock m_fileLock;
    InterProcessLock m_sharedProcessLock;
    InterProcessLock m_exclusiveProcessLock;

    void loadFromFile();

    void partialLoadFromFile();

    void loadFromAshmem();

    void checkLoadData();

    bool isFileValid();

    bool checkFileCRCValid();

    void recaculateCRCDigest();

    void updateCRCDigest(const uint8_t *ptr, size_t length, bool increaseSequence = false);

    void writeAcutalSize(size_t actualSize);

    bool ensureMemorySize(size_t newSize);

    bool fullWriteback();

    const MMBuffer &getDataForKey(const std::string &key);

    bool setDataForKey(MMBuffer &&data, const std::string &key);

    bool removeDataForKey(const std::string &key);

    bool appendDataWithKey(const MMBuffer &data, const std::string &key);

    void checkReSetCryptKey(int fd, int metaFD, std::string *cryptKey);

    // just forbid it for possibly misuse
    MMKV(const MMKV &other) = delete;

    MMKV &operator=(const MMKV &other) = delete;

public:
    MMKV(const std::string &mmapID,
         int size = DEFAULT_MMAP_SIZE,
         MMKVMode mode = MMKV_SINGLE_PROCESS,
         std::string *cryptKey = nullptr);

    MMKV(const std::string &mmapID,
         int ashmemFD,
         int ashmemMetaFd,
         std::string *cryptKey = nullptr);

    ~MMKV();

    static void initializeMMKV(const std::string &rootDir);

    // a generic purpose instance
    static MMKV *defaultMMKV(MMKVMode mode = MMKV_SINGLE_PROCESS, std::string *cryptKey = nullptr);

    // mmapID: any unique ID (com.tencent.xin.pay, etc)
    // if you want a per-user mmkv, you could merge user-id within mmapID
    static MMKV *mmkvWithID(const std::string &mmapID,
                            int size = DEFAULT_MMAP_SIZE,
                            MMKVMode mode = MMKV_SINGLE_PROCESS,
                            std::string *cryptKey = nullptr);

    static MMKV *mmkvWithAshmemFD(const std::string &mmapID,
                                  int fd,
                                  int metaFD,
                                  std::string *cryptKey = nullptr);

    static void onExit();

    const std::string &mmapID();

    const bool m_isInterProcess;

    const bool m_isAshmem;

    int ashmemFD() { return m_isAshmem ? m_fd : -1; }

    int ashmemMetaFD() { return m_isAshmem ? m_metaFile.getFd() : -1; }

    std::string cryptKey();

    // transform plain text into encrypted text
    // or vice versa by passing cryptKey = null
    // or change existing crypt key
    bool reKey(const std::string &cryptKey);

    // just reset cryptKey (will not encrypt or decrypt anything)
    // usually you should call this method after other process reKey() the inter-process mmkv
    void checkReSetCryptKey(const std::string *cryptKey);

    bool setStringForKey(const std::string &value, const std::string &key);

    bool setBytesForKey(const MMBuffer &value, const std::string &key);

    bool setBool(bool value, const std::string &key);

    bool setInt32(int32_t value, const std::string &key);

    bool setInt64(int64_t value, const std::string &key);

    bool setFloat(float value, const std::string &key);

    bool setDouble(double value, const std::string &key);

    bool setVectorForKey(const std::vector<std::string> &vector, const std::string &key);

    bool getStringForKey(const std::string &key, std::string &result);

    MMBuffer getBytesForKey(const std::string &key);

    bool getBoolForKey(const std::string &key, bool defaultValue = false);

    int32_t getInt32ForKey(const std::string &key, int32_t defaultValue = 0);

    int64_t getInt64ForKey(const std::string &key, int64_t defaultValue = 0);

    float getFloatForKey(const std::string &key, float defaultValue = 0);

    double getDoubleForKey(const std::string &key, double defaultValue = 0);

    bool getVectorForKey(const std::string &key, std::vector<std::string> &result);

    bool containsKey(const std::string &key);

    size_t count();

    size_t totalSize();

    std::vector<std::string> allKeys();

    void removeValueForKey(const std::string &key);

    void removeValuesForKeys(const std::vector<std::string> &arrKeys);

    void clearAll();

    // MMKV's size won't reduce after deleting key-values
    // call this method after lots of deleting f you care about disk usage
    // note that `clearAll` has the similar effect of `trim`
    void trim();

    // call this method if the instance is no longer needed in the near future
    // any subsequent call to the instance is undefined behavior
    void close();

    // call this method if you are facing memory-warning
    // any subsequent call to the instance will load all key-values from file again
    void clearMemoryState();

    // you don't need to call this, really, I mean it
    // unless you care about out of battery
    void sync();

    static bool isFileValid(const std::string &mmapID);

    void lock() { m_exclusiveProcessLock.lock(); }
    void unlock() { m_exclusiveProcessLock.unlock(); }
    bool try_lock() { return m_exclusiveProcessLock.try_lock(); }
};

#endif // MMKV_MMKV_H
