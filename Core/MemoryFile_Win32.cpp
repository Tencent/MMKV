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
#    include <strsafe.h>

using namespace std;

namespace mmkv {

static bool getFileSize(MMKVFileHandle_t fd, size_t &size);
static bool ftruncate(MMKVFileHandle_t file, size_t size);

File::File(MMKVPath_t path, OpenFlag flag) : m_path(std::move(path)), m_fd(INVALID_HANDLE_VALUE), m_flag(flag) {
    open();
}

static pair<int, int> OpenFlag2NativeFlag(OpenFlag flag) {
    int access = 0, create = OPEN_EXISTING;
    if (flag & OpenFlag::ReadWrite) {
        access = (GENERIC_READ | GENERIC_WRITE);
    } else if (flag & OpenFlag::ReadOnly) {
        access |= GENERIC_READ;
    } else if (flag & OpenFlag::WriteOnly) {
        access |= GENERIC_WRITE;
    }
    if (flag & OpenFlag::Create) {
        create = OPEN_ALWAYS;
    }
    if (flag & OpenFlag::Excel) {
        access = CREATE_NEW;
    }
    if (flag & OpenFlag::Truncate) {
        access = CREATE_ALWAYS;
    }
    return {access, create};
}

bool File::open() {
    if (isFileValid()) {
        return true;
    }
    auto pair = OpenFlag2NativeFlag(m_flag);
    m_fd = CreateFile(m_path.c_str(), pair.first, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                      pair.second, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!isFileValid()) {
        MMKVError("fail to open:[%ls], %d", m_path.c_str(), GetLastError());
        return false;
    }
    MMKVInfo("open fd[%p], %ls", m_fd, m_path.c_str());
    return true;
}

void File::close() {
    if (isFileValid()) {
        MMKVInfo("closing fd[%p], %ls", m_fd, m_path.c_str());
        if (CloseHandle(m_fd)) {
            m_fd = INVALID_HANDLE_VALUE;
        } else {
            MMKVError("fail to close [%ls], %d", m_path.c_str(), GetLastError());
        }
    }
}

size_t File::getActualFileSize() const {
    size_t size = 0;
    mmkv::getFileSize(m_fd, size);
    return size;
}

MemoryFile::MemoryFile(MMKVPath_t path, size_t expectedCapacity)
    : m_diskFile(std::move(path), OpenFlag::ReadWrite | OpenFlag::Create)
    , m_fileMapping(nullptr)
    , m_ptr(nullptr)
    , m_size(0) {
    reloadFromFile(expectedCapacity);
}

bool MemoryFile::truncate(size_t size) {
    if (!m_diskFile.isFileValid()) {
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

    if (m_ptr) {
        if (!UnmapViewOfFile(m_ptr)) {
            MMKVError("fail to munmap [%ls], %d", m_diskFile.m_path.c_str(), GetLastError());
        }
        m_ptr = nullptr;
    }
    if (m_fileMapping) {
        CloseHandle(m_fileMapping);
        m_fileMapping = nullptr;
    }

    if (!ftruncate(m_diskFile.getFd(), m_size)) {
        MMKVError("fail to truncate [%ls] to size %zu", m_diskFile.m_path.c_str(), m_size);
        m_size = oldSize;
        mmap();
        return false;
    }
    if (m_size > oldSize) {
        if (!zeroFillFile(m_diskFile.getFd(), oldSize, m_size - oldSize)) {
            MMKVError("fail to zeroFile [%ls] to size %zu", m_diskFile.m_path.c_str(), m_size);
            m_size = oldSize;
            mmap();
            return false;
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
        if (FlushViewOfFile(m_ptr, m_size)) {
            if (syncFlag == MMKV_SYNC) {
                if (!FlushFileBuffers(m_diskFile.getFd())) {
                    MMKVError("fail to FlushFileBuffers [%ls]:%d", m_diskFile.m_path.c_str(), GetLastError());
                    return false;
                }
            }
            return true;
        }
        MMKVError("fail to FlushViewOfFile [%ls]:%d", m_diskFile.m_path.c_str(), GetLastError());
        return false;
    }
    return false;
}

bool MemoryFile::mmap() {
    m_fileMapping = CreateFileMapping(m_diskFile.getFd(), nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (!m_fileMapping) {
        MMKVError("fail to CreateFileMapping [%ls], %d", m_diskFile.m_path.c_str(), GetLastError());
        return false;
    } else {
        m_ptr = (char *) MapViewOfFile(m_fileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (!m_ptr) {
            MMKVError("fail to mmap [%ls], %d", m_diskFile.m_path.c_str(), GetLastError());
            return false;
        }
    }

    return true;
}

void MemoryFile::reloadFromFile(size_t expectedCapacity) {
    if (isFileValid()) {
        MMKVWarning("calling reloadFromFile while the cache [%ls] is still valid", m_diskFile.m_path.c_str());
        assert(0);
        clearMemoryCache();
    }
    m_diskFile.open();
    if (m_diskFile.isFileValid()) {
        FileLock fileLock(m_diskFile.getFd());
        InterProcessLock lock(&fileLock, ExclusiveLockType);
        SCOPED_LOCK(&lock);

        mmkv::getFileSize(m_diskFile.getFd(), m_size);
        size_t expectedSize = std::max<size_t>(DEFAULT_MMAP_SIZE, expectedCapacity);
        // round up to (n * pagesize)
        expectedSize = (expectedSize + DEFAULT_MMAP_SIZE - 1) / DEFAULT_MMAP_SIZE * DEFAULT_MMAP_SIZE;
        // round up to (n * pagesize)
        if (m_size < expectedSize || (m_size % DEFAULT_MMAP_SIZE != 0)) {
            size_t roundSize = ((m_size / DEFAULT_MMAP_SIZE) + 1) * DEFAULT_MMAP_SIZE;;
            roundSize = std::max<size_t>(expectedSize, roundSize);
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
    m_diskFile.close();
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
                MMKVError("fail to create dir:%ls, %d", str.c_str(), GetLastError());
                free(path);
                return false;
            }
        } else if (!(attribute & FILE_ATTRIBUTE_DIRECTORY)) {
            MMKVError("%ls attribute:%d not a directry", str.c_str(), attribute);
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
                MMKVWarning("fail to read %ls: %d", nsFilePath.c_str(), GetLastError());
                delete buffer;
                buffer = nullptr;
            }
        }
        CloseHandle(fd);
    } else {
        MMKVWarning("fail to open %ls: %d", nsFilePath.c_str(), GetLastError());
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

static pair<MMKVPath_t, MMKVFileHandle_t> createUniqueTempFile(const wchar_t *prefix) {
    wchar_t lpTempPathBuffer[MAX_PATH];
    //  Gets the temp path env string (no guarantee it's a valid path).
    auto dwRetVal = GetTempPath(MAX_PATH, lpTempPathBuffer);
    if (dwRetVal > MAX_PATH || (dwRetVal == 0)) {
        MMKVError("GetTempPath failed %d", GetLastError());
        return {L"", INVALID_HANDLE_VALUE};
    }
    //  Generates a temporary file name.
    wchar_t szTempFileName[MAX_PATH];
    if (!GetTempFileName(lpTempPathBuffer, prefix, 0, szTempFileName)) {
        MMKVError("GetTempFileName failed %d", GetLastError());
        return {L"", INVALID_HANDLE_VALUE};
    }
    auto hTempFile =
        CreateFile(szTempFileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hTempFile == INVALID_HANDLE_VALUE) {
        MMKVError("fail to create unique temp file [%ls], %d", szTempFileName, GetLastError());
        return {L"", INVALID_HANDLE_VALUE};
    }
    MMKVDebug("create unique temp file [%ls] with fd[%p]", szTempFileName, hTempFile);
    return {MMKVPath_t(szTempFileName), hTempFile};
}

bool tryAtomicRename(const MMKVPath_t &srcPath, const MMKVPath_t &dstPath) {
    if (MoveFileEx(srcPath.c_str(), dstPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) == 0) {
        MMKVError("MoveFileEx [%ls] to [%ls] failed %d", srcPath.c_str(), dstPath.c_str(), GetLastError());
        return false;
    }
    return true;
}

bool copyFileContent(const MMKVPath_t &srcPath, MMKVFileHandle_t dstFD, bool needTruncate) {
    if (dstFD == INVALID_HANDLE_VALUE) {
        return false;
    }
    bool ret = false;
    File srcFile(srcPath, OpenFlag::ReadOnly);
    if (!srcFile.isFileValid()) {
        return false;
    }
    auto bufferSize = getPageSize();
    auto buffer = (char *) malloc(bufferSize);
    if (!buffer) {
        MMKVError("fail to malloc size %zu, %d(%s)", bufferSize, errno, strerror(errno));
        goto errorOut;
    }
    SetFilePointer(dstFD, 0, 0, FILE_BEGIN);

    // the Win32 platform don't have sendfile()/fcopyfile() equivalent, do it the hard way
    while (true) {
        DWORD sizeRead = 0;
        if (!ReadFile(srcFile.getFd(), buffer, bufferSize, &sizeRead, nullptr)) {
            MMKVError("fail to read %ls: %d", srcPath.c_str(), GetLastError());
            goto errorOut;
        }

        DWORD sizeWrite = 0;
        if (!WriteFile(dstFD, buffer, sizeRead, &sizeWrite, nullptr)) {
            MMKVError("fail to write fd [%d], %d", dstFD, GetLastError());
            goto errorOut;
        }

        if (sizeRead < bufferSize) {
            break;
        }
    }
    if (needTruncate) {
        size_t dstFileSize = 0;
        getFileSize(dstFD, dstFileSize);
        auto srcFileSize = srcFile.getActualFileSize();
        if ((dstFileSize != srcFileSize) && !ftruncate(dstFD, static_cast<off_t>(srcFileSize))) {
            MMKVError("fail to truncate [%d] to size [%zu]", dstFD, srcFileSize);
            goto errorOut;
        }
    }

    ret = true;
    MMKVInfo("copy content from %ls to fd[%d] finish", srcPath.c_str(), dstFD);

errorOut:
    free(buffer);
    return ret;
}

// copy to a temp file then rename it
// this is the best we can do on Win32
bool copyFile(const MMKVPath_t &srcPath, const MMKVPath_t &dstPath) {
    auto pair = createUniqueTempFile(L"MMKV");
    auto tmpFD = pair.second;
    auto &tmpPath = pair.first;
    if (tmpFD == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool renamed = false;
    if (copyFileContent(srcPath, tmpFD, false)) {
        MMKVInfo("copyed file [%ls] to [%ls]", srcPath.c_str(), tmpPath.c_str());
        CloseHandle(tmpFD);
        renamed = tryAtomicRename(tmpPath.c_str(), dstPath.c_str());
        if (renamed) {
            MMKVInfo("copyfile [%ls] to [%ls] finish.", srcPath.c_str(), dstPath.c_str());
        }
    } else {
        CloseHandle(tmpFD);
    }

    if (!renamed) {
        DeleteFile(tmpPath.c_str());
    }
    return renamed;
}

bool copyFileContent(const MMKVPath_t &srcPath, const MMKVPath_t &dstPath) {
    File dstFile(dstPath, OpenFlag::WriteOnly | OpenFlag::Create | OpenFlag::Truncate);
    if (!dstFile.isFileValid()) {
        return false;
    }
    auto ret = copyFileContent(srcPath, dstFile.getFd(), false);
    if (!ret) {
        MMKVError("fail to copyfile(): target file %ls", dstPath.c_str());
    } else {
        MMKVInfo("copy content from %ls to [%ls] finish", srcPath.c_str(), dstPath.c_str());
    }
    return ret;
}

bool copyFileContent(const MMKVPath_t &srcPath, MMKVFileHandle_t dstFD) {
    return copyFileContent(srcPath, dstFD, true);
}

void walkInDir(const MMKVPath_t &dirPath,
               WalkType type,
               const std::function<void(const MMKVPath_t &, WalkType)> &walker) {
    wchar_t szDir[MAX_PATH];
    StringCchCopy(szDir, MAX_PATH, dirPath.c_str());
    StringCchCat(szDir, MAX_PATH, L"\\*");

    WIN32_FIND_DATA ffd;
    auto hFind = FindFirstFile(szDir, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (type & WalkFolder) {
                if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) {
                    continue;
                }
                walker(dirPath + L"\\" + ffd.cFileName, WalkFolder);
            }
        } else if (type & WalkFile) {
            walker(dirPath + L"\\" + ffd.cFileName, WalkFile);
        }
    } while (FindNextFile(hFind, &ffd) != 0);

    auto dwError = GetLastError();
    if (dwError != ERROR_NO_MORE_FILES) {
        MMKVError("WalkInDir fail %d", dwError);
    }

    FindClose(hFind);
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

std::string MMKVPath_t2String(const MMKVPath_t &str) {
    auto length = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, nullptr, 0, 0, 0);
    auto buffer = new char[length];
    WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, buffer, length, 0, 0);
    string result(buffer);
    delete[] buffer;
    return result;
}

#endif // MMKV_WIN32
