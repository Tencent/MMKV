//
// Created by Ling Guo on 2018/4/4.
//

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
#include "MmapedFile.h"

using namespace std;

static unordered_map<std::string, MMKV*>* g_instanceDic;
static ThreadLock* g_instanceLock;
static std::string g_rootDir;
const int DEFAULT_MMAP_SIZE = getpagesize();

#define DEFAULT_MMAP_ID "mmkv.default"

static string mappedKVPathWithID(const string& mmapID);
static string crcPathWithMappedKVPath(const string& path);

MMKV::MMKV(const std::string& mmapID) : m_mmapID(mmapID), m_lock() {
    m_fd = -1;
    m_ptr = nullptr;
    m_size = 0;
    m_actualSize = 0;
    m_output = nullptr;

    m_isInBackground = false;
    m_needLoadFromFile = true;

    m_crcDigest = 0;
    m_crcFd = -1;
    m_crcPtr = nullptr;

    m_path = mappedKVPathWithID(mmapID);
    m_crcPath = crcPathWithMappedKVPath(m_path);

    loadFromFile();
}

MMKV::~MMKV() {
    if (m_ptr != MAP_FAILED && m_ptr != nullptr) {
        munmap(m_ptr, m_size);
        m_ptr = nullptr;
    }
    if (m_fd > 0) {
        close(m_fd);
        m_fd = -1;
    }
    if (m_output) {
        delete m_output;
        m_output = nullptr;
    }

    if (m_crcPtr != nullptr && m_crcPtr != MAP_FAILED) {
        munmap(m_crcPtr, computeFixed32Size(0));
        m_crcPtr = nullptr;
    }
    if (m_crcFd > 0) {
        close(m_crcFd);
        m_crcFd = -1;
    }
}

MMKV* MMKV::defaultMMKV() {
    return mmkvWithID(DEFAULT_MMAP_ID);
}

