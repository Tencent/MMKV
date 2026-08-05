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
#    include <algorithm>
#    include <atomic>
#    include <cassert>
#    include <climits>
#    include <cstddef>
#    include <limits>
#    include <strsafe.h>
#    include <winternl.h>
#    include <filesystem>

using namespace std;
namespace fs = std::filesystem;

namespace mmkv {

static bool getFileSize(MMKVFileHandle_t fd, size_t &size);
static bool getFileSize(const wchar_t *filename, size_t &size);
static bool ftruncate(MMKVFileHandle_t file, size_t size, DWORD *errorOut = nullptr, bool logFailure = true);

static bool getFileIdentity(MMKVFileHandle_t fd, array<uint64_t, 2> &identity) {
    if (fd == INVALID_HANDLE_VALUE) {
        return false;
    }
    BY_HANDLE_FILE_INFORMATION info = {};
    if (!GetFileInformationByHandle(fd, &info)) {
        return false;
    }
    identity[0] = info.dwVolumeSerialNumber;
    identity[1] = (static_cast<uint64_t>(info.nFileIndexHigh) << 32U) | info.nFileIndexLow;
    return true;
}

static MMKVFileHandle_t duplicateFileHandle(MMKVFileHandle_t fileHandle) {
    if (fileHandle == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
    }
    HANDLE duplicate = INVALID_HANDLE_VALUE;
    if (!DuplicateHandle(GetCurrentProcess(), fileHandle, GetCurrentProcess(), &duplicate, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
        return INVALID_HANDLE_VALUE;
    }
    return duplicate;
}

File::File(MMKVPath_t path, OpenFlag flag)
    : m_path(std::move(path)), m_utf8Path(MMKVPath_t2String(m_path)), m_fd(INVALID_HANDLE_VALUE), m_flag(flag) {
    open();
}

static pair<int, int> OpenFlag2NativeFlag(OpenFlag flag) {
    int access = 0, create = OPEN_EXISTING;
    if ((flag & OpenFlagRWMask) == OpenFlag::ReadWrite) {
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

bool File::open(bool existingOnly) {
    if (isFileValid()) {
        return true;
    }
    auto pair = OpenFlag2NativeFlag(m_flag);
    auto creationDisposition = existingOnly ? OPEN_EXISTING : pair.second;
    auto attributes = existingOnly ? FILE_FLAG_OPEN_REPARSE_POINT : FILE_ATTRIBUTE_NORMAL;
    m_fd = CreateFile(m_path.c_str(), pair.first, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                      creationDisposition, attributes, nullptr);
    if (!isFileValid()) {
        MMKVError("fail to open:[%s], flag %x, error %d", m_utf8Path.c_str(), m_flag, GetLastError());
        return false;
    }
    MMKVInfo("open fd[%p], flag %x, %s", m_fd, m_flag, m_utf8Path.c_str());
    return true;
}

void File::close() {
    if (isFileValid()) {
        MMKVInfo("closing fd[%p], %s", m_fd, m_utf8Path.c_str());
        if (CloseHandle(m_fd)) {
            m_fd = INVALID_HANDLE_VALUE;
        } else {
            MMKVError("fail to close [%s], %d", m_utf8Path.c_str(), GetLastError());
        }
    }
}

size_t File::getActualFileSize() const {
    size_t size = 0;
    if (isFileValid()) {
        mmkv::getFileSize(m_fd, size);
    } else {
        mmkv::getFileSize(m_path.c_str(), size);
    }
    return size;
}

MemoryFile::MemoryFile(MMKVPath_t path, size_t expectedCapacity, bool readOnly, bool mayflyFD)
    : m_diskFile(std::move(path), readOnly ? OpenFlag::ReadOnly : (OpenFlag::ReadWrite | OpenFlag::Create))
    , m_fileMapping(nullptr)
    , m_ptr(nullptr)
    , m_size(0)
    , m_readOnly(readOnly)
    , m_isMayflyFD(mayflyFD) {
    reloadFromFile(expectedCapacity);
}

bool MemoryFile::captureMappedFileIdentity() {
    array<uint64_t, 2> identity = {};
    if (!getFileIdentity(m_diskFile.getFd(), identity)) {
        clearMappedFileIdentity();
        return false;
    }
    m_mappedFileIdentity = identity;
    m_hasMappedFileIdentity = true;
    m_expectedFileIdentity = {};
    m_hasExpectedFileIdentity = false;
    return true;
}

void MemoryFile::clearMappedFileIdentity() {
    m_mappedFileIdentity = {};
    m_hasMappedFileIdentity = false;
}

bool MemoryFile::isMappedFile(MMKVFileHandle_t fileHandle) const {
    array<uint64_t, 2> identity = {};
    return m_hasMappedFileIdentity && getFileIdentity(fileHandle, identity) && identity == m_mappedFileIdentity;
}

bool MemoryFile::truncate(size_t size, FileLock *fileLock) {
    if (m_isMayflyFD && !openIfNeeded()) {
        return false;
    }
    if (!m_diskFile.isFileValid()) {
        return false;
    }
    if (size == m_size) {
        return true;
    }
    if (m_readOnly) {
        // truncate readonly file not allow
        return false;
    }

    auto oldSize = m_size;
    m_size = size;
    // round up to (n * pagesize)
    if (m_size < DEFAULT_MMAP_SIZE || (m_size % DEFAULT_MMAP_SIZE != 0)) {
        m_size = ((m_size / DEFAULT_MMAP_SIZE) + 1) * DEFAULT_MMAP_SIZE;
    }

    // Win32 won't ftruncate a file if there's active file mmapping/handle, we have to unmmap/close ahead
    bool needMMapOnFailure = false;
    if (m_ptr) {
        // if we have a valid file mapping before, we should restore it regardless
        needMMapOnFailure = true;
        if (!UnmapViewOfFile(m_ptr)) {
            MMKVError("fail to munmap [%s], %d", m_diskFile.getUTF8Path().c_str(), GetLastError());
        }
        m_ptr = nullptr;
    }
    if (m_fileMapping) {
        CloseHandle(m_fileMapping);
        m_fileMapping = nullptr;
    }

    if (!ftruncate(m_diskFile.getFd(), m_size)) {
        MMKVError("fail to truncate [%s] to size %zu", m_diskFile.getUTF8Path().c_str(), m_size);
        m_size = oldSize;
        if (needMMapOnFailure) {
            mmapOrCleanup(fileLock);
        }
        return false;
    }
    if (m_size > oldSize) {
        if (!zeroFillFile(m_diskFile.getFd(), oldSize, m_size - oldSize)) {
            MMKVError("fail to zeroFile [%s] to size %zu", m_diskFile.getUTF8Path().c_str(), m_size);
            m_size = oldSize;
            if (needMMapOnFailure) {
                mmapOrCleanup(fileLock);
            }
            return false;
        }
    }

    return mmapOrCleanup(fileLock);
}

bool MemoryFile::msync(SyncFlag syncFlag) {
    if (m_readOnly) {
        // there's no point in msync() readonly memory
        return true;
    }
    if (m_ptr) {
        if (FlushViewOfFile(m_ptr, m_size)) {
            if (syncFlag == MMKV_SYNC) {
                if (!openIfNeeded()) {
                    return false;
                }
                auto ret = FlushFileBuffers(m_diskFile.getFd());
                if (!ret) {
                    MMKVError("fail to FlushFileBuffers [%s]:%d", m_diskFile.getUTF8Path().c_str(), GetLastError());
                }
                cleanMayflyFD();
                return ret;
            }
            return true;
        }
        MMKVError("fail to FlushViewOfFile [%s]:%d", m_diskFile.getUTF8Path().c_str(), GetLastError());
        return false;
    }
    return false;
}

bool MemoryFile::mmapOrCleanup(FileLock *fileLock) {
    auto mode = m_readOnly ? PAGE_READONLY : PAGE_READWRITE;
    m_fileMapping = CreateFileMapping(m_diskFile.getFd(), nullptr, mode, 0, 0, nullptr);
    if (!m_fileMapping) {
        MMKVError("fail to CreateFileMapping [%s], mode %x, %d", m_diskFile.getUTF8Path().c_str(), mode,
                  GetLastError());
        return false;
    } else {
        auto viewMode = m_readOnly ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS;
        m_ptr = (char*)MapViewOfFile(m_fileMapping, viewMode, 0, 0, 0);
        if (!m_ptr) {
            MMKVError("fail to mmap [%s], mode %x, %d", m_diskFile.getUTF8Path().c_str(), viewMode, GetLastError());

            doCleanMemoryCache(false);
            return false;
        }
        if (!captureMappedFileIdentity()) {
            MMKVError("fail to capture mapped file identity [%s], %d", m_diskFile.getUTF8Path().c_str(),
                      GetLastError());
            doCleanMemoryCache(false);
            return false;
        }
        MMKVInfo("mmap to address [%p], [%s]", m_ptr, m_diskFile.getUTF8Path().c_str());

        if (m_isMayflyFD && fileLock) {
            fileLock->destroyAndUnLock();
        }

        cleanMayflyFD();
        return true;
    }
}

void MemoryFile::reloadFromFile(size_t expectedCapacity) {
    if (isFileValid()) {
        MMKVWarning("calling reloadFromFile while the cache [%s] is still valid", m_diskFile.getUTF8Path().c_str());
        assert(0);
        clearMemoryCache();
    }
    if (openIfNeeded()) {
        FileLock fileLock(m_diskFile.getFd());
        InterProcessLock lock(&fileLock, SharedLockType);
        SCOPED_LOCK(&lock);

        mmkv::getFileSize(m_diskFile.getFd(), m_size);
        size_t expectedSize = std::max<size_t>(DEFAULT_MMAP_SIZE, roundUp<size_t>(expectedCapacity, DEFAULT_MMAP_SIZE));
        // round up to (n * pagesize)
        if (!m_readOnly && (m_size < expectedSize || (m_size % DEFAULT_MMAP_SIZE != 0))) {
            InterProcessLock exclusiveLock(&fileLock, ExclusiveLockType);
            SCOPED_LOCK(&exclusiveLock);

            size_t roundSize = ((m_size / DEFAULT_MMAP_SIZE) + 1) * DEFAULT_MMAP_SIZE;;
            roundSize = std::max<size_t>(expectedSize, roundSize);
            truncate(roundSize, &fileLock);
        } else {
            mmapOrCleanup(&fileLock);
        }
    }
}

bool MemoryFile::reloadFromFileHandle(MMKVFileHandle_t fileHandle, size_t expectedCapacity) {
    array<uint64_t, 2> intendedIdentity = {};
    if (!getFileIdentity(fileHandle, intendedIdentity)) {
        return false;
    }
    auto mappingHandle = duplicateFileHandle(fileHandle);
    auto retainedHandle = duplicateFileHandle(fileHandle);
    if (mappingHandle == INVALID_HANDLE_VALUE || retainedHandle == INVALID_HANDLE_VALUE) {
        closeFileHandle(mappingHandle);
        closeFileHandle(retainedHandle);
        if (!m_hasMappedFileIdentity) {
            m_expectedFileIdentity = intendedIdentity;
            m_hasExpectedFileIdentity = true;
        }
        return false;
    }

    doCleanMemoryCache(false);
    clearMappedFileIdentity();
    m_expectedFileIdentity = {};
    m_hasExpectedFileIdentity = false;
    m_diskFile.m_fd = mappingHandle;
    reloadFromFile(expectedCapacity);
    if (m_ptr && m_hasMappedFileIdentity && m_mappedFileIdentity == intendedIdentity) {
        if (m_isMayflyFD) {
            // Keep the exact file handle available through the caller's
            // multiprocess size check; cleanMayflyFD() releases it afterward.
            // The caller may have passed this MemoryFile's own handle, which
            // doCleanMemoryCache() closed, so use the pre-cleanup duplicate.
            if (!m_diskFile.isFileValid()) {
                m_diskFile.m_fd = retainedHandle;
                retainedHandle = MMKVFileHandleInvalidValue;
            }
            if (!m_diskFile.isFileValid()) {
                doCleanMemoryCache(false);
                m_expectedFileIdentity = intendedIdentity;
                m_hasExpectedFileIdentity = true;
                closeFileHandle(retainedHandle);
                return false;
            }
            m_keepFileHandleForReload = true;
        }
        closeFileHandle(retainedHandle);
        return true;
    }

    // Keep the intended file pinned across a failed remap. A later retry will
    // use this handle instead of resolving a potentially replaced m_path.
    doCleanMemoryCache(false);
    clearMappedFileIdentity();
    m_diskFile.m_fd = retainedHandle;
    m_keepFileHandleForReload = m_diskFile.isFileValid();
    m_expectedFileIdentity = intendedIdentity;
    m_hasExpectedFileIdentity = true;
    return false;
}

void MemoryFile::doCleanMemoryCache(bool forceClean) {
    if (m_ptr) {
        UnmapViewOfFile(m_ptr);
        m_ptr = nullptr;
    }
    if (forceClean) {
        clearMappedFileIdentity();
    }
    if (m_fileMapping) {
        CloseHandle(m_fileMapping);
        m_fileMapping = nullptr;
    }
    if (forceClean) {
        m_expectedFileIdentity = {};
        m_hasExpectedFileIdentity = false;
    }
    m_keepFileHandleForReload = false;
    m_diskFile.close();
}

bool MemoryFile::openIfNeeded() {
    auto requireExistingIdentity = m_hasMappedFileIdentity || m_hasExpectedFileIdentity;
    if (!m_diskFile.isFileValid() && !m_diskFile.open(requireExistingIdentity)) {
        return false;
    }
    if (m_hasMappedFileIdentity && !isMappedFile(m_diskFile.getFd())) {
        MMKVError("refuse to reopen mapped file through replaced path [%s]", m_diskFile.getUTF8Path().c_str());
        m_diskFile.close();
        return false;
    }
    if (!m_ptr && m_hasExpectedFileIdentity) {
        array<uint64_t, 2> identity = {};
        if (!getFileIdentity(m_diskFile.getFd(), identity) || identity != m_expectedFileIdentity) {
            MMKVError("refuse to reload file through replaced path [%s]", m_diskFile.getUTF8Path().c_str());
            m_diskFile.close();
            return false;
        }
    }
    return true;
}

void MemoryFile::cleanMayflyFD(bool forceClean) {
    if (forceClean) {
        m_keepFileHandleForReload = false;
    }
    if (m_isMayflyFD) {
        if (!m_keepFileHandleForReload && m_diskFile.isFileValid()) {
            m_diskFile.close();
        }
        if (!m_keepFileHandleForReload && m_fileMapping) {
            CloseHandle(m_fileMapping);
            m_fileMapping = nullptr;
        }
    }
}

size_t MemoryFile::getActualFileSize() {
    if (!m_isMayflyFD && !m_diskFile.isFileValid()) {
        return 0;
    }
    auto openedHere = !m_diskFile.isFileValid();
    if (!openIfNeeded()) {
        return (m_ptr && m_hasMappedFileIdentity) ? m_size : 0;
    }
    auto size = m_diskFile.getActualFileSize();
    if (openedHere) {
        cleanMayflyFD();
    }
    return size;
}

MMKVFileHandle_t MemoryFile::getFd() {
    if (m_isMayflyFD) {
        openIfNeeded();
    }
    return m_diskFile.getFd();
}

size_t getPageSize() {
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return system_info.dwPageSize;
}

MMKVPath_t absolutePath(const MMKVPath_t& path) {
    fs::path relative_path(path);
    fs::path absolute_path = fs::absolute(relative_path);
    try {
        fs::path normalized = fs::weakly_canonical(absolute_path);
        return normalized.wstring();
    } catch (std::exception &e) {
        const auto &utf8Path = MMKVPath_t2String(absolute_path.wstring());
        MMKVError("fail to weakly_canonical() path %s, error: %s", utf8Path.c_str(), e.what());
    }
    return absolute_path.wstring();
}

bool isFileExist(const MMKVPath_t &nsFilePath) {
    if (nsFilePath.empty()) {
        return false;
    }
    auto attribute = GetFileAttributes(nsFilePath.c_str());
    return (attribute != INVALID_FILE_ATTRIBUTES);
}

bool mkPath(const MMKVPath_t &str) {
    if (str.empty() || str.find(L'\0') != MMKVPath_t::npos) {
        SetLastError(ERROR_INVALID_NAME);
        return false;
    }
    wchar_t *path = _wcsdup(str.c_str());
    if (!path) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return false;
    }

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
                const auto &utf8Path = MMKVPath_t2String(str);
                MMKVError("fail to create dir:%s, %d", utf8Path.c_str(), GetLastError());
                free(path);
                return false;
            }
        } else if (!(attribute & FILE_ATTRIBUTE_DIRECTORY)) {
            const auto &utf8Path = MMKVPath_t2String(str);
            MMKVError("%s attribute:%d not a directory", utf8Path.c_str(), attribute);
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
            if (ReadFile(fd, buffer->getPtr(), (DWORD) fileLength, &readSize, nullptr)) {
                //fileSize = readSize;
            } else {
                const auto &utf8Path = MMKVPath_t2String(nsFilePath);
                MMKVWarning("fail to read %s: %d", utf8Path.c_str(), GetLastError());
                delete buffer;
                buffer = nullptr;
            }
        }
        CloseHandle(fd);
    } else {
        const auto &utf8Path = MMKVPath_t2String(nsFilePath);
        MMKVWarning("fail to open %s: %d", utf8Path.c_str(), GetLastError());
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

    LARGE_INTEGER position = {};
    position.QuadPart = startPos;
    if (!SetFilePointerEx(file, position, nullptr, FILE_BEGIN)) {
        MMKVError("fail to lseek fd[%p], error:%d", file, GetLastError());
        return false;
    }

    static const char zeros[4096] = {};
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
        if (!WriteFile(file, zeros, (DWORD) size, &bytesWritten, nullptr)) {
            MMKVError("fail to write fd[%p], error:%d", file, GetLastError());
            return false;
        }
    }
    return true;
}

