//
// Created by Ling Guo on 2018/4/4.
//

#include "MmapedFile.h"
#include "MMBuffer.h"
#include "MMKV.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <zlib.h>
#include <algorithm>
#include "ScopedLock.hpp"
#include "CodedInputData.h"
#include "CodedOutputData.h"
#include "PBUtility.h"
#include "MiniPBCoder.h"
#include "MMKVLog.h"
#include "InterProcessLock.h"

using namespace std;

static unordered_map<std::string, MMKV*>* g_instanceDic;
static ThreadLock g_instanceLock;
static std::string g_rootDir;
const int DEFAULT_MMAP_SIZE = getpagesize();

#define DEFAULT_MMAP_ID "mmkv.default"
constexpr int Fixed32Size = computeFixed32Size(0);

static string mappedKVPathWithID(const string& mmapID);
static string crcPathWithMappedKVPath(const string& path);

enum : bool {
    KeepSequence = false,
    IncreaseSequence = true,
};

MMKV::MMKV(const std::string& mmapID, bool interProcess)
        : m_mmapID(mmapID),
          m_path(mappedKVPathWithID(mmapID)),
          m_crcPath(crcPathWithMappedKVPath(m_path)),
          m_metaFile(m_crcPath),
          m_fileLock(m_metaFile.getFd()),
          m_sharedProcessLock(&m_fileLock, SharedLockType),
          m_exclusiveProcessLock(&m_fileLock, ExclusiveLockType),
          m_isInterProcess(interProcess) {
    m_fd = -1;
    m_ptr = nullptr;
    m_size = 0;
    m_actualSize = 0;
    m_output = nullptr;

    m_needLoadFromFile = true;

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
}

MMKV* MMKV::defaultMMKV(bool interProcess) {
    return mmkvWithID(DEFAULT_MMAP_ID, interProcess);
}

void initialize() {
    g_instanceDic = new unordered_map<std::string, MMKV*>;
    g_instanceLock = ThreadLock();

    MMKVInfo("pagesize:%d", DEFAULT_MMAP_SIZE);
}
void MMKV::initializeMMKV(const std::string& rootDir) {
    static pthread_once_t once_control = PTHREAD_ONCE_INIT;
    pthread_once(&once_control, initialize);

    g_rootDir = rootDir;
    char* path = strdup(g_rootDir.c_str());
    mkpath(path);
    free(path);

    MMKVInfo("root dir: %s", g_rootDir.c_str());
}

MMKV* MMKV::mmkvWithID(const std::string &mmapID, bool interProcess) {

    if (mmapID.empty()) {
        return nullptr;
    }
    SCOPEDLOCK(g_instanceLock);

    auto itr = g_instanceDic->find(mmapID);
    if (itr != g_instanceDic->end()) {
        return itr->second;
    }
    auto kv = new MMKV(mmapID, interProcess);
    (*g_instanceDic)[mmapID] = kv;
    return kv;
}

void MMKV::onExit() {
    SCOPEDLOCK(g_instanceLock);

    for (auto &itr : *g_instanceDic) {
        MMKV* kv = itr.second;
        kv->sync();
        kv->clearMemoryState();
    }
}

const std::string& MMKV::mmapID() {
    return m_mmapID;
}

#pragma mark - really dirty work