void initialize() {
    g_instanceDic = new unordered_map<std::string, MMKV*>;
    g_instanceLock = new ThreadLock();

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

MMKV* MMKV::mmkvWithID(const std::string &mmapID) {

    if (mmapID.empty()) {
        return nullptr;
    }
    ScopedLock lock(g_instanceLock->getLock());

    auto itr = g_instanceDic->find(mmapID);
    if (itr != g_instanceDic->end()) {
        return itr->second;
    }
    auto kv = new MMKV(mmapID);
    (*g_instanceDic)[mmapID] = kv;
    return kv;
}

void MMKV::onExit() {
    ScopedLock lock(g_instanceLock->getLock());

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
    if(!isFileExist(m_path)) {
        m_fd = createFile(m_path);
    } else {
        m_fd = open(m_path.c_str(), O_RDWR, S_IRWXU);
    }
    if (m_fd <= 0) {
        MMKVError("fail to open:%s, %s", m_path.c_str(), strerror(errno));
    } else {
        m_size = 0;
        struct stat st = {};
        if (fstat(m_fd, &st) != -1) {
            m_size = (size_t)st.st_size;
        }
        // round up to (n * pagesize)
        if (m_size < DEFAULT_MMAP_SIZE || (m_size % DEFAULT_MMAP_SIZE != 0)) {
            m_size = ((m_size / DEFAULT_MMAP_SIZE) + 1 ) * DEFAULT_MMAP_SIZE;
            if (ftruncate(m_fd, m_size) != 0) {
                MMKVError("fail to truncate [%s] to size %zu, %s", m_mmapID.c_str(), m_size, strerror(errno));
                m_size = (size_t)st.st_size;
            }
        }
        m_ptr = (char*)mmap(NULL, m_size, PROT_READ|PROT_WRITE, MAP_SHARED, m_fd, 0);
        if (m_ptr == MAP_FAILED) {
            MMKVError("fail to mmap [%s], %s", m_mmapID.c_str(), strerror(errno));
        } else {
            const int offset = computeFixed32Size(0);
            try {
                m_actualSize = CodedInputData(m_ptr, offset).readFixed32();
            } catch (exception& exception) {
                MMKVError("%s", exception.what());
            }
            MMKVInfo("loading [%s] with %zu size in total, file size is %zu", m_mmapID.c_str(), m_actualSize, m_size);
            if (m_actualSize > 0) {
                if (m_actualSize < m_size && m_actualSize+offset <= m_size) {
                    if (checkFileCRCValid()) {
                        MMBuffer inputBuffer(m_ptr+offset, m_actualSize, true);
                        m_dic = MiniPBCoder::decodeMap(inputBuffer);
                        m_output = new CodedOutputData(m_ptr+offset+m_actualSize, m_size-offset-m_actualSize);
                    } else {
                        writeAcutalSize(0);
                        m_output = new CodedOutputData(m_ptr+offset, m_size-offset);
                        recaculateCRCDigest();
                    }
                } else {
                    MMKVError("load [%s] error: %zu size in total, file size is %zu", m_mmapID.c_str(), m_actualSize, m_size);
                    writeAcutalSize(0);
                    m_output = new CodedOutputData(m_ptr+offset, m_size-offset);
                    recaculateCRCDigest();
                }
            } else {
                m_output = new CodedOutputData(m_ptr+offset, m_size-offset);
                recaculateCRCDigest();
            }
            MMKVInfo("loaded [%s] with %zu values", m_mmapID.c_str(), m_dic.size());
        }
    }

    if (!isFileValid()) {
        MMKVWarning("[%s] file not valid", m_mmapID.c_str());
    }

//    [MMKV tryResetFileProtection:m_path];
//    [MMKV tryResetFileProtection:m_crcPath];
    m_needLoadFromFile = false;
}

void MMKV::checkLoadData() {
    //ScopedLock lock(m_lock.getLock());

    if (!m_needLoadFromFile) {
        return;
    }
    m_needLoadFromFile = false;
    loadFromFile();
}

void MMKV::clearAll() {
    MMKVInfo("cleaning all values [%s]", m_mmapID.c_str());

    ScopedLock lock(m_lock.getLock());

    if (m_needLoadFromFile) {
        if (remove(m_path.c_str()) != 0) {
            MMKVError("fail to remove file %s", m_path.c_str());
        }
        loadFromFile();
        return;
    }

    m_dic.clear();

    if (m_output) {
        delete m_output;
    }
    m_output = nullptr;

    if (m_ptr && m_ptr != MAP_FAILED) {
        // for truncate
        size_t size = std::min<size_t>(DEFAULT_MMAP_SIZE, m_size);
        memset(m_ptr, 0, size);
        if (msync(m_ptr, size, MS_SYNC) != 0) {
            MMKVError("fail to msync [%s]:%s", m_mmapID.c_str(), strerror(errno));
        }
        if (munmap(m_ptr, m_size) != 0) {
            MMKVError("fail to munmap [%s], %s", m_mmapID.c_str(), strerror(errno));
        }
    }
    m_ptr = nullptr;

    if (m_fd > 0) {
        if (m_size != DEFAULT_MMAP_SIZE) {
            MMKVInfo("truncating [%s] from %zu to %d", m_mmapID.c_str(), m_size, DEFAULT_MMAP_SIZE);
            if (ftruncate(m_fd, DEFAULT_MMAP_SIZE) != 0) {
                MMKVError("fail to truncate [%s] to size %d, %s", m_mmapID.c_str(), DEFAULT_MMAP_SIZE, strerror(errno));
            }
        }
        if (close(m_fd) != 0) {
            MMKVError("fail to close [%s], %s", m_mmapID.c_str(), strerror(errno));
        }
    }
    m_fd = 0;
    m_size = 0;
    m_actualSize = 0;
    m_crcDigest = 0;

    loadFromFile();
}

void MMKV::clearMemoryState() {
    MMKVInfo("clearMemoryState [%s]", m_mmapID.c_str());
    ScopedLock lock(m_lock.getLock());

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

bool MMKV::protectFromBackgroundWritting(size_t size, std::function<void(CodedOutputData*)> writerBlock) {
    try {
        if (m_isInBackground) {
            static const int offset = computeFixed32Size(0);
            static const int pagesize = getpagesize();
            size_t realOffset = offset + m_actualSize - size;
            size_t pageOffset = (realOffset / pagesize) * pagesize;
            size_t pointerOffset = realOffset - pageOffset;
            size_t mmapSize = offset + m_actualSize - pageOffset;
            char* ptr = m_ptr+pageOffset;
            if (mlock(ptr, mmapSize) != 0) {
                MMKVError("fail to mlock [%s], %s", m_mmapID.c_str(), strerror(errno));
                // just fail on this condition, otherwise app will crash anyway
                //writerBlock(m_output);
                return false;
            } else {
                CodedOutputData output(ptr + pointerOffset, size);
                writerBlock(&output);
                m_output->seek(size);
            }
            munlock(ptr, mmapSize);
        } else {
            writerBlock(m_output);
        }
    } catch (exception& exception) {
        MMKVError("%s", exception.what());
        return false;
    }

    return true;
}

// since we use append mode, when -[setData: forKey:] many times, space may not be enough
// try a full rewrite to make space
bool MMKV::ensureMemorySize(size_t newSize) {
    checkLoadData();

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
        bool ret = protectFromBackgroundWritting(m_actualSize, [&](CodedOutputData *output) {
            output->writeRawData(data);
        });
        if (ret) {
            recaculateCRCDigest();
        }
        return ret;
    }
    return true;
}

bool MMKV::writeAcutalSize(size_t actualSize) {
    assert(m_ptr != 0);
    assert(m_ptr != MAP_FAILED);

    char* actualSizePtr = m_ptr;
    char* tmpPtr = NULL;
    static const int offset = computeFixed32Size(0);

    if (m_isInBackground) {
        tmpPtr = m_ptr;
        if (mlock(tmpPtr, offset) != 0) {
            MMKVError("fail to mmap [%s], %d:%s", m_mmapID.c_str(), errno, strerror(errno));
            // just fail on this condition, otherwise app will crash anyway
            return false;
        } else {
            actualSizePtr = tmpPtr;
        }
    }

    try {
        CodedOutputData output(actualSizePtr, offset);
        output.writeFixed32((int32_t)actualSize);
    } catch(exception& exception) {
        MMKVError("%s", exception.what());
    }
    m_actualSize = actualSize;

    if (tmpPtr && tmpPtr != MAP_FAILED) {
        munlock(tmpPtr, offset);
    }
    return true;
}

const MMBuffer& MMKV::getDataForKey(const std::string &key) {
    ScopedLock lock(m_lock.getLock());
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

    ScopedLock lock(m_lock.getLock());
    bool hasEnoughSize = ensureMemorySize(size);

    if (!hasEnoughSize || !isFileValid()) {
        return false;
    }
    if (m_actualSize == 0) {
        auto allData = MiniPBCoder::encodeDataWithObject(m_dic);
        if (allData.length() > 0) {
            bool ret = writeAcutalSize(allData.length());
            if (ret) {
                ret = protectFromBackgroundWritting(m_actualSize, [&](CodedOutputData *output) {
                    output->writeRawData(allData);		// note: don't write size of data
                });
                if (ret) {
                    recaculateCRCDigest();
                }
            }
            return ret;
        }
        return false;
    } else {
        bool ret = writeAcutalSize(m_actualSize + size);
        if (ret) {
            static const int offset = computeFixed32Size(0);
            ret = protectFromBackgroundWritting(size, [&](CodedOutputData *output) {
                output->writeString(key);
                output->writeData(data);				// note: write size of data
            });
            if (ret) {
                updateCRCDigest((const uint8_t*)m_ptr+offset+m_actualSize-size, size);
            }
        }
        return ret;
    }
}

bool MMKV::fullWriteback() {
    ScopedLock lock(m_lock.getLock());
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
    if (allData.length() > 0 && isFileValid()) {
        int offset = computeFixed32Size(0);
        if (allData.length() + offset <= m_size) {
            bool ret = writeAcutalSize(allData.length());
            if (ret) {
                delete m_output;
                m_output = new CodedOutputData(m_ptr+offset, m_size-offset);
                ret = protectFromBackgroundWritting(m_actualSize, [&](CodedOutputData *output) {
                    output->writeRawData(allData);		// note: don't write size of data
                });
                if (ret) {
                    recaculateCRCDigest();
                }
            }
            return ret;
        } else {
            // ensureMemorySize will extend file & full rewrite, no need to write back again
            return ensureMemorySize(allData.length()+offset-m_size);
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
        int offset = computeFixed32Size(0);
        m_crcDigest = (uint32_t)crc32(0, (const uint8_t*)m_ptr+offset, (uint32_t)m_actualSize);

        // for backward compatibility
        if (!isFileExist(m_crcPath)) {
            MMKVInfo("crc32 file not found:%s", m_crcPath.c_str());
            return true;
        }
        uint32_t crc32 = 0;
        {
            MMBuffer* data = readWholeFile(m_crcPath.c_str());
            if (data) {
                try {
                    CodedInputData input(data->getPtr(), data->length());
                    crc32 = input.readFixed32();
                } catch (exception& exception) {
                    MMKVError("%s", exception.what());
                }
                delete data;
            }
        }
        if (m_crcDigest == crc32) {
            return true;
        }
        MMKVError("check crc [%s] fail, crc32:%u, m_crcDigest:%u", m_mmapID.c_str(), crc32, m_crcDigest);
    }
    return false;
}

void MMKV::recaculateCRCDigest() {
    if (m_ptr && m_ptr != MAP_FAILED) {
        m_crcDigest = 0;
        int offset = computeFixed32Size(0);
        updateCRCDigest((const uint8_t *)m_ptr+offset, m_actualSize);
    }
}

void MMKV::updateCRCDigest(const uint8_t *ptr, size_t length) {
    if (!ptr) {
        return;
    }
    m_crcDigest = (uint32_t)crc32(m_crcDigest, ptr, (uint32_t)length);

    if (m_crcPtr == nullptr || m_crcPtr == MAP_FAILED) {
        prepareCRCFile();
    }
    if (m_crcPtr == nullptr || m_crcPtr == MAP_FAILED) {
        return;
    }

    static const size_t bufferLength = computeFixed32Size(0);
    if (m_isInBackground) {
        if (mlock(m_crcPtr, bufferLength) != 0) {
            MMKVError("fail to mlock crc [%s]-%p, %d:%s", m_mmapID.c_str(), m_crcPtr, errno, strerror(errno));
            // just fail on this condition, otherwise app will crash anyway
            return;
        }
    }

    try {
        CodedOutputData output(m_crcPtr, bufferLength);
        output.writeFixed32((int32_t)m_crcDigest);
    } catch (exception& e) {
        MMKVError("%s", e.what());
    }
    if (m_isInBackground) {
        munlock(m_crcPtr, bufferLength);
    }
}

void MMKV::prepareCRCFile() {
    if (m_crcPtr == nullptr || m_crcPtr == MAP_FAILED) {
        if (!isFileExist(m_crcPath.c_str())) {
            createFile(m_crcPath.c_str());
        }
        m_crcFd = open(m_crcPath.c_str(), O_RDWR, S_IRWXU);
        if (m_crcFd <= 0) {
            MMKVError("fail to open:%s, %s", m_crcPath.c_str(), strerror(errno));
            removeFile(m_crcPath);
        } else {
            size_t size = 0;
            struct stat st = {};
            if (fstat(m_crcFd, &st) != -1) {
                size = (size_t)st.st_size;
            }
            int fileLegth = DEFAULT_MMAP_SIZE;
            if (size < fileLegth) {
                size = fileLegth;
                if (ftruncate(m_crcFd, size) != 0) {
                    MMKVError("fail to truncate [%s] to size %zu, %s", m_crcPath.c_str(), size, strerror(errno));
                    close(m_crcFd);
                    m_crcFd = -1;
                    removeFile(m_crcPath);
                    return;
                }
            }
            m_crcPtr = (char*)mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, m_crcFd, 0);
            if (m_crcPtr == MAP_FAILED) {
                MMKVError("fail to mmap [%s], %s", m_crcPath.c_str(), strerror(errno));
                close(m_crcFd);
                m_crcFd = -1;
            }
        }
    }
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
    ScopedLock lock(m_lock.getLock());
    checkLoadData();
    return m_dic.find(key) != m_dic.end();
}

size_t MMKV::count() {
    ScopedLock lock(m_lock.getLock());
    checkLoadData();
    return m_dic.size();
}

size_t MMKV::totalSize() {
    ScopedLock lock(m_lock.getLock());
    checkLoadData();
    return m_size;
}

//void MMKV::enumerateKeys(std::function<void(const std::string &key, bool &stop)> block) {
//    MMKVInfo("enumerate [%s] begin", m_mmapID.c_str());
//    ScopedLock lock(m_lock.getLock());
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
    ScopedLock lock(m_lock.getLock());
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
    ScopedLock lock(m_lock.getLock());
    checkLoadData();
//    m_dic.erase(key);
//
//    fullWriteback();
    removeDataForKey(key);
}

void MMKV::removeValuesForKeys(const std::vector<std::string> &arrKeys) {
    if (arrKeys.empty()) {
        return;
    }
    if (arrKeys.size() == 1) {
        return removeValueForKey(arrKeys[0]);
    }

    ScopedLock lock(m_lock.getLock());
    checkLoadData();
    for (const auto& key : arrKeys) {
        m_dic.erase(key);
    }

    fullWriteback();
}

#pragma mark - file

void MMKV::sync() {
    ScopedLock lock(m_lock.getLock());
    if (m_needLoadFromFile || !isFileValid()) {
        return;
    }
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
            CodedInputData input(data->getPtr(), data->length());
            crcFile = input.readFixed32();
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