static bool ftruncate(MMKVFileHandle_t file, size_t size, DWORD *errorOut, bool logFailure) {
    LARGE_INTEGER large = {};
    large.QuadPart = size;
    if (SetFilePointerEx(file, large, 0, FILE_BEGIN)) {
        if (SetEndOfFile(file)) {
            if (errorOut) {
                *errorOut = ERROR_SUCCESS;
            }
            return true;
        }
        const auto error = GetLastError();
        if (errorOut) {
            *errorOut = error;
        }
        if (logFailure) {
            MMKVError("fail to SetEndOfFile:%d", error);
        }
        SetLastError(error);
        return false;
    } else {
        const auto error = GetLastError();
        if (errorOut) {
            *errorOut = error;
        }
        if (logFailure) {
            MMKVError("fail to SetFilePointer:%d", error);
        }
        SetLastError(error);
        return false;
    }
}

static bool getFileSize(MMKVFileHandle_t fd, size_t &size) {
    LARGE_INTEGER filesize = {};
    if (GetFileSizeEx(fd, &filesize)) {
        size = static_cast<size_t>(filesize.QuadPart);
        return true;
    }
    return false;
}

bool getFileSize(const wchar_t *filename, size_t &size) {
    WIN32_FILE_ATTRIBUTE_DATA fileAttr = {};
    if (GetFileAttributesEx(filename, GetFileExInfoStandard, &fileAttr)) {
        size = ((ULONGLONG)fileAttr.nFileSizeHigh << 32) | fileAttr.nFileSizeLow;
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
    const auto &utf8Path = MMKVPath_t2String(szTempFileName);
    if (hTempFile == INVALID_HANDLE_VALUE) {
        MMKVError("fail to create unique temp file [%s], %d", utf8Path.c_str(), GetLastError());
        return {L"", INVALID_HANDLE_VALUE};
    }
    MMKVDebug("create unique temp file [%s] with fd[%p]", utf8Path.c_str(), hTempFile);
    return {MMKVPath_t(szTempFileName), hTempFile};
}

void closeFileHandle(MMKVFileHandle_t handle) {
    if (handle != MMKVFileHandleInvalidValue) {
        CloseHandle(handle);
    }
}

bool syncFile(MMKVFileHandle_t handle) {
    return handle != MMKVFileHandleInvalidValue && FlushFileBuffers(handle) != 0;
}

static bool isDirectChildName(const MMKVPath_t &fileName) {
    return !fileName.empty() && fileName != L"." && fileName != L".." &&
           fileName.find_first_of(L"\\/:") == MMKVPath_t::npos && fileName.find(L'\0') == MMKVPath_t::npos;
}

static bool getHandleAttributes(MMKVFileHandle_t handle, DWORD &attributes) {
    BY_HANDLE_FILE_INFORMATION info = {};
    if (!GetFileInformationByHandle(handle, &info)) {
        return false;
    }
    attributes = info.dwFileAttributes;
    return true;
}

static MMKVFileHandle_t openDirectoryRoot(const MMKVPath_t &dirPath) {
    if (dirPath.empty() || dirPath.find(L'\0') != MMKVPath_t::npos) {
        SetLastError(ERROR_INVALID_NAME);
        return MMKVFileHandleInvalidValue;
    }
    auto handle = CreateFile(dirPath.c_str(), FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                             FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    DWORD attributes = 0;
    if (handle == INVALID_HANDLE_VALUE || !getHandleAttributes(handle, attributes) ||
        !(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE))) {
        closeFileHandle(handle);
        return MMKVFileHandleInvalidValue;
    }
    return handle;
}

static MMKVPath_t childPath(const MMKVPath_t &dirPath, const MMKVPath_t &fileName) {
    auto fullPath = dirPath;
    if (!fullPath.empty() && fullPath.back() != L'\\' && fullPath.back() != L'/') {
        fullPath.push_back(L'\\');
    }
    fullPath += fileName;
    return fullPath;
}

enum class ChildOpenMode { ReadOnly, ReadWriteExisting, ReadWriteCreate, ReadWriteExclusive };

using NtCreateFileFunction = NTSTATUS(NTAPI *)(PHANDLE,
                                               ACCESS_MASK,
                                               POBJECT_ATTRIBUTES,
                                               PIO_STATUS_BLOCK,
                                               PLARGE_INTEGER,
                                               ULONG,
                                               ULONG,
                                               ULONG,
                                               ULONG,
                                               PVOID,
                                               ULONG);
using NtSetInformationFileFunction = NTSTATUS(NTAPI *)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, ULONG);
using NtQueryDirectoryFileFunction = NTSTATUS(NTAPI *)(HANDLE,
                                                       HANDLE,
                                                       PVOID,
                                                       PVOID,
                                                       PIO_STATUS_BLOCK,
                                                       PVOID,
                                                       ULONG,
                                                       ULONG,
                                                       BOOLEAN,
                                                       PUNICODE_STRING,
                                                       BOOLEAN);