void MMKV::loadFromFile() {
    m_metaInfo.read(m_metaFile.getMemory());

    m_fd = open(m_path.c_str(), O_RDWR | O_CREAT, S_IRWXU);
    if (m_fd <= 0) {
        MMKVError("fail to open:%s, %s", m_path.c_str(), strerror(errno));
    } else {
        m_size = 0;
        struct stat st = {0};
        if (fstat(m_fd, &st) != -1) {
            m_size = (size_t) st.st_size;
        }
        // round up to (n * pagesize)
        if (m_size < DEFAULT_MMAP_SIZE || (m_size % DEFAULT_MMAP_SIZE != 0)) {
            m_size = ((m_size / DEFAULT_MMAP_SIZE) + 1) * DEFAULT_MMAP_SIZE;
            if (ftruncate(m_fd, m_size) != 0) {
                MMKVError("fail to truncate [%s] to size %zu, %s", m_mmapID.c_str(), m_size, strerror(errno));
                m_size = (size_t) st.st_size;
            }
        }
        m_ptr = (char *) mmap(NULL, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
        if (m_ptr == MAP_FAILED) {
            MMKVError("fail to mmap [%s], %s", m_mmapID.c_str(), strerror(errno));
        } else {
            memcpy(&m_actualSize, m_ptr, Fixed32Size);
            MMKVInfo("loading [%s] with %zu size in total, file size is %zu", m_mmapID.c_str(), m_actualSize, m_size);
            bool loaded = false;
            if (m_actualSize > 0) {
                if (m_actualSize < m_size && m_actualSize + Fixed32Size <= m_size) {
                    if (checkFileCRCValid()) {
                        MMKVInfo("loading [%s] with crc %zu sequence %zu", m_mmapID.c_str(), m_metaInfo.m_crcDigest, m_metaInfo.m_sequence);
                        MMBuffer inputBuffer(m_ptr + Fixed32Size, m_actualSize, MMBufferNoCopy);
                        m_dic = MiniPBCoder::decodeMap(inputBuffer);
                        m_output = new CodedOutputData(m_ptr + Fixed32Size + m_actualSize, m_size - Fixed32Size - m_actualSize);
                        loaded = true;
                    }
                }
            }
            if (!loaded) {
                SCOPEDLOCK(m_exclusiveProcessLock);

                if (m_actualSize > 0) {
                    writeAcutalSize(0);
                }
                m_output = new CodedOutputData(m_ptr + Fixed32Size, m_size - Fixed32Size);
                recaculateCRCDigest();
            }
            MMKVInfo("loaded [%s] with %zu values", m_mmapID.c_str(), m_dic.size());
        }
    }

    if (!isFileValid()) {
        MMKVWarning("[%s] file not valid", m_mmapID.c_str());
    }

    m_needLoadFromFile = false;
}

// read from last position
void MMKV::partialLoadFromFile() {
    m_metaInfo.read(m_metaFile.getMemory());

    size_t oldActualSize = m_actualSize;
    try {
        m_actualSize = CodedInputData(m_ptr, Fixed32Size).readFixed32();
    } catch (exception &exception) {
        MMKVError("%s", exception.what());
    }
    MMKVInfo("loading [%s] with file size %zu, oldActualSize %zu, newActualSize %zu",
             m_mmapID.c_str(), m_size, oldActualSize, m_actualSize);

    if (m_actualSize > 0) {
        if (m_actualSize < m_size && m_actualSize + Fixed32Size <= m_size) {
            if (m_actualSize > oldActualSize) {
                size_t bufferSize = m_actualSize - oldActualSize;
                MMBuffer inputBuffer(m_ptr + Fixed32Size + oldActualSize, bufferSize, MMBufferNoCopy);
                // incremental update crc digest
                m_crcDigest = (uint32_t)crc32(m_crcDigest, (const uint8_t *)inputBuffer.getPtr(), inputBuffer.length());
                if (m_crcDigest == m_metaInfo.m_crcDigest) {
                    auto dic = MiniPBCoder::decodeMap(inputBuffer, bufferSize);
                    for (auto &itr : dic) {
                        //m_dic[itr.first] = std::move(itr.second);
                        auto target = m_dic.find(itr.first);
                        if (target == m_dic.end()) {
                            m_dic.emplace(itr.first, std::move(itr.second));
                        } else {
                            target->second = std::move(itr.second);
                        }
                    }
                    m_output->seek(bufferSize);

                    MMKVInfo("partial loaded [%s] with %zu values", m_mmapID.c_str(), m_dic.size());
                    return;
                } else {
                    MMKVError("m_crcDigest[%zu] != m_metaInfo.m_crcDigest[%zu]", m_crcDigest, m_metaInfo.m_crcDigest);
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
    MMKVMetaInfo metaInfo;
    metaInfo.read(m_metaFile.getMemory());
    if (m_metaInfo.m_sequence != metaInfo.m_sequence) {
        MMKVInfo("[%s] oldSeq %zu, newSeq %zu", m_mmapID.c_str(), m_metaInfo.m_sequence, metaInfo.m_sequence);
        SCOPEDLOCK(m_sharedProcessLock);

        clearMemoryState();
        loadFromFile();
    } else if (m_metaInfo.m_crcDigest != metaInfo.m_crcDigest) {
        MMKVInfo("[%s] oldCrc %zu, newCrc %zu", m_mmapID.c_str(), m_metaInfo.m_crcDigest, metaInfo.m_crcDigest);
        SCOPEDLOCK(m_sharedProcessLock);

        struct stat st = {0};
        fstat(m_fd, &st);
        size_t fileSize = (size_t) st.st_size;
        if (m_size != fileSize) {
            MMKVInfo("file size has changed [%s] from %zu to %zu", m_mmapID.c_str(), m_size, fileSize);
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
        removeFile(m_path.c_str());
        loadFromFile();
        return;
    }

    if (m_ptr && m_ptr != MAP_FAILED) {
        // for truncate
        size_t size = std::min<size_t>(DEFAULT_MMAP_SIZE, m_size);
        memset(m_ptr, 0, size);
        if (msync(m_ptr, size, MS_SYNC) != 0) {
            MMKVError("fail to msync [%s]:%s", m_mmapID.c_str(), strerror(errno));
        }
    }
    if (m_fd > 0) {
        if (m_size != DEFAULT_MMAP_SIZE) {
            MMKVInfo("truncating [%s] from %zu to %d", m_mmapID.c_str(), m_size, DEFAULT_MMAP_SIZE);
            if (ftruncate(m_fd, DEFAULT_MMAP_SIZE) != 0) {
                MMKVError("fail to truncate [%s] to size %d, %s", m_mmapID.c_str(), DEFAULT_MMAP_SIZE, strerror(errno));
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

    if (m_output) {
        delete m_output;
    }
    m_output = nullptr;

    if (m_ptr && m_ptr != MAP_FAILED) {
        if (munmap(m_ptr, m_size) != 0) {
            MMKVError("fail to munmap [%s], %s", m_mmapID.c_str(), strerror(errno));
        }
    }
    m_ptr = nullptr;

    if (m_fd > 0) {
        if (close(m_fd) != 0) {
            MMKVError("fail to close [%s], %s", m_mmapID.c_str(), strerror(errno));
        }
    }
    m_fd = 0;
    m_size = 0;
    m_actualSize = 0;
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
        static const int offset = computeFixed32Size(0);
        MMBuffer data = MiniPBCoder::encodeDataWithObject(m_dic);
        size_t lenNeeded = data.length() + offset + newSize;
        size_t futureUsage = newSize * std::max<size_t>(8, (m_dic.size()+1)/2);
        // 1. no space for a full rewrite, double it
        // 2. or space is not large enough for future usage, double it to avoid frequently full rewrite
        if (lenNeeded >= m_size || (lenNeeded + futureUsage) >= m_size) {
            size_t oldSize = m_size;
            do {
                m_size *= 2;
            } while (lenNeeded + futureUsage >= m_size);
            MMKVInfo("extending [%s] file size from %zu to %zu, incoming size:%zu, futrue usage:%zu",
                     m_mmapID.c_str(), oldSize, m_size, newSize, futureUsage);

            // if we can't extend size, rollback to old state
            if (ftruncate(m_fd, m_size) != 0) {
                MMKVError("fail to truncate [%s] to size %zu, %s", m_mmapID.c_str(), m_size, strerror(errno));
                m_size = oldSize;
                return false;
            }

            if (munmap(m_ptr, oldSize) != 0) {
                MMKVError("fail to munmap [%s], %s", m_mmapID.c_str(), strerror(errno));
            }
            m_ptr = (char*)mmap(m_ptr, m_size, PROT_READ|PROT_WRITE, MAP_SHARED, m_fd, 0);
            if (m_ptr == MAP_FAILED) {
                MMKVError("fail to mmap [%s], %s", m_mmapID.c_str(), strerror(errno));
            }

            // check if we fail to make more space
            if (!isFileValid()) {
                MMKVWarning("[%s] file not valid", m_mmapID.c_str());
                return false;
            }
            // keep m_output consistent with m_ptr -- writeAcutalSize: may fail
            delete m_output;
            m_output = new CodedOutputData(m_ptr+offset, m_size-offset);
            m_output->seek(m_actualSize);
        }

        if (!writeAcutalSize(data.length())) {
            return false;
        }

        delete m_output;
        m_output = new CodedOutputData(m_ptr+offset, m_size-offset);
        m_output->writeRawData(data);
        recaculateCRCDigest();
        return ret;
    }
    return true;
}

bool MMKV::writeAcutalSize(size_t actualSize) {
    assert(m_ptr != 0);
    assert(m_ptr != MAP_FAILED);

    memcpy(m_ptr, &actualSize, Fixed32Size);
    m_actualSize = actualSize;

    return true;
}

const MMBuffer& MMKV::getDataForKey(const std::string &key) {
    SCOPEDLOCK(m_lock);
    checkLoadData();
    auto itr = m_dic.find(key);
    if (itr != m_dic.end()) {
        return itr->second;
    }
    static MMBuffer nan(0);
    return nan;
}

bool MMKV::setDataForKey(MMBuffer&& data, const std::string &key) {
    if (data.length() == 0 || key.empty()) {
        return false;
    }
    SCOPEDLOCK(m_lock);
    SCOPEDLOCK(m_exclusiveProcessLock);
    checkLoadData();

//    m_dic[key] = std::move(data);
    auto itr = m_dic.find(key);
    if (itr == m_dic.end()) {
        itr = m_dic.emplace(key , std::move(data)).first;
    } else {
        itr->second = std::move(data);
    }

    return appendDataWithKey(itr->second, key);
}

bool MMKV::removeDataForKey(const std::string &key) {
    if (key.empty()) {
        return false;
    }

    auto deleteCount = m_dic.erase(key);
    if (deleteCount > 0) {
        static MMBuffer nan(0);
        return appendDataWithKey(nan, key);
    }

    return false;
}

bool MMKV::appendDataWithKey(const MMBuffer &data, const std::string &key) {
    size_t keyLength = key.length();
    size_t size = keyLength + computeRawVarint32Size((int32_t)keyLength);		// size needed to encode the key
    size += data.length() + computeRawVarint32Size((int32_t)data.length());		// size needed to encode the value

    SCOPEDLOCK(m_exclusiveProcessLock);

    bool hasEnoughSize = ensureMemorySize(size);

    if (!hasEnoughSize || !isFileValid()) {
        return false;
    }
    if (m_actualSize == 0) {
        auto allData = MiniPBCoder::encodeDataWithObject(m_dic);
        if (allData.length() > 0) {
            bool ret = writeAcutalSize(allData.length());
            if (ret) {
                m_output->writeRawData(allData);		// note: don't write size of data
                recaculateCRCDigest();
            }
            return ret;
        }
        return false;
    } else {
        bool ret = writeAcutalSize(m_actualSize + size);
        if (ret) {
            static const int offset = computeFixed32Size(0);
            m_output->writeString(key);
            m_output->writeData(data);				// note: write size of data
            updateCRCDigest((const uint8_t*)m_ptr+offset+m_actualSize-size, size, KeepSequence);
        }
        return ret;
    }
}

bool MMKV::fullWriteback() {
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
    if (allData.length() > 0 && isFileValid()) {
        if (allData.length() + Fixed32Size <= m_size) {
            bool ret = writeAcutalSize(allData.length());
            if (ret) {
                delete m_output;
                m_output = new CodedOutputData(m_ptr + Fixed32Size, m_size - Fixed32Size);
                m_output->writeRawData(allData);        // note: don't write size of data
                recaculateCRCDigest();
            }
            return ret;
        } else {
            // ensureMemorySize will extend file & full rewrite, no need to write back again
            return ensureMemorySize(allData.length() + Fixed32Size - m_size);
        }
    }
    return false;
}

bool MMKV::isFileValid() {
    if (m_fd > 0 && m_size > 0 && m_output && m_ptr && m_ptr != MAP_FAILED) {
        return true;
    }
    return false;
}

#pragma mark - crc

// assuming m_ptr & m_size is set
bool MMKV::checkFileCRCValid() {
    if (m_ptr && m_ptr != MAP_FAILED) {
        constexpr int offset = computeFixed32Size(0);
        m_crcDigest = (uint32_t) crc32(0, (const uint8_t *) m_ptr + offset, (uint32_t) m_actualSize);

        // for backward compatibility
        if (!isFileExist(m_crcPath)) {
            MMKVInfo("crc32 file not found:%s", m_crcPath.c_str());
            return true;
        }
        m_metaInfo.read(m_metaFile.getMemory());
        if (m_crcDigest == m_metaInfo.m_crcDigest) {
            return true;
        }
        MMKVError("check crc [%s] fail, crc32:%u, m_crcDigest:%u", m_mmapID.c_str(), m_metaInfo.m_crcDigest, m_crcDigest);
    }
    return false;
}

void MMKV::recaculateCRCDigest() {
    if (m_ptr && m_ptr != MAP_FAILED) {
        m_crcDigest = 0;
        constexpr int offset = computeFixed32Size(0);
        updateCRCDigest((const uint8_t *)m_ptr+offset, m_actualSize, IncreaseSequence);
    }
}

void MMKV::updateCRCDigest(const uint8_t *ptr, size_t length, bool increaseSequence) {
    if (!ptr) {
        return;
    }
    m_crcDigest = (uint32_t)crc32(m_crcDigest, ptr, (uint32_t)length);

    void * crcPtr = m_metaFile.getMemory();
    if (crcPtr == nullptr || crcPtr == MAP_FAILED) {
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

#pragma mark - set & get

bool MMKV::setStringForKey(const std::string &value, const std::string &key) {
    if (key.empty()) {
        return false;
    }
    auto data = MiniPBCoder::encodeDataWithObject(value);
    return setDataForKey(std::move(data), key);
}

bool MMKV::setBytesForKey(const MMBuffer &value, const std::string &key) {
    if (key.empty()) {
        return false;
    }
    auto data = MiniPBCoder::encodeDataWithObject(value);
    return setDataForKey(std::move(data), key);
}

bool MMKV::setBool(bool value, const std::string &key) {
    if (key.empty()) {
        return false;
    }
    size_t size = computeBoolSize(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeBool(value);

    return setDataForKey(std::move(data), key);
}

bool MMKV::setInt32(int32_t value, const std::string &key) {
    if (key.empty()) {
        return false;
    }
    size_t size = computeInt32Size(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeInt32(value);

    return setDataForKey(std::move(data), key);
}

bool MMKV::setInt64(int64_t value, const std::string &key) {
    if (key.empty()) {
        return false;
    }
    size_t size = computeInt64Size(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeInt64(value);

    return setDataForKey(std::move(data), key);
}

bool MMKV::setFloat(float value, const std::string &key) {
    if (key.empty()) {
        return false;
    }
    size_t size = computeFloatSize(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeFloat(value);

    return setDataForKey(std::move(data), key);
}

bool MMKV::setDouble(double value, const std::string &key) {
    if (key.empty()) {
        return false;
    }
    size_t size = computeDoubleSize(value);
    MMBuffer data(size);
    CodedOutputData output(data.getPtr(), size);
    output.writeDouble(value);

    return setDataForKey(std::move(data), key);
}

bool MMKV::setVectorForKey(const std::vector<std::string>& v, const std::string& key) {
    if (key.empty()) {
        return false;
    }
    auto data = MiniPBCoder::encodeDataWithObject(v);
    return setDataForKey(std::move(data), key);
}

bool MMKV::getStringForKey(const std::string& key, std::string& result) {
    if (key.empty()) {
        return false;
    }
    auto& data = getDataForKey(key);
    if (data.length() > 0) {
        result = MiniPBCoder::decodeString(data);
        return true;
    }
    return false;
}

MMBuffer MMKV::getBytesForKey(const std::string &key) {
    if (key.empty()) {
        return MMBuffer(0);
    }
    auto& data = getDataForKey(key);
    if (data.length() > 0) {
        return MiniPBCoder::decodeBytes(data);
    }
    return MMBuffer(0);
}

bool MMKV::getBoolForKey(const std::string &key, bool defaultValue) {
    if (key.empty()) {
        return defaultValue;
    }
    auto& data = getDataForKey(key);
    if (data.length() > 0) {
        try {
            CodedInputData input(data.getPtr(), data.length());
            return input.readBool();
        } catch (exception& e) {
            MMKVError("%s", e.what());
        }
    }
    return defaultValue;
}

int32_t MMKV::getInt32ForKey(const std::string &key, int32_t defaultValue) {
    if (key.empty()) {
        return defaultValue;
    }
    auto &data = getDataForKey(key);
    if (data.length() > 0) {
        try {
            CodedInputData input(data.getPtr(), data.length());
            return input.readInt32();
        } catch (exception &e) {
            MMKVError("%s", e.what());
        }
    }
    return defaultValue;
}

int64_t MMKV::getInt64ForKey(const std::string &key, int64_t defaultValue) {
    if (key.empty()) {
        return defaultValue;
    }
    auto& data = getDataForKey(key);
    if (data.length() > 0) {
        try {
            CodedInputData input(data.getPtr(), data.length());
            return input.readInt64();
        } catch (exception& e) {
            MMKVError("%s", e.what());
        }
    }
    return defaultValue;
}

float MMKV::getFloatForKey(const std::string &key, float defaultValue) {
    if (key.empty()) {
        return defaultValue;
    }
    auto& data = getDataForKey(key);
    if (data.length() > 0) {
        try {
            CodedInputData input(data.getPtr(), data.length());
            return input.readFloat();
        } catch (exception& e) {
            MMKVError("%s", e.what());
        }
    }
    return defaultValue;
}

double MMKV::getDoubleForKey(const std::string &key, double defaultValue) {
    if (key.empty()) {
        return defaultValue;
    }
    auto& data = getDataForKey(key);
    if (data.length() > 0) {
        try {
            CodedInputData input(data.getPtr(), data.length());
            return input.readDouble();
        } catch (exception& e) {
            MMKVError("%s", e.what());
        }
    }
    return defaultValue;
}

bool MMKV::getVectorForKey(const std::string &key, std::vector<std::string> &result) {
    if (key.empty()) {
        return false;
    }
    auto& data = getDataForKey(key);
    if (data.length() > 0) {
        result = MiniPBCoder::decodeSet(data);
        return true;
    }
    return false;
}

#pragma mark - enumerate

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

//void MMKV::enumerateKeys(std::function<void(const std::string &key, bool &stop)> block) {
//    MMKVInfo("enumerate [%s] begin", m_mmapID.c_str());
//    SCOPEDLOCK(m_lock);
//    checkLoadData();
//
//    bool stop = false;
//    for (const auto& itr : m_dic) {
//        block(itr.first, stop);
//        if (stop) {
//            break;
//        }
//    };
//    MMKVInfo("enumerate [%s] finish", m_mmapID.c_str());
//}
std::vector<std::string> MMKV::allKeys() {
    SCOPEDLOCK(m_lock);
    checkLoadData();

    vector<string> keys;
    for (const auto& itr : m_dic) {
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
    for (const auto& key : arrKeys) {
        m_dic.erase(key);
    }

    fullWriteback();
}

#pragma mark - file

void MMKV::sync() {
    SCOPEDLOCK(m_lock);
    if (m_needLoadFromFile || !isFileValid()) {
        return;
    }
    SCOPEDLOCK(m_exclusiveProcessLock);
    if (msync(m_ptr, m_size, MS_SYNC) != 0) {
        MMKVError("fail to msync [%s]:%s", m_mmapID.c_str(), strerror(errno));
    }
}

bool MMKV::isFileValid(const std::string &mmapID) {
    string kvPath = mappedKVPathWithID(mmapID);
    if (!isFileExist(kvPath)) {
        // kv文件不存在，有效
        return true;
    }

    string crcPath = crcPathWithMappedKVPath(kvPath);
    if (!isFileExist(crcPath.c_str())) {
        // crc文件不存在，有效
        return true;
    }

    // 读取crc文件值
    uint32_t crcFile = 0;
    MMBuffer* data = readWholeFile(crcPath.c_str());
    if (data) {
        try {
            MMKVMetaInfo metaInfo;
            metaInfo.read(data->getPtr());
            crcFile = metaInfo.m_crcDigest;
        } catch (exception& e) {
            // 有异常，无效
            delete data;
            return false;
        }
        delete data;
    } else {
        return false;
    }

    // 计算原kv crc值
    const int offset = computeFixed32Size(0);
    size_t actualSize = 0;
    MMBuffer* fileData = readWholeFile(kvPath.c_str());
    if (fileData) {
        try {
            actualSize = CodedInputData(fileData->getPtr(), fileData->length()).readFixed32();
        } catch(exception& e) {
            // 有异常，无效
            delete fileData;
            return false;
        }
        if (actualSize > fileData->length() - offset) {
            // 长度异常，无效
            delete fileData;
            return false;
        }

        uint32_t crcDigest = (uint32_t)crc32(0, (const uint8_t*)fileData->getPtr() + offset, (uint32_t)actualSize);

        delete fileData;
        return crcFile == crcDigest;
    } else {
        return false;
    }
}

static string mappedKVPathWithID(const string& mmapID) {
    return g_rootDir + "/" + mmapID;
}
static string crcPathWithMappedKVPath(const string& path) {
    return path + ".crc";
}

