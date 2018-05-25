//
// Created by Ling Guo on 2018/5/25.
//

#include "MmapedFile.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include "MMKVLog.h"
#include "MMBuffer.h"

using namespace std;

const int DEFAULT_MMAP_SIZE = getpagesize();

MmapedFile::MmapedFile(const std::string &path) : m_name(path) {
    if (!isFileExist(m_name.c_str())) {
        createFile(m_name.c_str());
    }
    m_fd = open(m_name.c_str(), O_RDWR, S_IRWXU);
    if (m_fd <= 0) {
        MMKVError("fail to open:%s, %s", m_name.c_str(), strerror(errno));
        removeFile(m_name);
    } else {
        m_segmentSize = 0;
        struct stat st = {};
        if (fstat(m_fd, &st) != -1) {
            m_segmentSize = (size_t)st.st_size;
        }
        int fileLegth = DEFAULT_MMAP_SIZE;
        if (m_segmentSize < fileLegth) {
            m_segmentSize = fileLegth;
            if (ftruncate(m_fd, m_segmentSize) != 0) {
                MMKVError("fail to truncate [%s] to size %zu, %s", m_name.c_str(), m_segmentSize, strerror(errno));
                close(m_fd);
                m_fd = -1;
                removeFile(m_name);
                return;
            }
        }
        m_segmentPtr = (char*)mmap(NULL, m_segmentSize, PROT_READ|PROT_WRITE, MAP_SHARED, m_fd, 0);
        if (m_segmentPtr == MAP_FAILED) {
            MMKVError("fail to mmap [%s], %s", m_name.c_str(), strerror(errno));
            close(m_fd);
            m_fd = -1;
        }
    }
}

MmapedFile::~MmapedFile() {
    if (m_segmentPtr != MAP_FAILED && m_segmentPtr != nullptr) {
        munmap(m_segmentPtr, m_segmentSize);
        m_segmentPtr = nullptr;
    }
    if (m_fd > 0) {
        close(m_fd);
        m_fd = -1;
    }
}

void* MmapedFile::getMemory() {
    return m_segmentPtr;
}

size_t MmapedFile::getFileSize() {
    return m_segmentSize;
}

bool MmapedFile::truncate(size_t length) {
    MMKVInfo("extending [%s] file size from %zu to %zu",
             m_name.c_str(), m_segmentSize, length);

    auto oldSize = m_segmentSize;
    m_segmentSize = length;
    // if we can't extend size, rollback to old state
    if (ftruncate(m_fd, m_segmentSize) != 0) {
        MMKVError("fail to truncate [%s] to size %zu, %s", m_name.c_str(), m_segmentSize, strerror(errno));
        m_segmentSize = oldSize;
        return false;
    }

    if (munmap(m_segmentPtr, oldSize) != 0) {
        MMKVError("fail to munmap [%s], %s", m_name.c_str(), strerror(errno));
    }
    m_segmentPtr = (char*)mmap(m_segmentPtr, m_segmentSize, PROT_READ|PROT_WRITE, MAP_SHARED, m_fd, 0);
    if (m_segmentPtr == MAP_FAILED) {
        MMKVError("fail to mmap [%s], %s", m_name.c_str(), strerror(errno));
        return false;
    }
    return true;
}

#pragma mark - file

bool isFileExist(const string& nsFilePath) {
    if(nsFilePath.empty()) {
        return false;
    }

    struct stat temp;
    return lstat(nsFilePath.c_str(), &temp) == 0;
}

int createFile(const string& nsFilePath) {
    // try create file at once
    int fd = open(nsFilePath.c_str(), O_RDWR | O_CREAT, S_IRWXU);
    if (fd <= 0) {
        MMKVError("fail to open:%s, %s", nsFilePath.c_str(), strerror(errno));
    } else {
        return fd;
    }

    // create parent directories
    char* path = strdup(nsFilePath.c_str());
    auto parentSucess = mkpath(dirname(path));
    free(path);
    if (!parentSucess) {
        return -1;
    }

    // create file again
    fd = open(nsFilePath.c_str(), O_RDWR | O_CREAT, S_IRWXU);
    if (fd <= 0) {
        MMKVError("fail to open:%s, %s", nsFilePath.c_str(), strerror(errno));
    }
    return fd;
}

bool mkpath(char *path) {
    struct stat sb= {};
    bool done = false;
    char* slash = path;

    while (!done) {
        slash += strspn(slash, "/");
        slash += strcspn(slash, "/");

        done = (*slash == '\0');
        *slash = '\0';

        if (stat(path, &sb) != 0) {
            if (errno != ENOENT || mkdir(path, 0777) != 0) {
                MMKVWarning("%s", path);
                return false;
            }
        } else if (!S_ISDIR(sb.st_mode)) {
            MMKVWarning("%s: %s", path, strerror(ENOTDIR));
            return false;
        }

        *slash = '/';
    }

    return true;
}

bool removeFile(const string &nsFilePath) {
    int ret = rmdir(nsFilePath.c_str());
    if (ret != 0) {
        MMKVError("remove file failed. filePath=%s, err=%s", nsFilePath.c_str(), strerror(errno));
        return false;
    }
    return true;
}

MMBuffer* readWholeFile(const char *path) {
    MMBuffer* buffer = nullptr;
    int fd = open(path, O_RDONLY);
    if (fd > 0) {
        auto fileLength = lseek(fd, 0, SEEK_END);
        if (fileLength > 0) {
            buffer = new MMBuffer(fileLength);
            lseek(fd, 0, SEEK_SET);
            auto readSize = read(fd, buffer->getPtr(), fileLength);
            if (readSize != -1) {
//                fileSize = readSize;
            } else {
                MMKVWarning("fail to read %s: %s", path, strerror(errno));

                delete buffer;
                buffer = nullptr;
            }
        }
        close(fd);
    } else {
        MMKVWarning("fail to open %s: %s", path, strerror(errno));
    }
    return buffer;
}
