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

#include "MemoryFile.h"

#ifndef MMKV_WIN32

#    include "InterProcessLock.h"
#    include "MMBuffer.h"
#    include "MMKVLog.h"
#    include "ScopedLock.hpp"
#    include <cerrno>
#    include <fcntl.h>
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <unistd.h>

using namespace std;

namespace mmkv {

static bool getFileSize(int fd, size_t &size);

#    ifdef MMKV_ANDROID
extern size_t ASharedMemory_getSize(int fd);
#    else
MemoryFile::MemoryFile(const MMKVPath_t &path) : m_name(path), m_fd(-1), m_ptr(nullptr), m_size(0) {
    reloadFromFile();
}
#    endif // MMKV_ANDROID

#    ifdef MMKV_IOS
void tryResetFileProtection(const string &path);
#    endif

bool MemoryFile::truncate(size_t size) {
    if (m_fd < 0) {
        return false;
    }
    if (size == m_size) {
        return true;
    }
#    ifdef MMKV_ANDROID
    if (m_fileType == MMFILE_TYPE_ASHMEM) {
        if (size > m_size) {
            MMKVError("ashmem %s reach size limit:%zu, consider configure with larger size", m_name.c_str(), m_size);
        } else {
            MMKVInfo("no way to trim ashmem %s from %zu to smaller size %zu", m_name.c_str(), m_size, size);
        }
        return false;
    }
#    endif // MMKV_ANDROID

    auto oldSize = m_size;
    m_size = size;
    // round up to (n * pagesize)
    if (m_size < DEFAULT_MMAP_SIZE || (m_size % DEFAULT_MMAP_SIZE != 0)) {
        m_size = ((m_size / DEFAULT_MMAP_SIZE) + 1) * DEFAULT_MMAP_SIZE;
    }

    if (::ftruncate(m_fd, static_cast<off_t>(m_size)) != 0) {
        MMKVError("fail to truncate [%s] to size %zu, %s", m_name.c_str(), m_size, strerror(errno));
        m_size = oldSize;
        return false;
    }
    if (m_size > oldSize) {
        if (!zeroFillFile(m_fd, oldSize, m_size - oldSize)) {
            MMKVError("fail to zeroFile [%s] to size %zu, %s", m_name.c_str(), m_size, strerror(errno));
            m_size = oldSize;
            return false;
        }
    }

    if (m_ptr) {
        if (munmap(m_ptr, oldSize) != 0) {
            MMKVError("fail to munmap [%s], %s", m_name.c_str(), strerror(errno));
        }
    }
    auto ret = mmap();
    if (!ret) {
        doCleanMemoryCache(true);
    }
    return ret;
}

bool MemoryFile::msync(SyncFlag syncFlag) {
    if (m_ptr) {
        auto ret = ::msync(m_ptr, m_size, syncFlag ? MS_SYNC : MS_ASYNC);
        if (ret == 0) {
            return true;
        }
        MMKVError("fail to msync [%s], %s", m_name.c_str(), strerror(errno));
    }
    return false;
}

bool MemoryFile::mmap() {
    m_ptr = (char *) ::mmap(m_ptr, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
    if (m_ptr == MAP_FAILED) {
        MMKVError("fail to mmap [%s], %s", m_name.c_str(), strerror(errno));
        m_ptr = nullptr;
        return false;
    }

    return true;
}

void MemoryFile::reloadFromFile() {
#    ifdef MMKV_ANDROID
    if (m_fileType == MMFILE_TYPE_ASHMEM) {
        return;
    }
#    endif
    if (isFileValid()) {
        MMKVWarning("calling reloadFromFile while the cache [%s] is still valid", m_name.c_str());
        MMKV_ASSERT(0);
        clearMemoryCache();
    }

    m_fd = open(m_name.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, S_IRWXU);
    if (m_fd < 0) {
        MMKVError("fail to open:%s, %s", m_name.c_str(), strerror(errno));
    } else {
        FileLock fileLock(m_fd);
        InterProcessLock lock(&fileLock, ExclusiveLockType);
        SCOPED_LOCK(&lock);

        mmkv::getFileSize(m_fd, m_size);
        // round up to (n * pagesize)
        if (m_size < DEFAULT_MMAP_SIZE || (m_size % DEFAULT_MMAP_SIZE != 0)) {
            size_t roundSize = ((m_size / DEFAULT_MMAP_SIZE) + 1) * DEFAULT_MMAP_SIZE;
            truncate(roundSize);
        } else {
            auto ret = mmap();
            if (!ret) {
                doCleanMemoryCache(true);
            }
        }
#    ifdef MMKV_IOS
        tryResetFileProtection(m_name);
#    endif
    }
}

void MemoryFile::doCleanMemoryCache(bool forceClean) {
#    ifdef MMKV_ANDROID
    if (m_fileType == MMFILE_TYPE_ASHMEM && !forceClean) {
        return;
    }
#    endif
    if (m_ptr && m_ptr != MAP_FAILED) {
        if (munmap(m_ptr, m_size) != 0) {
            MMKVError("fail to munmap [%s], %s", m_name.c_str(), strerror(errno));
        }
    }
    m_ptr = nullptr;

    if (m_fd >= 0) {
        if (::close(m_fd) != 0) {
            MMKVError("fail to close [%s], %s", m_name.c_str(), strerror(errno));
        }
    }
    m_fd = -1;
    m_size = 0;
}

size_t MemoryFile::getActualFileSize() {
#    ifdef MMKV_ANDROID
    if (m_fileType == MMFILE_TYPE_ASHMEM) {
        return ASharedMemory_getSize(m_fd);
    }
#    endif
    size_t size = 0;
    mmkv::getFileSize(m_fd, size);
    return size;
}

bool isFileExist(const string &nsFilePath) {
    if (nsFilePath.empty()) {
        return false;
    }

    struct stat temp = {};
    return lstat(nsFilePath.c_str(), &temp) == 0;
}

extern bool mkPath(const MMKVPath_t &str) {
    char *path = strdup(str.c_str());

    struct stat sb = {};
    bool done = false;
    char *slash = path;

    while (!done) {
        slash += strspn(slash, "/");
        slash += strcspn(slash, "/");

        done = (*slash == '\0');
        *slash = '\0';

        if (stat(path, &sb) != 0) {
            if (errno != ENOENT || mkdir(path, 0777) != 0) {
                MMKVWarning("%s : %s", path, strerror(errno));
                free(path);
                return false;
            }
        } else if (!S_ISDIR(sb.st_mode)) {
            MMKVWarning("%s: %s", path, strerror(ENOTDIR));
            free(path);
            return false;
        }

        *slash = '/';
    }
    free(path);

    return true;
}

MMBuffer *readWholeFile(const MMKVPath_t &path) {
    MMBuffer *buffer = nullptr;
    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
        auto fileLength = lseek(fd, 0, SEEK_END);
        if (fileLength > 0) {
            buffer = new MMBuffer(static_cast<size_t>(fileLength));
            lseek(fd, 0, SEEK_SET);
            auto readSize = read(fd, buffer->getPtr(), static_cast<size_t>(fileLength));
            if (readSize != -1) {
                //fileSize = readSize;
            } else {
                MMKVWarning("fail to read %s: %s", path.c_str(), strerror(errno));

                delete buffer;
                buffer = nullptr;
            }
        }
        close(fd);
    } else {
        MMKVWarning("fail to open %s: %s", path.c_str(), strerror(errno));
    }
    return buffer;
}

bool zeroFillFile(int fd, size_t startPos, size_t size) {
    if (fd < 0) {
        return false;
    }

    if (lseek(fd, static_cast<off_t>(startPos), SEEK_SET) < 0) {
        MMKVError("fail to lseek fd[%d], error:%s", fd, strerror(errno));
        return false;
    }

    static const char zeros[4096] = {};
    while (size >= sizeof(zeros)) {
        if (write(fd, zeros, sizeof(zeros)) < 0) {
            MMKVError("fail to write fd[%d], error:%s", fd, strerror(errno));
            return false;
        }
        size -= sizeof(zeros);
    }
    if (size > 0) {
        if (write(fd, zeros, size) < 0) {
            MMKVError("fail to write fd[%d], error:%s", fd, strerror(errno));
            return false;
        }
    }
    return true;
}

static bool getFileSize(int fd, size_t &size) {
    struct stat st = {};
    if (fstat(fd, &st) != -1) {
        size = (size_t) st.st_size;
        return true;
    }
    return false;
}

size_t getPageSize() {
    return static_cast<size_t>(getpagesize());
}

} // namespace mmkv

#endif // MMKV_WIN32