static NtCreateFileFunction getNtCreateFile() {
    static auto function = []() {
        auto module = GetModuleHandleW(L"ntdll.dll");
        return module ? reinterpret_cast<NtCreateFileFunction>(GetProcAddress(module, "NtCreateFile")) : nullptr;
    }();
    return function;
}

static NtSetInformationFileFunction getNtSetInformationFile() {
    static auto function = []() {
        auto module = GetModuleHandleW(L"ntdll.dll");
        return module ? reinterpret_cast<NtSetInformationFileFunction>(
                            GetProcAddress(module, "NtSetInformationFile"))
                      : nullptr;
    }();
    return function;
}

static NtQueryDirectoryFileFunction getNtQueryDirectoryFile() {
    static auto function = []() {
        auto module = GetModuleHandleW(L"ntdll.dll");
        return module ? reinterpret_cast<NtQueryDirectoryFileFunction>(
                            GetProcAddress(module, "NtQueryDirectoryFile"))
                      : nullptr;
    }();
    return function;
}

static constexpr ULONG MMKV_OBJ_DONT_REPARSE = 0x00001000L;
static constexpr ULONG MMKV_FILE_OPEN = 0x00000001L;
static constexpr ULONG MMKV_FILE_CREATE = 0x00000002L;
static constexpr ULONG MMKV_FILE_OPEN_IF = 0x00000003L;
static constexpr ULONG MMKV_FILE_DIRECTORY_FILE = 0x00000001L;
static constexpr ULONG MMKV_FILE_SEQUENTIAL_ONLY = 0x00000004L;
static constexpr ULONG MMKV_FILE_SYNCHRONOUS_IO_NONALERT = 0x00000020L;
static constexpr ULONG MMKV_FILE_NON_DIRECTORY_FILE = 0x00000040L;
static constexpr ULONG MMKV_FILE_OPEN_REPARSE_POINT = 0x00200000L;
static constexpr ULONG MMKV_FILE_RENAME_INFORMATION = 10;
static constexpr ULONG MMKV_FILE_DISPOSITION_INFORMATION = 13;
static constexpr ULONG MMKV_FILE_BOTH_DIRECTORY_INFORMATION = 3;
static constexpr NTSTATUS MMKV_STATUS_INVALID_PARAMETER = static_cast<NTSTATUS>(0xC000000DL);
static constexpr NTSTATUS MMKV_STATUS_NO_SUCH_FILE = static_cast<NTSTATUS>(0xC000000FL);
static constexpr NTSTATUS MMKV_STATUS_OBJECT_NAME_NOT_FOUND = static_cast<NTSTATUS>(0xC0000034L);
static constexpr NTSTATUS MMKV_STATUS_NO_MORE_FILES = static_cast<NTSTATUS>(0x80000006L);

static NTSTATUS ntCreateFileWithNoReparseFallback(NtCreateFileFunction ntCreateFile,
                                                  PHANDLE fileHandle,
                                                  ACCESS_MASK desiredAccess,
                                                  POBJECT_ATTRIBUTES objectAttributes,
                                                  PIO_STATUS_BLOCK ioStatus,
                                                  PLARGE_INTEGER allocationSize,
                                                  ULONG fileAttributes,
                                                  ULONG shareAccess,
                                                  ULONG createDisposition,
                                                  ULONG createOptions,
                                                  PVOID eaBuffer,
                                                  ULONG eaLength) {
    auto status = ntCreateFile(fileHandle, desiredAccess, objectAttributes, ioStatus, allocationSize,
                               fileAttributes, shareAccess, createDisposition, createOptions, eaBuffer, eaLength);
    if (status == MMKV_STATUS_INVALID_PARAMETER && objectAttributes &&
        (objectAttributes->Attributes & MMKV_OBJ_DONT_REPARSE)) {
        // OBJ_DONT_REPARSE is unavailable on older kernels. The caller still
        // opens the final component itself and must reject a reparse-point
        // handle after the retry.
        const auto attributes = objectAttributes->Attributes;
        objectAttributes->Attributes &= ~MMKV_OBJ_DONT_REPARSE;
        status = ntCreateFile(fileHandle, desiredAccess, objectAttributes, ioStatus, allocationSize,
                              fileAttributes, shareAccess, createDisposition, createOptions, eaBuffer, eaLength);
        objectAttributes->Attributes = attributes;
    }
    return status;
}

struct MMKVFileBothDirectoryInformation {
    ULONG NextEntryOffset;
    ULONG FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG FileAttributes;
    ULONG FileNameLength;
    ULONG EaSize;
    CCHAR ShortNameLength;
    WCHAR ShortName[12];
    WCHAR FileName[1];
};

struct MMKVFileRenameInformation {
    BOOLEAN ReplaceIfExists;
    HANDLE RootDirectory;
    ULONG FileNameLength;
    WCHAR FileName[1];
};

struct MMKVFileDispositionInformation {
    BOOLEAN DeleteFile;
};

static bool ntRenameOpenedChild(MMKVFileHandle_t fileHandle,
                                MMKVFileHandle_t dstDirFD,
                                const MMKVPath_t &dstFileName,
                                bool replaceIfExists) {
    constexpr size_t fixedSize = sizeof(MMKVFileRenameInformation) - sizeof(WCHAR);
    auto ntSetInformationFile = getNtSetInformationFile();
    if (!ntSetInformationFile || fileHandle == INVALID_HANDLE_VALUE || dstDirFD == INVALID_HANDLE_VALUE ||
        !isDirectChildName(dstFileName) || fixedSize > ULONG_MAX ||
        dstFileName.size() > (ULONG_MAX / sizeof(wchar_t))) {
        return false;
    }
    auto nameBytes = static_cast<ULONG>(dstFileName.size() * sizeof(wchar_t));
    if (nameBytes > ULONG_MAX - fixedSize) {
        return false;
    }
    auto bufferSize = static_cast<ULONG>(fixedSize + nameBytes);
    vector<uint8_t> buffer(bufferSize);
    auto renameInfo = reinterpret_cast<MMKVFileRenameInformation *>(buffer.data());
    renameInfo->ReplaceIfExists = replaceIfExists ? TRUE : FALSE;
    renameInfo->RootDirectory = dstDirFD;
    renameInfo->FileNameLength = nameBytes;
    memcpy(renameInfo->FileName, dstFileName.data(), nameBytes);
    IO_STATUS_BLOCK ioStatus = {};
    auto status = ntSetInformationFile(fileHandle, &ioStatus, renameInfo, static_cast<ULONG>(bufferSize),
                                       MMKV_FILE_RENAME_INFORMATION);
    return status >= 0;
}

static bool ntDeleteOpenedChild(MMKVFileHandle_t fileHandle) {
    auto ntSetInformationFile = getNtSetInformationFile();
    if (!ntSetInformationFile || fileHandle == INVALID_HANDLE_VALUE) {
        return false;
    }
    MMKVFileDispositionInformation disposition = {TRUE};
    IO_STATUS_BLOCK ioStatus = {};
    auto status = ntSetInformationFile(fileHandle, &ioStatus, &disposition, sizeof(disposition),
                                       MMKV_FILE_DISPOSITION_INFORMATION);
    return status >= 0;
}

