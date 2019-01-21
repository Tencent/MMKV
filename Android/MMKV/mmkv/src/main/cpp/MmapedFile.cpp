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

#include "MmapedFile.h"
#include "InterProcessLock.h"
#include "MMBuffer.h"
#include "MMKVLog.h"
#include "ScopedLock.hpp"
#include <cerrno>
#include <fcntl.h>
#include <libgen.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

const int DEFAULT_MMAP_SIZE = getpagesize();

MmapedFile::MmapedFile(const std::string &path, size_t size, bool fileType)
    : m_name(path), m_fd(-1), m_segmentPtr(nullptr), m_segmentSize(0), m_fileType(fileType) {
    if (m_fileType == MMAP_FILE) {
        m_fd = open(m_name.c_str(), O_RDWR | O_CREAT, S_IRWXU);
        if (m_fd < 0) {
            MMKVError("fail to open:%s, %s", m_name.c_str(), strerror(errno));
        } else {
            FileLock fileLock(m_fd);
            InterProcessLock lock(&fileLock, ExclusiveLockType);
            SCOPEDLOCK(lock);

            struct stat st = {};
            if (fstat(m_fd, &st) != -1) {
                m_segmentSize = static_cast<size_t>(st.st_size);
            }
            if (m_segmentSize < DEFAULT_MMAP_SIZE) {
                m_segmentSize = static_cast<size_t>(DEFAULT_MMAP_SIZE);
                if (ftruncate(m_fd, m_segmentSize) != 0 || !zeroFillFile(m_fd, 0, m_segmentSize)) {
                    MMKVError("fail to truncate [%s] to size %zu, %s", m_name.c_str(),
                              m_segmentSize, strerror(errno));
                    close(m_fd);
                    m_fd = -1;
                    removeFile(m_name);
                    return;
                }
            }
            m_segmentPtr =
                (char *) mmap(nullptr, m_segmentSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
            if (m_segmentPtr == MAP_FAILED) {
                MMKVError("fail to mmap [%s], %s", m_name.c_str(), strerror(errno));
                close(m_fd);
                m_fd = -1;
                m_segmentPtr = nullptr;
            }
        }
    } else {
        m_fd = open(ASHMEM_NAME_DEF, O_RDWR);
        if (m_fd < 0) {
            MMKVError("fail to open ashmem:%s, %s", m_name.c_str(), strerror(errno));
        } else {
            if (ioctl(m_fd, ASHMEM_SET_NAME, m_name.c_str()) != 0) {
                MMKVError("fail to set ashmem name:%s, %s", m_name.c_str(), strerror(errno));
            } else if (ioctl(m_fd, ASHMEM_SET_SIZE, size) != 0) {
                MMKVError("fail to set ashmem:%s, size %zu, %s", m_name.c_str(), size,
                          strerror(errno));
            } else {
                m_segmentSize = static_cast<size_t>(size);
                m_segmentPtr = (char *) mmap(nullptr, m_segmentSize, PROT_READ | PROT_WRITE,
                                             MAP_SHARED, m_fd, 0);
                if (m_segmentPtr == MAP_FAILED) {
                    MMKVError("fail to mmap [%s], %s", m_name.c_str(), strerror(errno));
                    m_segmentPtr = nullptr;
                } else {
                    return;
                }
            }
            close(m_fd);
            m_fd = -1;
        }
    }
}

MmapedFile::MmapedFile(int ashmemFD)
    : m_name(""), m_fd(ashmemFD), m_segmentPtr(nullptr), m_segmentSize(0), m_fileType(MMAP_ASHMEM) {
    if (m_fd < 0) {
        MMKVError("fd %d invalid", m_fd);
    } else {
        char name[ASHMEM_NAME_LEN] = {0};
        if (ioctl(m_fd, ASHMEM_GET_NAME, name) != 0) {
            MMKVError("fail to get ashmem name:%d, %s", m_fd, strerror(errno));
        } else {
            m_name = string(name);
            int size = ioctl(m_fd, ASHMEM_GET_SIZE, nullptr);
            if (size < 0) {
                MMKVError("fail to get ashmem size:%s, %s", m_name.c_str(), strerror(errno));
            } else {
                m_segmentSize = static_cast<size_t>(size);
                MMKVInfo("ashmem verified, name:%s, size:%zu", m_name.c_str(), m_segmentSize);
                m_segmentPtr = (char *) mmap(nullptr, m_segmentSize, PROT_READ | PROT_WRITE,
                                             MAP_SHARED, m_fd, 0);
                if (m_segmentPtr == MAP_FAILED) {
                    MMKVError("fail to mmap [%s], %s", m_name.c_str(), strerror(errno));
                    m_segmentPtr = nullptr;
                }
            }
        }
    }
}

MmapedFile::~MmapedFile() {
    if (m_segmentPtr != MAP_FAILED && m_segmentPtr != nullptr) {
        munmap(m_segmentPtr, m_segmentSize);
        m_segmentPtr = nullptr;
    }
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
}

#pragma mark - file

bool isFileExist(const string &nsFilePath) {
    if (nsFilePath.empty()) {
        return false;
    }

    struct stat temp;
    return lstat(nsFilePath.c_str(), &temp) == 0;
}

bool mkPath(char *path) {
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

bool createFile(const std::string &filePath) {
    bool ret = false;

    // try create at once
    auto fd = open(filePath.c_str(), O_RDWR | O_CREAT, S_IRWXU);
    if (fd >= 0) {
        close(fd);
        ret = true;
    } else {
        // create parent dir
        char *path = strdup(filePath.c_str());
        auto ptr = strrchr(path, '/');
        if (ptr) {
            *ptr = '\0';
        }
        if (mkPath(path)) {
            // try again
            fd = open(filePath.c_str(), O_RDWR | O_CREAT, S_IRWXU);
            if (fd >= 0) {
                close(fd);
                ret = true;
            } else {
                MMKVWarning("fail to create file %s, %s", filePath.c_str(), strerror(errno));
            }
        }
        free(path);
    }
    return ret;
}

bool removeFile(const string &nsFilePath) {
    int ret = unlink(nsFilePath.c_str());
    if (ret != 0) {
        MMKVError("remove file failed. filePath=%s, err=%s", nsFilePath.c_str(), strerror(errno));
        return false;
    }
    return true;
}

MMBuffer *readWholeFile(const char *path) {
    MMBuffer *buffer = nullptr;
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        auto fileLength = lseek(fd, 0, SEEK_END);
        if (fileLength > 0) {
            buffer = new MMBuffer(static_cast<size_t>(fileLength));
            lseek(fd, 0, SEEK_SET);
            auto readSize = read(fd, buffer->getPtr(), static_cast<size_t>(fileLength));
            if (readSize != -1) {
                //fileSize = readSize;
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

bool zeroFillFile(int fd, size_t startPos, size_t size) {
    if (fd < 0) {
        return false;
    }

    if (lseek(fd, startPos, SEEK_SET) < 0) {
        MMKVError("fail to lseek fd[%d], error:%s", fd, strerror(errno));
        return false;
    }

    static const char zeros[4096] = {0};
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
