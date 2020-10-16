/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2019 THL A29 Limited, a Tencent company.
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

#ifdef MMKV_WIN32

#    include "InterProcessLock.h"
#    include "MMBuffer.h"
#    include "MMKVLog.h"
#    include "ScopedLock.hpp"
#    include "ThreadLock.h"
#    include <cassert>

using namespace std;

namespace mmkv {

static bool getFileSize(MMKVFileHandle_t fd, size_t &size);
static bool ftruncate(MMKVFileHandle_t file, size_t size);

MemoryFile::MemoryFile(const MMKVPath_t &path)
    : m_name(path), m_fd(INVALID_HANDLE_VALUE), m_fileMapping(nullptr), m_ptr(nullptr), m_size(0) {
    reloadFromFile();
}

bool MemoryFile::truncate(size_t size) {
    if (m_fd < 0) {
        return false;
    }
    if (size == m_size) {
        return true;
    }

    auto oldSize = m_size;
    m_size = size;
    // round up to (n * pagesize)
    if (m_size < DEFAULT_MMAP_SIZE || (m_size % DEFAULT_MMAP_SIZE != 0)) {
        m_size = ((m_size / DEFAULT_MMAP_SIZE) + 1) * DEFAULT_MMAP_SIZE;
    }

    if (!ftruncate(m_fd, m_size)) {
        MMKVError("fail to truncate [%ws] to size %zu", m_name.c_str(), m_size);
        m_size = oldSize;
        return false;
    }
    if (m_size > oldSize) {
        if (!zeroFillFile(m_fd, oldSize, m_size - oldSize)) {
            MMKVError("fail to zeroFile [%ws] to size %zu", m_name.c_str(), m_size);
            m_size = oldSize;
            return false;
        }
    }

    if (m_ptr) {
        if (!UnmapViewOfFile(m_ptr)) {
            MMKVError("fail to munmap [%ws], %d", m_name.c_str(), GetLastError());
        }
        m_ptr = nullptr;
    }
    if (m_fileMapping) {
        CloseHandle(m_fileMapping);
        m_fileMapping = nullptr;
    }
    auto ret = mmap();
    if (!ret) {
        doCleanMemoryCache(true);
    }
    return ret;
}

bool MemoryFile::msync(SyncFlag syncFlag) {
    if (m_ptr) {
        if (FlushViewOfFile(m_ptr, m_size)) {
            if (syncFlag == MMKV_SYNC) {
                if (!FlushFileBuffers(m_fd)) {
                    MMKVError("fail to FlushFileBuffers [%ws]:%d", m_name.c_str(), GetLastError());
                    return false;
                }
            }
            return true;
        }
        MMKVError("fail to FlushViewOfFile [%ws]:%d", m_name.c_str(), GetLastError());
        return false;
    }
    return false;
}

bool MemoryFile::mmap() {
    m_fileMapping = CreateFileMapping(m_fd, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (!m_fileMapping) {
        MMKVError("fail to CreateFileMapping [%ws], %d", m_name.c_str(), GetLastError());
        return false;
    } else {
        m_ptr = (char *) MapViewOfFile(m_fileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (!m_ptr) {
            MMKVError("fail to mmap [%ws], %d", m_name.c_str(), GetLastError());
            return false;
        }
    }

    return true;
}

void MemoryFile::reloadFromFile() {
    if (isFileValid()) {
        MMKVWarning("calling reloadFromFile while the cache [%ws] is still valid", m_name.c_str());
        assert(0);
        clearMemoryCache();
    }

    m_fd =
        CreateFile(m_name.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                   nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (m_fd == INVALID_HANDLE_VALUE) {
        MMKVError("fail to open:%ws, %d", m_name.c_str(), GetLastError());
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
    }
}

void MemoryFile::doCleanMemoryCache(bool forceClean) {
    if (m_ptr) {
        UnmapViewOfFile(m_ptr);
        m_ptr = nullptr;
    }
    if (m_fileMapping) {
        CloseHandle(m_fileMapping);
        m_fileMapping = nullptr;
    }
    if (m_fd != INVALID_HANDLE_VALUE) {
        CloseHandle(m_fd);
        m_fd = INVALID_HANDLE_VALUE;
    }
}

size_t MemoryFile::getActualFileSize() {
    size_t size = 0;
    mmkv::getFileSize(m_fd, size);
    return size;
}

size_t getPageSize() {
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return system_info.dwPageSize;
}

bool isFileExist(const MMKVPath_t &nsFilePath) {
    if (nsFilePath.empty()) {
        return false;
    }
    auto attribute = GetFileAttributes(nsFilePath.c_str());
    return (attribute != INVALID_FILE_ATTRIBUTES);
}

bool mkPath(const MMKVPath_t &str) {
    wchar_t *path = _wcsdup(str.c_str());

    bool done = false;
    wchar_t *slash = path;

    while (!done) {
        slash += wcsspn(slash, L"\\");
        slash += wcscspn(slash, L"\\");

        done = (*slash == L'\0');
        *slash = L'\0';

        auto attribute = GetFileAttributes(path);
        if (attribute == INVALID_FILE_ATTRIBUTES) {
            if (!CreateDirectory(path, nullptr)) {
                MMKVError("fail to create dir:%ws, %d", str.c_str(), GetLastError());
                free(path);
                return false;
            }
        } else if (!(attribute & FILE_ATTRIBUTE_DIRECTORY)) {
            MMKVError("%ws attribute:%d not a directry", str.c_str(), attribute);
            free(path);
            return false;
        }

        *slash = L'\\';
    }
    free(path);
    return true;
}

MMBuffer *readWholeFile(const MMKVPath_t &nsFilePath) {
    MMBuffer *buffer = nullptr;
    auto fd = CreateFile(nsFilePath.c_str(), GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fd != INVALID_HANDLE_VALUE) {
        size_t fileLength = 0;
        getFileSize(fd, fileLength);
        if (fileLength > 0) {
            buffer = new MMBuffer(static_cast<size_t>(fileLength));
            SetFilePointer(fd, 0, 0, FILE_BEGIN);
            DWORD readSize = 0;
            if (ReadFile(fd, buffer->getPtr(), fileLength, &readSize, nullptr)) {
                //fileSize = readSize;
            } else {
                MMKVWarning("fail to read %ws: %d", nsFilePath.c_str(), GetLastError());
                delete buffer;
                buffer = nullptr;
            }
        }
        CloseHandle(fd);
    } else {
        MMKVWarning("fail to open %ws: %d", nsFilePath.c_str(), GetLastError());
    }
    return buffer;
}

bool zeroFillFile(MMKVFileHandle_t file, size_t startPos, size_t size) {
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    if (size == 0) {
        return true;
    }

    LARGE_INTEGER position;
    position.QuadPart = startPos;
    if (!SetFilePointerEx(file, position, nullptr, FILE_BEGIN)) {
        MMKVError("fail to lseek fd[%p], error:%d", file, GetLastError());
        return false;
    }

    static const char zeros[4096] = {0};
    while (size >= sizeof(zeros)) {
        DWORD bytesWritten = 0;
        if (!WriteFile(file, zeros, sizeof(zeros), &bytesWritten, nullptr)) {
            MMKVError("fail to write fd[%p], error:%d", file, GetLastError());
            return false;
        }
        size -= bytesWritten;
    }
    if (size > 0) {
        DWORD bytesWritten = 0;
        if (!WriteFile(file, zeros, size, &bytesWritten, nullptr)) {
            MMKVError("fail to write fd[%p], error:%d", file, GetLastError());
            return false;
        }
    }
    return true;
}

static bool ftruncate(MMKVFileHandle_t file, size_t size) {
    LARGE_INTEGER large;
    large.QuadPart = size;
    if (SetFilePointerEx(file, large, 0, FILE_BEGIN)) {
        if (SetEndOfFile(file)) {
            return true;
        }
        MMKVError("fail to SetEndOfFile:%d", GetLastError());
        return false;
    } else {
        MMKVError("fail to SetFilePointer:%d", GetLastError());
        return false;
    }
}

static bool getFileSize(MMKVFileHandle_t fd, size_t &size) {
    LARGE_INTEGER filesize = {0};
    if (GetFileSizeEx(fd, &filesize)) {
        size = static_cast<size_t>(filesize.QuadPart);
        return true;
    }
    return false;
}

} // namespace mmkv

std::wstring string2MMKVPath_t(const std::string &str) {
    auto length = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    auto buffer = new wchar_t[length];
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer, length);
    wstring result(buffer);
    delete[] buffer;
    return result;
}

#endif // MMKV_WIN32