static bool ntCreateRegularChild(MMKVFileHandle_t dirFD,
                                 const MMKVPath_t &fileName,
                                 ChildOpenMode mode,
                                 MMKVFileHandle_t &fileHandle) {
    fileHandle = MMKVFileHandleInvalidValue;
    auto ntCreateFile = getNtCreateFile();
    if (!ntCreateFile || dirFD == INVALID_HANDLE_VALUE || !isDirectChildName(fileName) ||
        fileName.size() > USHRT_MAX / sizeof(wchar_t)) {
        return false;
    }

    UNICODE_STRING objectName = {};
    objectName.Buffer = const_cast<PWSTR>(fileName.c_str());
    objectName.Length = static_cast<USHORT>(fileName.size() * sizeof(wchar_t));
    objectName.MaximumLength = objectName.Length;
    OBJECT_ATTRIBUTES objectAttributes = {};
    objectAttributes.Length = sizeof(objectAttributes);
    objectAttributes.RootDirectory = dirFD;
    objectAttributes.ObjectName = &objectName;
    objectAttributes.Attributes = OBJ_CASE_INSENSITIVE | MMKV_OBJ_DONT_REPARSE;

    auto desiredAccess = mode == ChildOpenMode::ReadOnly ? GENERIC_READ | SYNCHRONIZE
                                                         : GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE;
    if (mode == ChildOpenMode::ReadWriteExclusive) {
        desiredAccess |= DELETE;
    }
    auto disposition = MMKV_FILE_OPEN;
    if (mode == ChildOpenMode::ReadWriteCreate) {
        disposition = MMKV_FILE_OPEN_IF;
    } else if (mode == ChildOpenMode::ReadWriteExclusive) {
        disposition = MMKV_FILE_CREATE;
    }
    auto options = MMKV_FILE_NON_DIRECTORY_FILE | MMKV_FILE_OPEN_REPARSE_POINT |
                   MMKV_FILE_SYNCHRONOUS_IO_NONALERT |
                   (mode == ChildOpenMode::ReadOnly ? MMKV_FILE_SEQUENTIAL_ONLY : 0);
    IO_STATUS_BLOCK ioStatus = {};
    auto status = ntCreateFileWithNoReparseFallback(
        ntCreateFile, &fileHandle, desiredAccess, &objectAttributes, &ioStatus, nullptr, FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, disposition, options, nullptr, 0);
    if (status < 0) {
        closeFileHandle(fileHandle);
        fileHandle = MMKVFileHandleInvalidValue;
        return false;
    }
    return true;
}

static bool isPathSeparator(wchar_t value) {
    return value == L'\\' || value == L'/';
}

static bool containsParentPathComponent(const MMKVPath_t &path) {
    size_t position = 0;
    while (position < path.size()) {
        while (position < path.size() && isPathSeparator(path[position])) {
            position++;
        }
        auto start = position;
        while (position < path.size() && !isPathSeparator(path[position])) {
            position++;
        }
        if (position - start == 2 && path[start] == L'.' && path[start + 1] == L'.') {
            return true;
        }
    }
    return false;
}

static bool getAbsoluteLexicalPath(const MMKVPath_t &path, MMKVPath_t &absolutePath) {
    if (path.empty() || path.find(L'\0') != MMKVPath_t::npos || containsParentPathComponent(path)) {
        SetLastError(ERROR_INVALID_NAME);
        return false;
    }
    auto required = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (required == 0) {
        return false;
    }
    vector<wchar_t> buffer(static_cast<size_t>(required) + 1);
    auto written = GetFullPathNameW(path.c_str(), static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
    if (written == 0 || written >= buffer.size()) {
        return false;
    }
    absolutePath.assign(buffer.data(), written);
    replace(absolutePath.begin(), absolutePath.end(), L'/', L'\\');
    return true;
}

static bool consumePathComponent(const MMKVPath_t &path, size_t &position, MMKVPath_t &component) {
    while (position < path.size() && isPathSeparator(path[position])) {
        position++;
    }
    auto start = position;
    while (position < path.size() && !isPathSeparator(path[position])) {
        position++;
    }
    if (position == start) {
        return false;
    }
    component = path.substr(start, position - start);
    return isDirectChildName(component);
}

static bool isCanonicalVolumeGuid(const MMKVPath_t &path, size_t start) {
    static constexpr size_t GuidLength = 36;
    if (start > path.size() || path.size() - start < GuidLength) {
        return false;
    }
    for (size_t index = 0; index < GuidLength; index++) {
        auto value = path[start + index];
        if (index == 8 || index == 13 || index == 18 || index == 23) {
            if (value != L'-') {
                return false;
            }
        } else if (!((value >= L'0' && value <= L'9') || (value >= L'a' && value <= L'f') ||
                     (value >= L'A' && value <= L'F'))) {
            return false;
        }
    }
    return true;
}

static bool splitAbsoluteDirectoryRoot(const MMKVPath_t &path, MMKVPath_t &rootPath, size_t &position) {
    position = 0;
    const MMKVPath_t extendedUNCPrefix = L"\\\\?\\UNC\\";
    if (path.size() >= extendedUNCPrefix.size() &&
        _wcsnicmp(path.c_str(), extendedUNCPrefix.c_str(), extendedUNCPrefix.size()) == 0) {
        position = extendedUNCPrefix.size();
        MMKVPath_t server;
        MMKVPath_t share;
        if (!consumePathComponent(path, position, server) || !consumePathComponent(path, position, share)) {
            return false;
        }
        rootPath = extendedUNCPrefix + server + L"\\" + share + L"\\";
        if (position < path.size() && isPathSeparator(path[position])) {
            position++;
        }
        return true;
    }

    if (path.size() >= 7 && path.compare(0, 4, L"\\\\?\\") == 0 && path[5] == L':' &&
        isPathSeparator(path[6])) {
        auto drive = path[4];
        if (!((drive >= L'A' && drive <= L'Z') || (drive >= L'a' && drive <= L'z'))) {
            return false;
        }
        rootPath = path.substr(0, 7);
        position = 7;
        return true;
    }

    const MMKVPath_t extendedVolumePrefix = L"\\\\?\\Volume{";
    if (path.size() > extendedVolumePrefix.size() &&
        _wcsnicmp(path.c_str(), extendedVolumePrefix.c_str(), extendedVolumePrefix.size()) == 0) {
        auto closingBrace = extendedVolumePrefix.size() + 36;
        if (!isCanonicalVolumeGuid(path, extendedVolumePrefix.size()) || closingBrace >= path.size() ||
            path[closingBrace] != L'}' || closingBrace + 1 >= path.size() ||
            !isPathSeparator(path[closingBrace + 1])) {
            return false;
        }
        rootPath = path.substr(0, closingBrace + 2);
        position = closingBrace + 2;
        return true;
    }

    // Do not reinterpret unsupported extended/device namespaces such as
    // \\?\GLOBALROOT or \\.\GLOBALROOT as ordinary UNC paths. Only the explicitly handled
    // extended drive, UNC, and Volume-GUID forms are filesystem roots here.
    if (path.size() >= 4 &&
        (path.compare(0, 4, L"\\\\?\\") == 0 || path.compare(0, 4, L"\\\\.\\") == 0)) {
        return false;
    }

    if (path.size() >= 3 && path[1] == L':' && isPathSeparator(path[2])) {
        auto drive = path[0];
        if (!((drive >= L'A' && drive <= L'Z') || (drive >= L'a' && drive <= L'z'))) {
            return false;
        }
        rootPath = path.substr(0, 3);
        position = 3;
        return true;
    }

    if (path.size() >= 2 && isPathSeparator(path[0]) && isPathSeparator(path[1])) {
        position = 2;
        MMKVPath_t server;
        MMKVPath_t share;
        if (!consumePathComponent(path, position, server) || !consumePathComponent(path, position, share)) {
            return false;
        }
        rootPath = L"\\\\" + server + L"\\" + share + L"\\";
        if (position < path.size() && isPathSeparator(path[position])) {
            position++;
        }
        return true;
    }
    return false;
}

static MMKVFileHandle_t ntOpenDirectoryChild(MMKVFileHandle_t dirFD,
                                             const MMKVPath_t &fileName,
                                             bool create) {
    auto ntCreateFile = getNtCreateFile();
    if (!ntCreateFile || dirFD == INVALID_HANDLE_VALUE || !isDirectChildName(fileName) ||
        fileName.size() > USHRT_MAX / sizeof(wchar_t)) {
        return MMKVFileHandleInvalidValue;
    }

    UNICODE_STRING objectName = {};
    objectName.Buffer = const_cast<PWSTR>(fileName.c_str());
    objectName.Length = static_cast<USHORT>(fileName.size() * sizeof(wchar_t));
    objectName.MaximumLength = objectName.Length;
    OBJECT_ATTRIBUTES objectAttributes = {};
    objectAttributes.Length = sizeof(objectAttributes);
    objectAttributes.RootDirectory = dirFD;
    objectAttributes.ObjectName = &objectName;
    objectAttributes.Attributes = OBJ_CASE_INSENSITIVE | MMKV_OBJ_DONT_REPARSE;

    MMKVFileHandle_t childFD = MMKVFileHandleInvalidValue;
    IO_STATUS_BLOCK ioStatus = {};
    auto status = ntCreateFileWithNoReparseFallback(
        ntCreateFile, &childFD, FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | SYNCHRONIZE, &objectAttributes,
        &ioStatus, nullptr, FILE_ATTRIBUTE_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        create ? MMKV_FILE_OPEN_IF : MMKV_FILE_OPEN,
        MMKV_FILE_DIRECTORY_FILE | MMKV_FILE_OPEN_REPARSE_POINT | MMKV_FILE_SYNCHRONOUS_IO_NONALERT, nullptr, 0);
    if (status < 0) {
        closeFileHandle(childFD);
        return MMKVFileHandleInvalidValue;
    }
    DWORD attributes = 0;
    if (!getHandleAttributes(childFD, attributes) || !(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE))) {
        closeFileHandle(childFD);
        return MMKVFileHandleInvalidValue;
    }
    return childFD;
}

