//
// Created by Ling Guo on 2018/4/4.
//

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

enum : bool {
    MMKV_SINGLE_PROCESS = false,
    MMKV_MULTI_PROCESS = true,
};

class MMKV {
    std::unordered_map<std::string, MMBuffer> m_dic;
    std::string m_path;
    std::string m_crcPath;
    std::string m_mmapID;
    int m_fd;
    char *m_ptr;
    size_t m_size;
    size_t m_actualSize;
    CodedOutputData *m_output;

    bool m_needLoadFromFile;

    uint32_t m_crcDigest;
    MmapedFile m_metaFile;
    MMKVMetaInfo m_metaInfo;

    ThreadLock m_lock;
    FileLock m_fileLock;
    InterProcessLock m_sharedProcessLock;
    InterProcessLock m_exclusiveProcessLock;

    void loadFromFile();

    void partialLoadFromFile();

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

    // just forbid it for possibly misuse
    MMKV(const MMKV &other) = delete;

    MMKV &operator=(const MMKV &other) = delete;

public:
    MMKV(const std::string &mmapID, bool interProcess = MMKV_SINGLE_PROCESS);

    ~MMKV();

    static void initializeMMKV(const std::string &rootDir);

    // a generic purpose instance
    static MMKV *defaultMMKV(bool interProcess = MMKV_SINGLE_PROCESS);

    /* mmapID: any unique ID (com.tencent.xin.pay, etc)
   * if you want a per-user mmkv, you could merge user-id within mmapID */
    static MMKV *mmkvWithID(const std::string &mmapID, bool interProcess = MMKV_SINGLE_PROCESS);

    static void onExit();

    const std::string &mmapID();

    const bool m_isInterProcess;

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

    // call on memory warning
    void clearMemoryState();

    // you don't need to call this, really, I mean it
    // unless you care about out of battery
    void sync();

    static bool isFileValid(const std::string &mmapID);

    void lock() {
        m_exclusiveProcessLock.lock();
    }
    void unlock() {
        m_exclusiveProcessLock.unlock();
    }
    bool try_lock() {
        return m_exclusiveProcessLock.try_lock();
    }
};

#endif // MMKV_MMKV_H
