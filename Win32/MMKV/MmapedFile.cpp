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

#include "pch.h"

#include "InterProcessLock.h"
#include "MMBuffer.h"
#include "MMKVLog.h"
#include "MmapedFile.h"
#include "ScopedLock.hpp"

using namespace std;

namespace mmkv {

const size_t DEFAULT_MMAP_SIZE = getpagesize();

std::wstring string2wstring(const std::string &str) {
    auto length = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    auto buffer = new wchar_t[length];
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer, length);
    wstring result(buffer);
    delete[] buffer;
    return result;
}

MmapedFile::MmapedFile(const std::wstring &path, size_t size)
    : m_name(path), m_file(INVALID_HANDLE_VALUE), m_segmentPtr(nullptr), m_segmentSize(0) {
    m_file = CreateFile(m_name.c_str(), GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (m_file == INVALID_HANDLE_VALUE) {
        MMKVError("fail to open:%ws, %d", m_name.c_str(), GetLastError());
    } else {
        FileLock fileLock(m_file);
        InterProcessLock lock(&fileLock, ExclusiveLockType);
        SCOPEDLOCK(lock);

        getfilesize(m_file, m_segmentSize);
        if (m_segmentSize < DEFAULT_MMAP_SIZE) {
            m_segmentSize = static_cast<size_t>(DEFAULT_MMAP_SIZE);
            if (!ftruncate(m_file, m_segmentSize) || !zeroFillFile(m_file, 0, m_segmentSize)) {
                MMKVError("fail to truncate [%ws] to size %zu, %d", m_name.c_str(), m_segmentSize,
                          GetLastError());
                CloseHandle(m_file);
                m_file = INVALID_HANDLE_VALUE;
                removeFile(m_name);
                return;
            }
        }
        m_fileMapping = CreateFileMapping(m_file, nullptr, PAGE_READWRITE, 0, 0, nullptr);
        if (!m_fileMapping) {
            MMKVError("fail to CreateFileMapping [%ws], %d", m_name.c_str(), GetLastError());
            CloseHandle(m_file);
            m_file = INVALID_HANDLE_VALUE;
            removeFile(m_name);
            return;
        }
        m_segmentPtr = MapViewOfFile(m_fileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (!m_segmentPtr) {
            MMKVError("fail to MapViewOfFile [%ws], %d", m_name.c_str(), GetLastError());
            CloseHandle(m_fileMapping);
            m_fileMapping = NULL;
            CloseHandle(m_file);
            m_file = INVALID_HANDLE_VALUE;
            removeFile(m_name);
            return;
        }
    }
}

MmapedFile::~MmapedFile() {
    if (m_segmentPtr) {
        UnmapViewOfFile(m_segmentPtr);
        m_segmentPtr = nullptr;
    }
    if (m_fileMapping) {
        CloseHandle(m_fileMapping);
        m_fileMapping = nullptr;
    }
    if (m_file != INVALID_HANDLE_VALUE) {
        CloseHandle(m_file);
        m_file = INVALID_HANDLE_VALUE;
    }
}

// file

size_t getpagesize(void) {
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return system_info.dwPageSize;
}

bool isFileExist(const wstring &nsFilePath) {
    if (nsFilePath.empty()) {
        return false;
    }
    auto attribute = GetFileAttributes(nsFilePath.c_str());
    return (attribute != INVALID_FILE_ATTRIBUTES);
}

bool mkPath(const std::wstring &str) {
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
                delete[] path;
                return false;
            }
        } else if (!(attribute & FILE_ATTRIBUTE_DIRECTORY)) {
            MMKVError("%ws attribute:%d not a directry", str.c_str(), attribute);
            delete[] path;
            return false;
        }

        *slash = L'\\';
    }
    free(path);
    return true;
}

bool removeFile(const wstring &nsFilePath) {
    if (!DeleteFile(nsFilePath.c_str())) {
        MMKVError("remove file failed. filePath=%ws, %d", nsFilePath.c_str(), GetLastError());
        return false;
    }
    return true;
}

MMBuffer *readWholeFile(const std::wstring &nsFilePath) {
    MMBuffer *buffer = nullptr;
    auto fd = CreateFile(nsFilePath.c_str(), GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fd != INVALID_HANDLE_VALUE) {
        size_t fileLength = 0;
        getfilesize(fd, fileLength);
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

bool zeroFillFile(HANDLE file, size_t startPos, size_t size) {
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

bool ftruncate(HANDLE file, size_t size) {
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

bool getfilesize(HANDLE file, size_t &size) {
    LARGE_INTEGER filesize = {0};
    if (GetFileSizeEx(file, &filesize)) {
        size = static_cast<size_t>(filesize.QuadPart);
        return true;
    }
    return false;
}

} // namespace mmkv