static MMKVFileHandle_t openDirectoryPathPinned(const MMKVPath_t &dirPath, bool create) {
    MMKVPath_t absoluteDirPath;
    if (!getAbsoluteLexicalPath(dirPath, absoluteDirPath)) {
        return MMKVFileHandleInvalidValue;
    }
    MMKVPath_t rootPath;
    size_t position = 0;
    if (!splitAbsoluteDirectoryRoot(absoluteDirPath, rootPath, position)) {
        SetLastError(ERROR_INVALID_NAME);
        return MMKVFileHandleInvalidValue;
    }

    // Validate the complete relative portion before creating anything. A
    // single-pass create walk could otherwise leave valid prefixes behind
    // when a later component is malformed (for example, contains a colon).
    vector<MMKVPath_t> components;
    while (position < absoluteDirPath.size()) {
        while (position < absoluteDirPath.size() && isPathSeparator(absoluteDirPath[position])) {
            position++;
        }
        if (position == absoluteDirPath.size()) {
            break;
        }
        auto start = position;
        while (position < absoluteDirPath.size() && !isPathSeparator(absoluteDirPath[position])) {
            position++;
        }
        auto component = absoluteDirPath.substr(start, position - start);
        if (component == L".") {
            continue;
        }
        if (!isDirectChildName(component) || component.size() > USHRT_MAX / sizeof(wchar_t)) {
            SetLastError(ERROR_INVALID_NAME);
            return MMKVFileHandleInvalidValue;
        }
        components.emplace_back(std::move(component));
    }

    auto currentFD = openDirectoryRoot(rootPath);
    if (currentFD == MMKVFileHandleInvalidValue) {
        return MMKVFileHandleInvalidValue;
    }
    for (const auto &component : components) {
        auto nextFD = ntOpenDirectoryChild(currentFD, component, create);
        closeFileHandle(currentFD);
        if (nextFD == MMKVFileHandleInvalidValue) {
            return MMKVFileHandleInvalidValue;
        }
        currentFD = nextFD;
    }
    return currentFD;
}

MMKVFileHandle_t openDirectoryHandle(const MMKVPath_t &dirPath) {
    return openDirectoryPathPinned(dirPath, false);
}

MMKVFileHandle_t openOrCreateDirectoryHandle(const MMKVPath_t &dirPath) {
    return openDirectoryPathPinned(dirPath, true);
}

MMKVFileHandle_t openDirectoryInDir(MMKVFileHandle_t dirFD,
                                    const MMKVPath_t &dirPath,
                                    const MMKVPath_t &childName,
                                    bool create) {
    (void) dirPath;
    return ntOpenDirectoryChild(dirFD, childName, create);
}

static MMKVFileHandle_t openRegularChild(MMKVFileHandle_t dirFD,
                                         const MMKVPath_t &dirPath,
                                         const MMKVPath_t &fileName,
                                         ChildOpenMode mode) {
    (void) dirPath;
    MMKVFileHandle_t fileHandle = MMKVFileHandleInvalidValue;
    if (!ntCreateRegularChild(dirFD, fileName, mode, fileHandle)) {
        return MMKVFileHandleInvalidValue;
    }
    BY_HANDLE_FILE_INFORMATION fileInfo = {};
    if (fileHandle == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(fileHandle, &fileInfo) ||
        (fileInfo.dwFileAttributes &
         (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE)) ||
        fileInfo.nNumberOfLinks != 1) {
        closeFileHandle(fileHandle);
        return MMKVFileHandleInvalidValue;
    }
    return fileHandle;
}

MMKVFileHandle_t openRegularFileInDir(MMKVFileHandle_t dirFD,
                                      const MMKVPath_t &dirPath,
                                      const MMKVPath_t &fileName) {
    return openRegularChild(dirFD, dirPath, fileName, ChildOpenMode::ReadOnly);
}

MMKVFileHandle_t openOrCreateRegularFileInDir(MMKVFileHandle_t dirFD,
                                              const MMKVPath_t &dirPath,
                                              const MMKVPath_t &fileName) {
    return openRegularChild(dirFD, dirPath, fileName, ChildOpenMode::ReadWriteCreate);
}

bool openRegularFilePairInDir(MMKVFileHandle_t dirFD,
                              const MMKVPath_t &dirPath,
                              const MMKVPath_t &firstName,
                              const MMKVPath_t &secondName,
                              MMKVFileHandle_t &firstFD,
                              MMKVFileHandle_t &secondFD,
                              bool writable) {
    auto mode = writable ? ChildOpenMode::ReadWriteExisting : ChildOpenMode::ReadOnly;
    firstFD = openRegularChild(dirFD, dirPath, firstName, mode);
    secondFD = MMKVFileHandleInvalidValue;
    if (firstFD == MMKVFileHandleInvalidValue) {
        return false;
    }
    secondFD = openRegularChild(dirFD, dirPath, secondName, mode);
    if (secondFD == MMKVFileHandleInvalidValue) {
        closeFileHandle(firstFD);
        closeFileHandle(secondFD);
        firstFD = MMKVFileHandleInvalidValue;
        secondFD = MMKVFileHandleInvalidValue;
        return false;
    }
    return true;
}

MMKVFileHandle_t openRegularFileInDir(const MMKVPath_t &dirPath, const MMKVPath_t &fileName) {
    auto dirFD = openDirectoryHandle(dirPath);
    if (dirFD == MMKVFileHandleInvalidValue) {
        return MMKVFileHandleInvalidValue;
    }
    auto fileFD = openRegularFileInDir(dirFD, dirPath, fileName);
    closeFileHandle(dirFD);
    return fileFD;
}

bool readFileContent(MMKVFileHandle_t srcFD, void *buffer, size_t size) {
    if (srcFD == INVALID_HANDLE_VALUE || (!buffer && size > 0) || size > MAXDWORD) {
        return false;
    }
    if (size == 0) {
        return true;
    }
    auto temporary = static_cast<uint8_t *>(malloc(size));
    if (!temporary) {
        return false;
    }
    LARGE_INTEGER zero = {};
    if (!SetFilePointerEx(srcFD, zero, nullptr, FILE_BEGIN)) {
        free(temporary);
        return false;
    }
    bool ret = false;
    size_t offset = 0;
    while (offset < size) {
        DWORD count = 0;
        if (!ReadFile(srcFD, temporary + offset, static_cast<DWORD>(size - offset), &count, nullptr) || count == 0) {
            goto exit;
        }
        offset += count;
    }
    memcpy(buffer, temporary, size);
    ret = true;

exit:
    free(temporary);
    return ret;
}

bool copyFileContent(MMKVFileHandle_t srcFD, MMKVFileHandle_t dstFD, bool needTruncate) {
    if (srcFD == INVALID_HANDLE_VALUE || dstFD == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER zero = {};
    if (!SetFilePointerEx(srcFD, zero, nullptr, FILE_BEGIN) || !SetFilePointerEx(dstFD, zero, nullptr, FILE_BEGIN)) {
        return false;
    }

    const auto bufferSize = static_cast<DWORD>(getPageSize());
    auto buffer = static_cast<uint8_t *>(malloc(bufferSize));
    if (!buffer) {
        return false;
    }
    bool ret = false;
    size_t totalSize = 0;
    while (true) {
        DWORD sizeRead = 0;
        if (!ReadFile(srcFD, buffer, bufferSize, &sizeRead, nullptr)) {
            goto exit;
        }
        if (sizeRead == 0) {
            break;
        }
        if (static_cast<size_t>(sizeRead) > std::numeric_limits<size_t>::max() - totalSize) {
            SetLastError(ERROR_ARITHMETIC_OVERFLOW);
            goto exit;
        }
        DWORD totalWrite = 0;
        while (totalWrite < sizeRead) {
            DWORD sizeWrite = 0;
            if (!WriteFile(dstFD, buffer + totalWrite, sizeRead - totalWrite, &sizeWrite, nullptr) || sizeWrite == 0) {
                goto exit;
            }
            totalWrite += sizeWrite;
        }
        totalSize += sizeRead;
    }
    if (needTruncate) {
        size_t destinationSize = 0;
        if (!getFileSize(dstFD, destinationSize)) {
            goto exit;
        }
        DWORD truncateError = ERROR_SUCCESS;
        if (destinationSize != totalSize && !ftruncate(dstFD, totalSize, &truncateError, false)) {
            // A cached MMKV keeps this file mapped while restore writes through
            // a separately pinned handle. Windows refuses to shrink any file
            // with a live mapping. Retaining the larger allocation is safe:
            // MMKV metadata publishes the logical payload size, and the mapping
            // is reloaded after the restore transaction.
            if (!(destinationSize > totalSize && truncateError == ERROR_USER_MAPPED_FILE)) {
                MMKVError("fail to truncate destination to size [%zu], error:%d", totalSize, truncateError);
                goto exit;
            }
            MMKVInfo("retain mapped destination capacity [%zu] instead of shrinking to [%zu]", destinationSize,
                     totalSize);
        }
    }
    ret = true;

exit:
    free(buffer);
    return ret;
}

bool copyFileContent(MMKVFileHandle_t srcFD, MMKVFileHandle_t dstFD) {
    return copyFileContent(srcFD, dstFD, true);
}

static bool splitPath(const MMKVPath_t &path, MMKVPath_t &dirPath, MMKVPath_t &fileName) {
    auto backslash = path.rfind(L'\\');
    auto slash = path.rfind(L'/');
    auto separator = backslash == MMKVPath_t::npos ? slash
                                                    : slash == MMKVPath_t::npos ? backslash : max(backslash, slash);
    if (separator == MMKVPath_t::npos) {
        dirPath = L".";
        fileName = path;
    } else {
        MMKVPath_t rootPath;
        size_t rootEnd = 0;
        if (splitAbsoluteDirectoryRoot(path, rootPath, rootEnd) && separator + 1 == rootEnd) {
            // Preserve the trailing separator required by drive, extended
            // drive, UNC, extended UNC, and Volume-GUID roots.
            dirPath = std::move(rootPath);
        } else {
            dirPath = separator == 0 ? path.substr(0, 1) : path.substr(0, separator);
        }
        fileName = path.substr(separator + 1);
    }
    return isDirectChildName(fileName);
}

bool copyFileContent(MMKVFileHandle_t srcFD,
                     MMKVFileHandle_t dstDirFD,
                     const MMKVPath_t &dstDirPath,
                     const MMKVPath_t &dstFileName) {
    auto dstFD = openOrCreateRegularFileInDir(dstDirFD, dstDirPath, dstFileName);
    if (dstFD == MMKVFileHandleInvalidValue) {
        return false;
    }
    auto ret = copyFileContent(srcFD, dstFD);
    closeFileHandle(dstFD);
    return ret;
}

bool copyFileContent(MMKVFileHandle_t srcFD, const MMKVPath_t &dstPath) {
    MMKVPath_t dstDirPath;
    MMKVPath_t dstFileName;
    if (!splitPath(dstPath, dstDirPath, dstFileName)) {
        return false;
    }
    auto dstDirFD = openDirectoryHandle(dstDirPath);
    if (dstDirFD == MMKVFileHandleInvalidValue) {
        return false;
    }
    auto ret = copyFileContent(srcFD, dstDirFD, dstDirPath, dstFileName);
    closeFileHandle(dstDirFD);
    return ret;
}

static pair<MMKVPath_t, MMKVFileHandle_t> createUniqueTempFileInDir(MMKVFileHandle_t dirFD,
                                                                    const MMKVPath_t &dirPath) {
    static atomic<uint64_t> counter{0};
    using RtlGenRandomFunction = BOOLEAN(WINAPI *)(PVOID, ULONG);
    static auto rtlGenRandom = []() {
        auto module = LoadLibraryW(L"advapi32.dll");
        return module ? reinterpret_cast<RtlGenRandomFunction>(GetProcAddress(module, "SystemFunction036"))
                      : nullptr;
    }();
    uint64_t nonce = 0;
    if (!rtlGenRandom || !rtlGenRandom(&nonce, sizeof(nonce))) {
        return {L"", MMKVFileHandleInvalidValue};
    }
    for (size_t attempt = 0; attempt < 128; attempt++) {
        auto serial = counter.fetch_add(1, memory_order_relaxed);
        auto name = wstring(L".mmkv.tmp.") + to_wstring(GetCurrentProcessId()) + L"." + to_wstring(nonce) + L"." +
                    to_wstring(serial);
        auto tmpFD = openRegularChild(dirFD, dirPath, name, ChildOpenMode::ReadWriteExclusive);
        if (tmpFD != MMKVFileHandleInvalidValue) {
            return {std::move(name), tmpFD};
        }
    }
    return {L"", MMKVFileHandleInvalidValue};
}

MMKVFileHandle_t createTemporaryFileInDir(MMKVFileHandle_t dirFD, const MMKVPath_t &dirPath) {
    if (dirFD == INVALID_HANDLE_VALUE) {
        return MMKVFileHandleInvalidValue;
    }
    auto pair = createUniqueTempFileInDir(dirFD, dirPath);
    auto tmpFD = pair.second;
    if (tmpFD == MMKVFileHandleInvalidValue) {
        return MMKVFileHandleInvalidValue;
    }
    if (!ntDeleteOpenedChild(tmpFD)) {
        closeFileHandle(tmpFD);
        return MMKVFileHandleInvalidValue;
    }
    return tmpFD;
}

bool isSameFile(MMKVFileHandle_t left, MMKVFileHandle_t right) {
    BY_HANDLE_FILE_INFORMATION leftInfo = {};
    BY_HANDLE_FILE_INFORMATION rightInfo = {};
    return GetFileInformationByHandle(left, &leftInfo) && GetFileInformationByHandle(right, &rightInfo) &&
           leftInfo.dwVolumeSerialNumber == rightInfo.dwVolumeSerialNumber &&
           leftInfo.nFileIndexHigh == rightInfo.nFileIndexHigh && leftInfo.nFileIndexLow == rightInfo.nFileIndexLow;
}

static bool copyFileWithIdentity(MMKVFileHandle_t srcFD,
                                 MMKVFileHandle_t dstDirFD,
                                 const MMKVPath_t &dstDirPath,
                                 const MMKVPath_t &dstFileName,
                                 bool destinationWasMissing,
                                 MMKVFileHandle_t *committedIdentity) {
    if (committedIdentity) {
        *committedIdentity = MMKVFileHandleInvalidValue;
    }
    if (!getNtSetInformationFile() || srcFD == INVALID_HANDLE_VALUE || dstDirFD == INVALID_HANDLE_VALUE ||
        !isDirectChildName(dstFileName)) {
        return false;
    }
    auto pair = createUniqueTempFileInDir(dstDirFD, dstDirPath);
    auto tmpFD = pair.second;
    if (tmpFD == INVALID_HANDLE_VALUE) {
        return false;
    }

    auto copied = copyFileContent(srcFD, tmpFD, false);
    bool moved = copied && ntRenameOpenedChild(tmpFD, dstDirFD, dstFileName, !destinationWasMissing);
    bool ret = false;
    if (moved) {
        auto dstFD = openRegularFileInDir(dstDirFD, dstDirPath, dstFileName);
        ret = dstFD != INVALID_HANDLE_VALUE && isSameFile(tmpFD, dstFD);
        closeFileHandle(dstFD);
        if (committedIdentity) {
            // Keep the renamed file's identity even if a concurrent replacement
            // makes verification fail. Rollback can then refuse to delete the
            // replacement while safely removing our file if it is still named.
            *committedIdentity = tmpFD;
            tmpFD = MMKVFileHandleInvalidValue;
        }
    } else if (copied) {
        auto dstFD = destinationWasMissing
                         ? openRegularChild(dstDirFD, dstDirPath, dstFileName, ChildOpenMode::ReadWriteExclusive)
                         : openOrCreateRegularFileInDir(dstDirFD, dstDirPath, dstFileName);
        if (dstFD != MMKVFileHandleInvalidValue) {
            ret = copyFileContent(tmpFD, dstFD);
            if (committedIdentity) {
                // Retain the exact destination opened by this attempt. For a
                // missing entry this enables identity-bound deletion; for an
                // existing entry it keeps rollback writes off a concurrent
                // replacement of the directory name.
                *committedIdentity = dstFD;
                dstFD = MMKVFileHandleInvalidValue;
            }
        }
        closeFileHandle(dstFD);
    }
    if (!moved) {
        ntDeleteOpenedChild(tmpFD);
    }
    closeFileHandle(tmpFD);
    return ret;
}

bool copyFile(MMKVFileHandle_t srcFD,
              MMKVFileHandle_t dstDirFD,
              const MMKVPath_t &dstDirPath,
              const MMKVPath_t &dstFileName) {
    return copyFileWithIdentity(srcFD, dstDirFD, dstDirPath, dstFileName, false, nullptr);
}

enum class RegularChildState { Missing, Regular, Unsafe, Error };

static RegularChildState inspectRegularChild(MMKVFileHandle_t dirFD, const MMKVPath_t &fileName) {
    auto ntCreateFile = getNtCreateFile();
    if (!ntCreateFile || dirFD == INVALID_HANDLE_VALUE || !isDirectChildName(fileName) ||
        fileName.size() > USHRT_MAX / sizeof(wchar_t)) {
        return RegularChildState::Error;
    }

    UNICODE_STRING objectName = {};
    objectName.Buffer = const_cast<PWSTR>(fileName.c_str());
    objectName.Length = static_cast<USHORT>(fileName.size() * sizeof(wchar_t));
    objectName.MaximumLength = objectName.Length;
    OBJECT_ATTRIBUTES objectAttributes = {};
    objectAttributes.Length = sizeof(objectAttributes);
    objectAttributes.RootDirectory = dirFD;
    objectAttributes.ObjectName = &objectName;
    objectAttributes.Attributes = OBJ_CASE_INSENSITIVE | MMKV_OBJ_DONT_REPARSE;

    MMKVFileHandle_t childFD = MMKVFileHandleInvalidValue;
    IO_STATUS_BLOCK ioStatus = {};
    auto status = ntCreateFileWithNoReparseFallback(
        ntCreateFile, &childFD, FILE_READ_ATTRIBUTES | SYNCHRONIZE, &objectAttributes, &ioStatus, nullptr,
        FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, MMKV_FILE_OPEN,
        MMKV_FILE_OPEN_REPARSE_POINT | MMKV_FILE_SYNCHRONOUS_IO_NONALERT, nullptr, 0);
    if (status == MMKV_STATUS_NO_SUCH_FILE || status == MMKV_STATUS_OBJECT_NAME_NOT_FOUND) {
        return RegularChildState::Missing;
    }
    if (status < 0) {
        closeFileHandle(childFD);
        return RegularChildState::Error;
    }
    DWORD attributes = 0;
    auto state = getHandleAttributes(childFD, attributes) &&
                         !(attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT |
                                         FILE_ATTRIBUTE_DEVICE))
                     ? RegularChildState::Regular
                     : RegularChildState::Unsafe;
    closeFileHandle(childFD);
    return state;
}

static bool removeCreatedRegularChild(MMKVFileHandle_t dirFD,
                                      const MMKVPath_t &fileName,
                                      MMKVFileHandle_t expectedFD) {
    auto state = inspectRegularChild(dirFD, fileName);
    if (state == RegularChildState::Missing) {
        return true;
    }
    if (state != RegularChildState::Regular) {
        return false;
    }
    if (expectedFD == MMKVFileHandleInvalidValue) {
        // The name now exists, but without the identity of the file this
        // transaction created it is unsafe to remove it during rollback.
        return false;
    }

    auto ntCreateFile = getNtCreateFile();
    UNICODE_STRING objectName = {};
    objectName.Buffer = const_cast<PWSTR>(fileName.c_str());
    objectName.Length = static_cast<USHORT>(fileName.size() * sizeof(wchar_t));
    objectName.MaximumLength = objectName.Length;
    OBJECT_ATTRIBUTES objectAttributes = {};
    objectAttributes.Length = sizeof(objectAttributes);
    objectAttributes.RootDirectory = dirFD;
    objectAttributes.ObjectName = &objectName;
    objectAttributes.Attributes = OBJ_CASE_INSENSITIVE | MMKV_OBJ_DONT_REPARSE;

    MMKVFileHandle_t childFD = MMKVFileHandleInvalidValue;
    IO_STATUS_BLOCK ioStatus = {};
    auto status = ntCreateFileWithNoReparseFallback(
        ntCreateFile, &childFD, DELETE | FILE_READ_ATTRIBUTES | SYNCHRONIZE, &objectAttributes, &ioStatus, nullptr,
        FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, MMKV_FILE_OPEN,
        MMKV_FILE_NON_DIRECTORY_FILE | MMKV_FILE_OPEN_REPARSE_POINT | MMKV_FILE_SYNCHRONOUS_IO_NONALERT, nullptr,
        0);
    if (status < 0) {
        closeFileHandle(childFD);
        return false;
    }
    DWORD attributes = 0;
    auto removed = getHandleAttributes(childFD, attributes) &&
                   !(attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE)) &&
                   isSameFile(childFD, expectedFD) && ntDeleteOpenedChild(childFD);
    closeFileHandle(childFD);
    return removed;
}

bool openOrCreateRegularFilePairInDir(MMKVFileHandle_t dirFD,
                                      const MMKVPath_t &dirPath,
                                      const MMKVPath_t &firstName,
                                      const MMKVPath_t &secondName,
                                      MMKVFileHandle_t &firstFD,
                                      MMKVFileHandle_t &secondFD,
                                      bool &firstCreated,
                                      bool &secondCreated) {
    firstFD = MMKVFileHandleInvalidValue;
    secondFD = MMKVFileHandleInvalidValue;
    firstCreated = false;
    secondCreated = false;
    if (dirFD == INVALID_HANDLE_VALUE || firstName == secondName || !isDirectChildName(firstName) ||
        !isDirectChildName(secondName)) {
        return false;
    }
    auto firstState = inspectRegularChild(dirFD, firstName);
    auto secondState = inspectRegularChild(dirFD, secondName);
    if (firstState == RegularChildState::Unsafe || firstState == RegularChildState::Error ||
        secondState == RegularChildState::Unsafe || secondState == RegularChildState::Error) {
        return false;
    }
    firstFD = openRegularChild(dirFD, dirPath, firstName,
                               firstState == RegularChildState::Missing ? ChildOpenMode::ReadWriteExclusive
                                                                        : ChildOpenMode::ReadWriteExisting);
    firstCreated = firstFD != MMKVFileHandleInvalidValue && firstState == RegularChildState::Missing;
    if (firstFD != MMKVFileHandleInvalidValue) {
        secondFD = openRegularChild(dirFD, dirPath, secondName,
                                    secondState == RegularChildState::Missing ? ChildOpenMode::ReadWriteExclusive
                                                                             : ChildOpenMode::ReadWriteExisting);
        secondCreated = secondFD != MMKVFileHandleInvalidValue && secondState == RegularChildState::Missing;
    }
    if (firstFD != MMKVFileHandleInvalidValue && secondFD != MMKVFileHandleInvalidValue) {
        return true;
    }
    if (firstCreated && !removeCreatedRegularChild(dirFD, firstName, firstFD)) {
        const auto &utf8Path = MMKVPath_t2String(childPath(dirPath, firstName));
        MMKVError("failed to clean newly created file [%s]", utf8Path.c_str());
    }
    if (secondCreated && !removeCreatedRegularChild(dirFD, secondName, secondFD)) {
        const auto &utf8Path = MMKVPath_t2String(childPath(dirPath, secondName));
        MMKVError("failed to clean newly created file [%s]", utf8Path.c_str());
    }
    closeFileHandle(firstFD);
    closeFileHandle(secondFD);
    firstFD = MMKVFileHandleInvalidValue;
    secondFD = MMKVFileHandleInvalidValue;
    firstCreated = false;
    secondCreated = false;
    return false;
}

bool copyFilePair(MMKVFileHandle_t firstSrcFD,
                  MMKVFileHandle_t secondSrcFD,
                  MMKVFileHandle_t dstDirFD,
                  const MMKVPath_t &dstDirPath,
                  const MMKVPath_t &firstDstName,
                  const MMKVPath_t &secondDstName) {
    if (firstSrcFD == INVALID_HANDLE_VALUE || secondSrcFD == INVALID_HANDLE_VALUE ||
        dstDirFD == INVALID_HANDLE_VALUE || firstDstName == secondDstName || !isDirectChildName(firstDstName) ||
        !isDirectChildName(secondDstName)) {
        return false;
    }

    auto stagedFirst = createTemporaryFileInDir(dstDirFD, dstDirPath);
    auto stagedSecond = createTemporaryFileInDir(dstDirFD, dstDirPath);
    if (stagedFirst == INVALID_HANDLE_VALUE || stagedSecond == INVALID_HANDLE_VALUE ||
        !copyFileContent(firstSrcFD, stagedFirst) || !copyFileContent(secondSrcFD, stagedSecond)) {
        closeFileHandle(stagedFirst);
        closeFileHandle(stagedSecond);
        return false;
    }

    auto firstState = inspectRegularChild(dstDirFD, firstDstName);
    auto secondState = inspectRegularChild(dstDirFD, secondDstName);
    if (firstState == RegularChildState::Unsafe || firstState == RegularChildState::Error ||
        secondState == RegularChildState::Unsafe || secondState == RegularChildState::Error) {
        closeFileHandle(stagedFirst);
        closeFileHandle(stagedSecond);
        return false;
    }

    auto oldFirst = firstState == RegularChildState::Regular
                        ? openRegularFileInDir(dstDirFD, dstDirPath, firstDstName)
                        : MMKVFileHandleInvalidValue;
    auto oldSecond = secondState == RegularChildState::Regular
                         ? openRegularFileInDir(dstDirFD, dstDirPath, secondDstName)
                         : MMKVFileHandleInvalidValue;
    if ((firstState == RegularChildState::Regular && oldFirst == INVALID_HANDLE_VALUE) ||
        (secondState == RegularChildState::Regular && oldSecond == INVALID_HANDLE_VALUE)) {
        closeFileHandle(oldFirst);
        closeFileHandle(oldSecond);
        closeFileHandle(stagedFirst);
        closeFileHandle(stagedSecond);
        return false;
    }
    auto savedFirst = MMKVFileHandleInvalidValue;
    auto savedSecond = MMKVFileHandleInvalidValue;
    if (firstState == RegularChildState::Regular) {
        savedFirst = createTemporaryFileInDir(dstDirFD, dstDirPath);
    }
    if (secondState == RegularChildState::Regular) {
        savedSecond = createTemporaryFileInDir(dstDirFD, dstDirPath);
    }
    if ((firstState == RegularChildState::Regular &&
         (savedFirst == INVALID_HANDLE_VALUE || !copyFileContent(oldFirst, savedFirst))) ||
        (secondState == RegularChildState::Regular &&
         (savedSecond == INVALID_HANDLE_VALUE || !copyFileContent(oldSecond, savedSecond)))) {
        closeFileHandle(oldFirst);
        closeFileHandle(oldSecond);
        closeFileHandle(savedFirst);
        closeFileHandle(savedSecond);
        closeFileHandle(stagedFirst);
        closeFileHandle(stagedSecond);
        return false;
    }
    closeFileHandle(oldFirst);
    closeFileHandle(oldSecond);

    MMKVFileHandle_t committedFirst = MMKVFileHandleInvalidValue;
    MMKVFileHandle_t committedSecond = MMKVFileHandleInvalidValue;
    auto firstCommitted = copyFileWithIdentity(stagedFirst, dstDirFD, dstDirPath, firstDstName,
                                               firstState == RegularChildState::Missing, &committedFirst);
    auto secondAttempted = firstCommitted;
    auto secondCommitted =
        secondAttempted && copyFileWithIdentity(stagedSecond, dstDirFD, dstDirPath, secondDstName,
                                                secondState == RegularChildState::Missing, &committedSecond);
    if (!secondCommitted) {
        auto firstRestored = firstState == RegularChildState::Regular
                                 ? (committedFirst == MMKVFileHandleInvalidValue ||
                                    copyFileContent(savedFirst, committedFirst))
                                 : removeCreatedRegularChild(dstDirFD, firstDstName, committedFirst);
        auto secondRestored = !secondAttempted ||
                              (secondState == RegularChildState::Regular
                                   ? (committedSecond == MMKVFileHandleInvalidValue ||
                                      copyFileContent(savedSecond, committedSecond))
                                   : removeCreatedRegularChild(dstDirFD, secondDstName, committedSecond));
        if (!firstRestored || !secondRestored) {
            const auto &utf8Path = MMKVPath_t2String(dstDirPath);
            MMKVError("failed to roll back destination file pair in [%s]", utf8Path.c_str());
        }
    }

    closeFileHandle(savedFirst);
    closeFileHandle(savedSecond);
    closeFileHandle(committedFirst);
    closeFileHandle(committedSecond);
    closeFileHandle(stagedFirst);
    closeFileHandle(stagedSecond);
    return secondCommitted;
}

bool copyFileContentPair(MMKVFileHandle_t firstSrcFD,
                         MMKVFileHandle_t secondSrcFD,
                         MMKVFileHandle_t firstDstFD,
                         MMKVFileHandle_t secondDstFD,
                         MMKVFileHandle_t tempDirFD,
                         const MMKVPath_t &tempDirPath) {
    if (firstSrcFD == INVALID_HANDLE_VALUE || secondSrcFD == INVALID_HANDLE_VALUE ||
        firstDstFD == INVALID_HANDLE_VALUE || secondDstFD == INVALID_HANDLE_VALUE ||
        tempDirFD == INVALID_HANDLE_VALUE) {
        return false;
    }

    auto stagedFirst = createTemporaryFileInDir(tempDirFD, tempDirPath);
    auto stagedSecond = createTemporaryFileInDir(tempDirFD, tempDirPath);
    auto savedFirst = createTemporaryFileInDir(tempDirFD, tempDirPath);
    auto savedSecond = createTemporaryFileInDir(tempDirFD, tempDirPath);
    if (stagedFirst == INVALID_HANDLE_VALUE || stagedSecond == INVALID_HANDLE_VALUE ||
        savedFirst == INVALID_HANDLE_VALUE || savedSecond == INVALID_HANDLE_VALUE ||
        !copyFileContent(firstSrcFD, stagedFirst) || !copyFileContent(secondSrcFD, stagedSecond) ||
        !copyFileContent(firstDstFD, savedFirst) || !copyFileContent(secondDstFD, savedSecond)) {
        closeFileHandle(stagedFirst);
        closeFileHandle(stagedSecond);
        closeFileHandle(savedFirst);
        closeFileHandle(savedSecond);
        return false;
    }

    auto firstCommitted = copyFileContent(stagedFirst, firstDstFD);
    auto secondAttempted = firstCommitted;
    auto secondCommitted = secondAttempted && copyFileContent(stagedSecond, secondDstFD);
    if (!secondCommitted) {
        auto firstRestored = copyFileContent(savedFirst, firstDstFD);
        auto secondRestored = !secondAttempted || copyFileContent(savedSecond, secondDstFD);
        if (!firstRestored || !secondRestored) {
            MMKVError("failed to roll back destination file contents");
        }
    }

    closeFileHandle(stagedFirst);
    closeFileHandle(stagedSecond);
    closeFileHandle(savedFirst);
    closeFileHandle(savedSecond);
    return secondCommitted;
}

bool copyFileContentPair(MMKVFileHandle_t firstSrcFD,
                         MMKVFileHandle_t secondSrcFD,
                         MMKVFileHandle_t firstDstFD,
                         MMKVFileHandle_t secondDstFD,
                         MMKVFileHandle_t tempDirFD,
                         const MMKVPath_t &tempDirPath,
                         const MMKVPath_t &firstDstName,
                         const MMKVPath_t &secondDstName,
                         bool firstCreated,
                         bool secondCreated) {
    auto ret = copyFileContentPair(firstSrcFD, secondSrcFD, firstDstFD, secondDstFD, tempDirFD, tempDirPath);
    if (!ret) {
        auto firstCleaned = !firstCreated ||
                            removeCreatedRegularChild(tempDirFD, firstDstName, firstDstFD);
        auto secondCleaned = !secondCreated ||
                             removeCreatedRegularChild(tempDirFD, secondDstName, secondDstFD);
        if (!firstCleaned || !secondCleaned) {
            const auto &utf8Path = MMKVPath_t2String(tempDirPath);
            MMKVError("failed to restore original destination pair state in [%s]", utf8Path.c_str());
        }
    }
    return ret;
}

bool copyFile(MMKVFileHandle_t srcFD, const MMKVPath_t &dstPath) {
    MMKVPath_t dstDirPath;
    MMKVPath_t dstFileName;
    if (!splitPath(dstPath, dstDirPath, dstFileName)) {
        return false;
    }
    auto dstDirFD = openDirectoryHandle(dstDirPath);
    if (dstDirFD == MMKVFileHandleInvalidValue) {
        return false;
    }
    auto ret = copyFile(srcFD, dstDirFD, dstDirPath, dstFileName);
    closeFileHandle(dstDirFD);
    return ret;
}

bool tryAtomicRename(const MMKVPath_t &srcPath, const MMKVPath_t &dstPath) {
    if (MoveFileEx(srcPath.c_str(), dstPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) == 0) {
        const auto &utf8SrcPath = MMKVPath_t2String(srcPath);
        const auto &utf8DstPath = MMKVPath_t2String(dstPath);
        MMKVError("MoveFileEx [%s] to [%s] failed %d", utf8SrcPath.c_str(), utf8DstPath.c_str(), GetLastError());
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
        if (!ReadFile(srcFile.getFd(), buffer, (DWORD) bufferSize, &sizeRead, nullptr)) {
            MMKVError("fail to read %s: %d", srcFile.getUTF8Path().c_str(), GetLastError());
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
    MMKVInfo("copy content from %s to fd[%d] finish", srcFile.getUTF8Path().c_str(), dstFD);

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
        const auto &utf8SrcPath = MMKVPath_t2String(srcPath);
        const auto &utf8TmpPath = MMKVPath_t2String(tmpPath);
        MMKVInfo("copied file [%s] to [%s]", utf8SrcPath.c_str(), utf8TmpPath.c_str());
        CloseHandle(tmpFD);
        renamed = tryAtomicRename(tmpPath.c_str(), dstPath.c_str());
        if (renamed) {
            const auto &utf8DstPath = MMKVPath_t2String(dstPath);
            MMKVInfo("copyfile [%s] to [%s] finish.", utf8SrcPath.c_str(), utf8DstPath.c_str());
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
        MMKVError("fail to copyfile(): target file %s", dstFile.getUTF8Path().c_str());
    } else {
        const auto &utf8SrcPath = MMKVPath_t2String(srcPath);
        MMKVInfo("copy content from %s to [%s] finish", utf8SrcPath.c_str(), dstFile.getUTF8Path().c_str());
    }
    return ret;
}

bool copyFileContent(const MMKVPath_t &srcPath, MMKVFileHandle_t dstFD) {
    return copyFileContent(srcPath, dstFD, true);
}

bool walkInOpenedDir(MMKVFileHandle_t dirFD,
                     const MMKVPath_t &dirPath,
                     WalkType type,
                     const std::function<void(const MMKVPath_t &, WalkType)> &walker) {
    (void) dirPath;
    auto ntQueryDirectoryFile = getNtQueryDirectoryFile();
    if (!ntQueryDirectoryFile || dirFD == INVALID_HANDLE_VALUE) {
        return false;
    }

    vector<pair<MMKVPath_t, WalkType>> children;
    vector<uint8_t> buffer(256 * 1024);
    BOOLEAN restartScan = TRUE;
    while (true) {
        IO_STATUS_BLOCK ioStatus = {};
        auto status = ntQueryDirectoryFile(dirFD, nullptr, nullptr, nullptr, &ioStatus, buffer.data(),
                                           static_cast<ULONG>(buffer.size()),
                                           MMKV_FILE_BOTH_DIRECTORY_INFORMATION, FALSE, nullptr, restartScan);
        restartScan = FALSE;
        if (status == MMKV_STATUS_NO_MORE_FILES) {
            break;
        }
        if (status < 0 || ioStatus.Information == 0 || ioStatus.Information > buffer.size()) {
            return false;
        }

        size_t offset = 0;
        const auto bytesReturned = static_cast<size_t>(ioStatus.Information);
        while (true) {
            constexpr size_t headerSize = offsetof(MMKVFileBothDirectoryInformation, FileName);
            if (offset > bytesReturned || bytesReturned - offset < headerSize) {
                return false;
            }
            auto entry = reinterpret_cast<const MMKVFileBothDirectoryInformation *>(buffer.data() + offset);
            if ((entry->FileNameLength % sizeof(wchar_t)) != 0 ||
                entry->FileNameLength > bytesReturned - offset - headerSize) {
                return false;
            }
            MMKVPath_t fileName(entry->FileName, entry->FileNameLength / sizeof(wchar_t));
            if (fileName != L"." && fileName != L".." &&
                !(entry->FileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE))) {
                if ((entry->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (type & WalkFolder)) {
                    children.emplace_back(std::move(fileName), WalkFolder);
                } else if (!(entry->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (type & WalkFile)) {
                    children.emplace_back(std::move(fileName), WalkFile);
                }
            }
            if (entry->NextEntryOffset == 0) {
                break;
            }
            if (entry->NextEntryOffset < headerSize || entry->NextEntryOffset > bytesReturned - offset) {
                return false;
            }
            offset += entry->NextEntryOffset;
        }
    }

    for (const auto &child : children) {
        walker(child.first, child.second);
    }
    return true;
}

void walkInDir(const MMKVPath_t &dirPath,
               WalkType type,
               const std::function<void(const MMKVPath_t &, WalkType)> &walker) {
    auto dirFD = openDirectoryHandle(dirPath);
    if (dirFD == MMKVFileHandleInvalidValue) {
        return;
    }
    walkInOpenedDir(dirFD, dirPath, type, [&](const MMKVPath_t &fileName, WalkType childType) {
        walker(childPath(dirPath, fileName), childType);
    });
    closeFileHandle(dirFD);
}

bool isDiskOfMMAPFileCorrupted(MemoryFile *file, bool &needReportReadFail) {
    // make sure the file is valid
    __try {
        auto filesize = file->getFileSize();
        volatile uint8_t* ptr = (uint8_t*) file->getMemory();
        // check the head of every page
        for (size_t index = 0; index < filesize; index += DEFAULT_MMAP_SIZE) {
            volatile uint8_t byte = ptr[index];
            MMKVDebug("%zu byte of the file: 0x%x", index, byte);
        }
        // check the very last byte of the file
        if (filesize > 1) {
            volatile uint8_t byte = ptr[filesize - 1];
            MMKVDebug("%zu byte of the file: 0x%x", filesize - 1, byte);
        }
    }
    __except ((GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR || GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
              ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        needReportReadFail = true;
        DWORD errorCode = GetExceptionCode();
        MMKVError("fail to mmap [%s], %d", file->getUTF8Path().c_str(), errorCode);
        return true;
    }
    return false;
}

bool deleteFile(const MMKVPath_t &path) {
    if (!DeleteFile(path.c_str())) {
        const auto &utf8Path = MMKVPath_t2String(path);
        MMKVError("failed to delete file [%s], %d", utf8Path.c_str(), GetLastError());
        return false;
    }
    return true;
}

std::optional<MMKVPath_t> getUniqueFileName(const MMKVPath_t &folder, const MMKVPath_t &prefix) {
    // Buffer for the resulting path
    wchar_t tempFileName[MAX_PATH];
    UINT uUnique = 0;

    UINT result = GetTempFileName(folder.c_str(), prefix.c_str(), uUnique, tempFileName);
    if (result == 0) {
        const auto &utf8Folder = MMKVPath_t2String(folder);
        MMKVError("failed to GetTempFileName file [%s], %d", utf8Folder.c_str(), GetLastError());
        return std::nullopt;
    }

    return std::wstring(tempFileName);
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
