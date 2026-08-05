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
#    include <atomic>
#    include <cerrno>
#    include <cstdlib>
#    include <utility>
#    include <fcntl.h>
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <unistd.h>
#    include <sys/file.h>
#    include <dirent.h>
#    include <cstring>
#    include <filesystem>
#    include <limits>
#    include <random>

using namespace std;
namespace fs = std::filesystem;

namespace mmkv {

static bool getFileSize(const char *path, size_t &size);

static bool getFileIdentity(MMKVFileHandle_t fd, array<uint64_t, 2> &identity) {
    if (fd < 0) {
        return false;
    }
    struct stat info = {};
    if (::fstat(fd, &info) != 0) {
        return false;
    }
    static_assert(sizeof(info.st_dev) <= sizeof(uint64_t), "unsupported device identifier width");
    static_assert(sizeof(info.st_ino) <= sizeof(uint64_t), "unsupported inode identifier width");
    identity[0] = static_cast<uint64_t>(info.st_dev);
    identity[1] = static_cast<uint64_t>(info.st_ino);
    return true;
}

static MMKVFileHandle_t duplicateFileHandle(MMKVFileHandle_t fileHandle) {
    if (fileHandle < 0) {
        return -1;
    }
    int duplicatedFD = -1;
#    ifdef F_DUPFD_CLOEXEC
    do {
        duplicatedFD = ::fcntl(fileHandle, F_DUPFD_CLOEXEC, 0);
    } while (duplicatedFD < 0 && errno == EINTR);
#    else
    do {
        duplicatedFD = ::dup(fileHandle);
    } while (duplicatedFD < 0 && errno == EINTR);
    if (duplicatedFD >= 0 && ::fcntl(duplicatedFD, F_SETFD, FD_CLOEXEC) != 0) {
        ::close(duplicatedFD);
        duplicatedFD = -1;
    }
#    endif
    return duplicatedFD;
}

#    ifdef MMKV_ANDROID
extern size_t ASharedMemory_getSize(int fd);
#    else
File::File(MMKVPath_t path, OpenFlag flag) : m_path(std::move(path)), m_fd(-1), m_flag(flag) {
    open();
}

MemoryFile::MemoryFile(MMKVPath_t path, size_t expectedCapacity, bool readOnly, bool mayflyFD)
    : m_diskFile(std::move(path), readOnly ? OpenFlag::ReadOnly : (OpenFlag::ReadWrite | OpenFlag::Create))
    , m_ptr(nullptr), m_size(0), m_readOnly(readOnly), m_isMayflyFD(mayflyFD)
{
    reloadFromFile(expectedCapacity);
}
#    endif // !defined(MMKV_ANDROID)

#    ifdef MMKV_IOS
void tryResetFileProtection(const string &path);
#    endif

static int OpenFlag2NativeFlag(OpenFlag flag) {
    int native = O_CLOEXEC;
    if ((flag & OpenFlagRWMask) == OpenFlag::ReadWrite) {
        native |= O_RDWR;
    } else if (flag & OpenFlag::ReadOnly) {
        native |= O_RDONLY;
    } else if (flag & OpenFlag::WriteOnly) {
        native |= O_WRONLY;
    }

    if (flag & OpenFlag::Create) {
        native |= O_CREAT;
    }
    if (flag & OpenFlag::Excel) {
        native |= O_EXCL;
    }
    if (flag & OpenFlag::Truncate) {
        native |= O_TRUNC;
    }
    return native;
}

bool File::open(bool existingOnly) {
#    ifdef MMKV_ANDROID
    if (m_fileType == MMFILE_TYPE_ASHMEM) {
        return isFileValid();
    }
#    endif
    if (isFileValid()) {
        return true;
    }
    auto nativeFlag = OpenFlag2NativeFlag(m_flag);
    if (existingOnly) {
        nativeFlag &= ~(O_CREAT | O_EXCL | O_TRUNC);
#    ifdef O_NOFOLLOW
        nativeFlag |= O_NOFOLLOW;
#    endif
#    ifdef O_NONBLOCK
        nativeFlag |= O_NONBLOCK;
#    endif
    }
    m_fd = ::open(m_path.c_str(), nativeFlag, S_IRWXU);
    if (!isFileValid()) {
        MMKVError("fail to open [%s], flag 0x%x, %d(%s)", m_path.c_str(), m_flag, errno, strerror(errno));
        return false;
    }
    MMKVInfo("open fd[%d], flag 0x%x, %s", m_fd, m_flag, m_path.c_str());
    return true;
}

void File::close() {
    if (isFileValid()) {
        MMKVInfo("closing fd[%d], %s", m_fd, m_path.c_str());
        if (::close(m_fd) == 0) {
            m_fd = -1;
        } else {
            MMKVError("fail to close [%s], %d(%s)", m_path.c_str(), errno, strerror(errno));
        }
    }
}

size_t File::getActualFileSize() const {
#    ifdef MMKV_ANDROID
    if (m_fileType == MMFILE_TYPE_ASHMEM) {
        return ASharedMemory_getSize(m_fd);
    }
#    endif
    size_t size = 0;
    if (isFileValid()) {
        mmkv::getFileSize(m_fd, size);
    } else {
        mmkv::getFileSize(m_path.c_str(), size);
    }
    return size;
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

bool MemoryFile::openIfNeeded() {
    auto requireExistingIdentity = m_hasMappedFileIdentity || m_hasExpectedFileIdentity;
    if (!m_diskFile.isFileValid() && !m_diskFile.open(requireExistingIdentity)) {
        return false;
    }
    if (m_hasMappedFileIdentity && !isMappedFile(m_diskFile.getFd())) {
        MMKVError("refuse to reopen mapped file through replaced path [%s]", m_diskFile.m_path.c_str());
        m_diskFile.close();
        return false;
    }
    if (!m_ptr && m_hasExpectedFileIdentity) {
        array<uint64_t, 2> identity = {};
        if (!getFileIdentity(m_diskFile.getFd(), identity) || identity != m_expectedFileIdentity) {
            MMKVError("refuse to reload file through replaced path [%s]", m_diskFile.m_path.c_str());
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
    if (m_isMayflyFD && !m_keepFileHandleForReload && m_diskFile.isFileValid()) {
        m_diskFile.close();
    }
}

size_t MemoryFile::getActualFileSize() {
    if (!m_isMayflyFD && !m_diskFile.isFileValid()) {
        return 0;
    }
    auto openedHere = !m_diskFile.isFileValid();
    if (!openIfNeeded()) {
        // A live mapping still has a trustworthy captured extent. Returning
        // that extent is safer than treating a replacement-path failure as a
        // size change and remapping through the replaced path.
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
#    ifdef MMKV_ANDROID
    if (m_diskFile.m_fileType == MMFILE_TYPE_ASHMEM) {
        if (size > m_size) {
            MMKVError("ashmem %s reach size limit:%zu, consider configure with larger size", m_diskFile.m_path.c_str(), m_size);
        } else {
            MMKVInfo("no way to trim ashmem %s from %zu to smaller size %zu", m_diskFile.m_path.c_str(), m_size, size);
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

    if (::ftruncate(m_diskFile.m_fd, static_cast<off_t>(m_size)) != 0) {
        MMKVError("fail to truncate [%s] to size %zu, %s", m_diskFile.m_path.c_str(), m_size, strerror(errno));
        m_size = oldSize;
        return false;
    }
    if (m_size > oldSize) {
        if (!zeroFillFile(m_diskFile.m_fd, oldSize, m_size - oldSize)) {
            MMKVError("fail to zeroFile [%s] to size %zu, %s", m_diskFile.m_path.c_str(), m_size, strerror(errno));
            m_size = oldSize;

            // redo ftruncate to its previous size
            int status = ::ftruncate(m_diskFile.m_fd, static_cast<off_t>(m_size));
            if (status != 0) {
                MMKVError("failed to truncate back [%s] to size %zu, %s", m_diskFile.m_path.c_str(), m_size, strerror(errno));
            } else {
                MMKVError("success to truncate [%s] back to size %zu", m_diskFile.m_path.c_str(), m_size);
                MMKVError("after truncate, file size = %zu", getActualFileSize());
            }

            return false;
        }
    }

    if (m_ptr) {
        if (munmap(m_ptr, oldSize) != 0) {
            MMKVError("fail to munmap [%s], %s", m_diskFile.m_path.c_str(), strerror(errno));
        }
        m_ptr = nullptr;
    }
    return mmapOrCleanup(fileLock);
}

bool MemoryFile::msync(SyncFlag syncFlag) {
    if (m_readOnly) {
        // there's no point in msync() readonly memory
        return true;
    }
    if (m_ptr) {
        auto ret = ::msync(m_ptr, m_size, syncFlag ? MS_SYNC : MS_ASYNC);
        if (ret == 0) {
            return true;
        }
        MMKVError("fail to msync [%s], %s", m_diskFile.m_path.c_str(), strerror(errno));
    }
    return false;
}

bool MemoryFile::mmapOrCleanup(FileLock *fileLock) {
    auto oldPtr = m_ptr;
    auto mode = m_readOnly ? PROT_READ : (PROT_READ | PROT_WRITE);
    m_ptr = (char *) ::mmap(m_ptr, m_size, mode, MAP_SHARED, m_diskFile.m_fd, 0);
    if (m_ptr == MAP_FAILED) {
        MMKVError("fail to mmap [%s], mode 0x%x, %s", m_diskFile.m_path.c_str(), mode, strerror(errno));
        m_ptr = nullptr;

#    ifdef MMKV_ANDROID
        doCleanMemoryCache(m_diskFile.m_fileType == MMFILE_TYPE_ASHMEM);
#    else
        doCleanMemoryCache(false);
#    endif
        return false;
    }
    if (!captureMappedFileIdentity()) {
        MMKVError("fail to capture mapped file identity [%s], %s", m_diskFile.m_path.c_str(), strerror(errno));
#    ifdef MMKV_ANDROID
        doCleanMemoryCache(m_diskFile.m_fileType == MMFILE_TYPE_ASHMEM);
#    else
        doCleanMemoryCache(false);
#    endif
        return false;
    }
    MMKVInfo("mmap to address [%p], oldPtr [%p], [%s]", m_ptr, oldPtr, m_diskFile.m_path.c_str());

    if (m_isMayflyFD && fileLock) {
        fileLock->destroyAndUnLock();
    }

    cleanMayflyFD();
    return true;
}

void MemoryFile::reloadFromFile(size_t expectedCapacity) {
#    ifdef MMKV_ANDROID
    if (m_fileType == MMFILE_TYPE_ASHMEM) {
        return;
    }
#    endif
    if (isFileValid()) {
        MMKVWarning("calling reloadFromFile while the cache [%s] is still valid", m_diskFile.m_path.c_str());
        MMKV_ASSERT(0);
        doCleanMemoryCache(false);
    }

    if (openIfNeeded()) {
        FileLock fileLock(m_diskFile.m_fd);
        InterProcessLock lock(&fileLock, SharedLockType);
        SCOPED_LOCK(&lock);

        mmkv::getFileSize(m_diskFile.m_fd, m_size);
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
#    ifdef MMKV_IOS
        if (!m_readOnly) {
            tryResetFileProtection(m_diskFile.m_path);
        }
#    endif
    }
}

bool MemoryFile::reloadFromFileHandle(MMKVFileHandle_t fileHandle, size_t expectedCapacity) {
#    ifdef MMKV_ANDROID
    if (m_fileType != MMFILE_TYPE_FILE) {
        return false;
    }
#    endif
    if (fileHandle < 0) {
        return false;
    }
    array<uint64_t, 2> intendedIdentity = {};
    if (!getFileIdentity(fileHandle, intendedIdentity)) {
        return false;
    }
    auto mappingFD = duplicateFileHandle(fileHandle);
    auto retainedFD = duplicateFileHandle(fileHandle);
    if (mappingFD < 0 || retainedFD < 0) {
        closeFileHandle(mappingFD);
        closeFileHandle(retainedFD);
        if (!m_hasMappedFileIdentity) {
            m_expectedFileIdentity = intendedIdentity;
            m_hasExpectedFileIdentity = true;
        }
        return false;
    }

    doCleanMemoryCache(false);
    // This is an intentional handle-bound remap. The intended identity was
    // captured above and will be installed by mmapOrCleanup(); do not make
    // openIfNeeded() compare the adopted descriptor with stale mapping state.
    clearMappedFileIdentity();
    m_expectedFileIdentity = {};
    m_hasExpectedFileIdentity = false;
    m_diskFile.m_fd = mappingFD;
    reloadFromFile(expectedCapacity);
    if (m_ptr && m_hasMappedFileIdentity && m_mappedFileIdentity == intendedIdentity) {
        if (m_isMayflyFD) {
            // mmapOrCleanup() closed the adopted descriptor. Retain another
            // duplicate through the caller's loadFromFile() so its
            // multiprocess size check remains handle-bound. The original
            // handle may itself have belonged to this MemoryFile and been
            // closed by doCleanMemoryCache(), so use the duplicate retained
            // before cleanup rather than touching fileHandle again.
            if (!m_diskFile.isFileValid()) {
                m_diskFile.m_fd = retainedFD;
                retainedFD = MMKVFileHandleInvalidValue;
            }
            if (!m_diskFile.isFileValid()) {
                doCleanMemoryCache(false);
                m_expectedFileIdentity = intendedIdentity;
                m_hasExpectedFileIdentity = true;
                closeFileHandle(retainedFD);
                return false;
            }
            m_keepFileHandleForReload = true;
        }
        closeFileHandle(retainedFD);
        return true;
    }

    // mmapOrCleanup() closes the adopted descriptor on failure. Keep the
    // duplicate pinned before cleanup so a retry cannot resolve m_path to an
    // unrelated replacement, including when fileHandle was our own mayfly fd.
    doCleanMemoryCache(false);
    clearMappedFileIdentity();
    m_diskFile.m_fd = retainedFD;
    m_keepFileHandleForReload = m_diskFile.isFileValid();
    m_expectedFileIdentity = intendedIdentity;
    m_hasExpectedFileIdentity = true;
    return false;
}

void MemoryFile::doCleanMemoryCache(bool forceClean) {
#    ifdef MMKV_ANDROID
    if (m_diskFile.m_fileType == MMFILE_TYPE_ASHMEM && !forceClean) {
        return;
    }
#    endif
    if (m_ptr && m_ptr != MAP_FAILED) {
        if (munmap(m_ptr, m_size) != 0) {
            MMKVError("fail to munmap [%s], %s", m_diskFile.m_path.c_str(), strerror(errno));
        } else {
            MMKVInfo("munmap from address [%p], [%s]", m_ptr, m_diskFile.m_path.c_str());
        }
    }
    m_ptr = nullptr;
    // A normal cache clear unmaps the data but the cached MMKV still refers to
    // the same file. Preserve its last verified identity so a lazy backup,
    // restore, or reload cannot accept replacement data merely because the
    // metadata mapping still matches. Destruction and hard failures force a
    // complete identity reset.
    if (forceClean) {
        clearMappedFileIdentity();
        m_expectedFileIdentity = {};
        m_hasExpectedFileIdentity = false;
    }
    m_keepFileHandleForReload = false;

    m_diskFile.close();
    m_size = 0;
}

bool isFileExist(const string &nsFilePath) {
    if (nsFilePath.empty()) {
        return false;
    }

    return access(nsFilePath.c_str(), F_OK) == 0;
}

#ifndef MMKV_APPLE
extern bool mkPath(const MMKVPath_t &str) {
    if (str.empty() || str.find('\0') != MMKVPath_t::npos) {
        errno = EINVAL;
        return false;
    }
    char *path = strdup(str.c_str());
    if (!path) {
        return false;
    }

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
                // there's report that some Android devices might not have access permission on parent dir
                if (done) {
                    free(path);
                    return false;
                }
                goto LContinue;
            }
        } else if (!S_ISDIR(sb.st_mode)) {
            MMKVWarning("%s: %s", path, strerror(ENOTDIR));
            free(path);
            return false;
        }
LContinue:
        *slash = '/';
    }
    free(path);

    return true;
}
#else
// avoid using so-called privacy API
extern bool mkPath(const MMKVPath_t &str) {
    if (str.empty() || str.find('\0') != MMKVPath_t::npos) {
        errno = EINVAL;
        return false;
    }
    auto path = [NSString stringWithUTF8String:str.c_str()];
    NSError *error = nil;
    auto ret = [[NSFileManager defaultManager] createDirectoryAtPath:path withIntermediateDirectories:YES attributes:nil error:&error];
    if (!ret) {
        MMKVWarning("%s", error.localizedDescription.UTF8String);
        return false;
    }
    return true;
}
#endif

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

#ifndef MMKV_APPLE

bool getFileSize(int fd, size_t &size) {
    struct stat st = {};
    if (fstat(fd, &st) != -1) {
        size = (size_t) st.st_size;
        return true;
    }
    return false;
}

bool getFileSize(const char *path, size_t &size) {
    struct stat st = {};
    if (stat(path, &st) != -1) {
        size = (size_t) st.st_size;
        return true;
    }
    return false;
}

#else // !MMKV_APPLE

// avoid using so-called privacy API
bool getFileSize(int fd, size_t &size) {
    auto cur = lseek(fd, 0, SEEK_CUR);
    if (cur == -1) {
        return false;
    }
    auto end = lseek(fd, 0, SEEK_END);
    if (end == -1) {
        return false;
    }
    size = (size_t) end;

    lseek(fd, cur, SEEK_SET);
    return true;
}

bool getFileSize(const char *path, size_t &size) {
    auto fd = open(path, O_RDONLY);
    if (fd >= 0) {
        auto ret = getFileSize(fd, size);
        close(fd);
        return ret;
    }
    return false;
}

#endif // !MMKV_APPLE

size_t getPageSize() {
    return static_cast<size_t>(getpagesize());
}

extern MMKVPath_t absolutePath(const MMKVPath_t &path) {
    fs::path relative_path(path);
    fs::path absolute_path = fs::absolute(relative_path);
    try {
        fs::path normalized = fs::weakly_canonical(absolute_path);
        return normalized.string();
    } catch (std::exception &e) {
        MMKVError("fail to weakly_canonical() path %s, error: %s", absolute_path.c_str(), e.what());
    }
    return absolute_path.string();
}

static pair<MMKVPath_t, int> createUniqueTempFile(const char *prefix) {
    char path[PATH_MAX];
#ifdef MMKV_ANDROID
    snprintf(path, PATH_MAX, "%s/%s.XXXXXX", g_android_tmpDir.c_str(), prefix);
#else
    const auto envTmpDir = getenv("TMPDIR");
    const auto tmpDir = (envTmpDir && envTmpDir[0] != '\0') ? envTmpDir : P_tmpdir;
    snprintf(path, PATH_MAX, "%s/%s.XXXXXX", tmpDir, prefix);
#endif

    auto fd = mkstemp(path);
    if (fd < 0) {
        MMKVError("fail to create unique temp file [%s], %d(%s)", path, errno, strerror(errno));
        return {"", fd};
    }
    MMKVDebug("create unique temp file [%s] with fd[%d]", path, fd);
    return {MMKVPath_t(path), fd};
}

#ifndef MMKV_APPLE

#if !defined(MMKV_ANDROID) && !defined(MMKV_LINUX)

bool tryAtomicRename(const MMKVPath_t &srcPath, const MMKVPath_t &dstPath) {
    if (::rename(srcPath.c_str(), dstPath.c_str()) != 0) {
        MMKVError("fail to rename [%s] to [%s], %d(%s)", srcPath.c_str(), dstPath.c_str(), errno, strerror(errno));
        return false;
    }
    return true;
}

bool copyFileContent(const MMKVPath_t &srcPath, MMKVFileHandle_t dstFD, bool needTruncate) {
    if (dstFD < 0) {
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
    lseek(dstFD, 0, SEEK_SET);

    // the POSIX standard don't have sendfile()/fcopyfile() equivalent, do it the hard way
    while (true) {
        auto sizeRead = read(srcFile.getFd(), buffer, bufferSize);
        if (sizeRead < 0) {
            MMKVError("fail to read file [%s], %d(%s)", srcPath.c_str(), errno, strerror(errno));
            goto errorOut;
        }

        size_t totalWrite = 0;
        do {
            auto sizeWrite = write(dstFD, buffer + totalWrite, sizeRead - totalWrite);
            if (sizeWrite < 0) {
                MMKVError("fail to write fd [%d], %d(%s)", dstFD, errno, strerror(errno));
                goto errorOut;
            }
            totalWrite += sizeWrite;
        } while (totalWrite < sizeRead);

        if (sizeRead < bufferSize) {
            break;
        }
    }
    if (needTruncate) {
        size_t dstFileSize = 0;
        getFileSize(dstFD, dstFileSize);
        auto srcFileSize = srcFile.getActualFileSize();
        if ((dstFileSize != srcFileSize) && (::ftruncate(dstFD, static_cast<off_t>(srcFileSize)) != 0)) {
            MMKVError("fail to truncate [%d] to size [%zu], %d(%s)", dstFD, srcFileSize, errno, strerror(errno));
            goto errorOut;
        }
    }

    ret = true;
    MMKVInfo("copy content from %s to fd[%d] finish", srcPath.c_str(), dstFD);

errorOut:
    free(buffer);
    return ret;
}

#endif // !defined(MMKV_ANDROID) && !defined(MMKV_LINUX)

// copy to a temp file then rename it
// this is the best we can do under the POSIX standard
bool copyFile(const MMKVPath_t &srcPath, const MMKVPath_t &dstPath) {
    auto pair = createUniqueTempFile("MMKV");
    auto tmpFD = pair.second;
    auto &tmpPath = pair.first;
    if (tmpFD < 0) {
        return false;
    }

    bool renamed = false;
    if (copyFileContent(srcPath, tmpFD, false)) {
        MMKVInfo("copyfile [%s] to [%s]", srcPath.c_str(), tmpPath.c_str());
        renamed = tryAtomicRename(tmpPath, dstPath);
        if (!renamed) {
            MMKVInfo("rename fail, try copy file content instead.");
            if (copyFileContent(tmpPath, dstPath)) {
                renamed = true;
                ::unlink(tmpPath.c_str());
            }
        }
        if (renamed) {
            MMKVInfo("copyfile [%s] to [%s] finish.", srcPath.c_str(), dstPath.c_str());
        }
    }

    ::close(tmpFD);
    if (!renamed) {
        ::unlink(tmpPath.c_str());
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
        MMKVError("fail to copyfile(): target file %s", dstPath.c_str());
    } else {
        MMKVInfo("copy content from %s to [%s] finish", srcPath.c_str(), dstPath.c_str());
    }
    return ret;
}

bool copyFileContent(const MMKVPath_t &srcPath, MMKVFileHandle_t dstFD) {
    return copyFileContent(srcPath, dstFD, true);
}

#endif // !defined(MMKV_APPLE)

void closeFileHandle(MMKVFileHandle_t handle) {
    if (handle != MMKVFileHandleInvalidValue) {
        ::close(handle);
    }
}

bool isSameFile(MMKVFileHandle_t left, MMKVFileHandle_t right) {
    if (left < 0 || right < 0) {
        return false;
    }
    struct stat leftInfo = {};
    struct stat rightInfo = {};
    return ::fstat(left, &leftInfo) == 0 && ::fstat(right, &rightInfo) == 0 &&
           leftInfo.st_dev == rightInfo.st_dev && leftInfo.st_ino == rightInfo.st_ino;
}

bool syncFile(MMKVFileHandle_t handle) {
    if (handle < 0) {
        return false;
    }
    while (::fsync(handle) != 0) {
        if (errno != EINTR) {
            return false;
        }
    }
    return true;
}

static bool isDirectChildName(const MMKVPath_t &fileName) {
    return !fileName.empty() && fileName != "." && fileName != ".." && fileName.find('/') == MMKVPath_t::npos &&
           fileName.find('\0') == MMKVPath_t::npos;
}

static int directoryOpenFlags() {
    int flags = O_RDONLY | O_CLOEXEC;
#ifdef O_DIRECTORY
    flags |= O_DIRECTORY;
#endif
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    return flags;
}

static bool containsParentDirectoryComponent(const MMKVPath_t &path) {
    size_t position = 0;
    while (position < path.size()) {
        while (position < path.size() && path[position] == '/') {
            position++;
        }
        auto end = path.find('/', position);
        auto length = (end == MMKVPath_t::npos ? path.size() : end) - position;
        if (length == 2 && path[position] == '.' && path[position + 1] == '.') {
            return true;
        }
        position = end == MMKVPath_t::npos ? path.size() : end + 1;
    }
    return false;
}

static MMKVFileHandle_t openDirectoryPath(const MMKVPath_t &dirPath, bool create) {
#if !defined(O_DIRECTORY) || !defined(O_NOFOLLOW)
    (void) dirPath;
    (void) create;
    errno = ENOTSUP;
    return MMKVFileHandleInvalidValue;
#else
    if (dirPath.empty() || dirPath.find('\0') != MMKVPath_t::npos || containsParentDirectoryComponent(dirPath)) {
        errno = EINVAL;
        return MMKVFileHandleInvalidValue;
    }

    const bool absolute = dirPath.front() == '/';
    auto currentFD = ::open(absolute ? "/" : ".", directoryOpenFlags());
    if (currentFD < 0) {
        return MMKVFileHandleInvalidValue;
    }

    size_t position = absolute ? 1 : 0;
    while (position < dirPath.size()) {
        while (position < dirPath.size() && dirPath[position] == '/') {
            position++;
        }
        if (position == dirPath.size()) {
            break;
        }
        auto end = dirPath.find('/', position);
        auto component = dirPath.substr(position, end == MMKVPath_t::npos ? MMKVPath_t::npos : end - position);
        position = end == MMKVPath_t::npos ? dirPath.size() : end + 1;
        if (component == ".") {
            continue;
        }
        // Refuse lexical parent traversal so every accepted component has
        // been opened and verified beneath the preceding directory handle.
        if (component == "..") {
            ::close(currentFD);
            errno = EPERM;
            return MMKVFileHandleInvalidValue;
        }

        auto nextFD = ::openat(currentFD, component.c_str(), directoryOpenFlags());
        if (nextFD < 0 && create && errno == ENOENT) {
            if (::mkdirat(currentFD, component.c_str(), 0777) != 0 && errno != EEXIST) {
                auto createError = errno;
                ::close(currentFD);
                errno = createError;
                return MMKVFileHandleInvalidValue;
            }
            nextFD = ::openat(currentFD, component.c_str(), directoryOpenFlags());
        }
        auto openError = errno;
        ::close(currentFD);
        if (nextFD < 0) {
            errno = openError;
            return MMKVFileHandleInvalidValue;
        }
        currentFD = nextFD;
    }

    struct stat dirInfo = {};
    if (::fstat(currentFD, &dirInfo) != 0 || !S_ISDIR(dirInfo.st_mode)) {
        ::close(currentFD);
        errno = ENOTDIR;
        return MMKVFileHandleInvalidValue;
    }
    return currentFD;
#endif
}

MMKVFileHandle_t openDirectoryHandle(const MMKVPath_t &dirPath) {
    return openDirectoryPath(dirPath, false);
}

MMKVFileHandle_t openOrCreateDirectoryHandle(const MMKVPath_t &dirPath) {
    return openDirectoryPath(dirPath, true);
}

MMKVFileHandle_t openDirectoryInDir(MMKVFileHandle_t dirFD,
                                    const MMKVPath_t &dirPath,
                                    const MMKVPath_t &childName,
                                    bool create) {
    (void) dirPath;
#if !defined(O_DIRECTORY) || !defined(O_NOFOLLOW)
    (void) dirFD;
    (void) childName;
    (void) create;
    errno = ENOTSUP;
    return MMKVFileHandleInvalidValue;
#else
    if (dirFD < 0 || !isDirectChildName(childName)) {
        errno = EINVAL;
        return MMKVFileHandleInvalidValue;
    }
    struct stat parentInfo = {};
    if (::fstat(dirFD, &parentInfo) != 0 || !S_ISDIR(parentInfo.st_mode)) {
        errno = ENOTDIR;
        return MMKVFileHandleInvalidValue;
    }

    auto childFD = ::openat(dirFD, childName.c_str(), directoryOpenFlags());
    if (childFD < 0 && create && errno == ENOENT) {
        if (::mkdirat(dirFD, childName.c_str(), 0777) != 0 && errno != EEXIST) {
            return MMKVFileHandleInvalidValue;
        }
        childFD = ::openat(dirFD, childName.c_str(), directoryOpenFlags());
    }
    if (childFD < 0) {
        return MMKVFileHandleInvalidValue;
    }
    struct stat childInfo = {};
    if (::fstat(childFD, &childInfo) != 0 || !S_ISDIR(childInfo.st_mode)) {
        auto openError = errno;
        ::close(childFD);
        errno = openError != 0 ? openError : ENOTDIR;
        return MMKVFileHandleInvalidValue;
    }
    return childFD;
#endif
}

enum class ChildOpenMode { ReadOnly, ReadWriteExisting, ReadWriteCreate, ReadWriteExclusive };

static MMKVFileHandle_t openRegularChild(MMKVFileHandle_t dirFD,
                                         const MMKVPath_t &dirPath,
                                         const MMKVPath_t &fileName,
                                         ChildOpenMode mode) {
    if (dirFD < 0 || !isDirectChildName(fileName)) {
        return MMKVFileHandleInvalidValue;
    }

#ifndef O_NOFOLLOW
    (void) dirPath;
    (void) mode;
    errno = ENOTSUP;
    return MMKVFileHandleInvalidValue;
#else
    struct stat dirInfo = {};
    if (::fstat(dirFD, &dirInfo) != 0 || !S_ISDIR(dirInfo.st_mode)) {
        errno = ENOTDIR;
        return MMKVFileHandleInvalidValue;
    }

    int fileFlags = O_CLOEXEC | O_NONBLOCK | O_NOFOLLOW;
    switch (mode) {
        case ChildOpenMode::ReadOnly:
            fileFlags |= O_RDONLY;
            break;
        case ChildOpenMode::ReadWriteExisting:
            fileFlags |= O_RDWR;
            break;
        case ChildOpenMode::ReadWriteCreate:
            fileFlags |= O_RDWR | O_CREAT;
            break;
        case ChildOpenMode::ReadWriteExclusive:
            fileFlags |= O_RDWR | O_CREAT | O_EXCL;
            break;
    }
    auto fileFD = (mode == ChildOpenMode::ReadWriteCreate || mode == ChildOpenMode::ReadWriteExclusive)
                      ? ::openat(dirFD, fileName.c_str(), fileFlags, S_IRUSR | S_IWUSR)
                      : ::openat(dirFD, fileName.c_str(), fileFlags);
    if (fileFD < 0) {
        auto openError = errno;
        MMKVDebug("skip unsafe or unreadable directory entry [%s/%s], %d(%s)", dirPath.c_str(), fileName.c_str(),
                  openError, strerror(openError));
        errno = openError;
        return MMKVFileHandleInvalidValue;
    }

    struct stat fileInfo = {};
    if (::fstat(fileFD, &fileInfo) != 0 || !S_ISREG(fileInfo.st_mode) || fileInfo.st_nlink != 1) {
        MMKVDebug("skip non-regular directory entry [%s/%s]", dirPath.c_str(), fileName.c_str());
        ::close(fileFD);
        return MMKVFileHandleInvalidValue;
    }
    return fileFD;
#endif
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
        firstFD = MMKVFileHandleInvalidValue;
        secondFD = MMKVFileHandleInvalidValue;
        return false;
    }
    return true;
}

MMKVFileHandle_t openRegularFileInDir(const MMKVPath_t &dirPath, const MMKVPath_t &fileName) {
    auto dirFD = openDirectoryHandle(dirPath);
    if (dirFD == MMKVFileHandleInvalidValue) {
        MMKVError("fail to open directory [%s], %d(%s)", dirPath.c_str(), errno, strerror(errno));
        return MMKVFileHandleInvalidValue;
    }
    auto fileFD = openRegularFileInDir(dirFD, dirPath, fileName);
    closeFileHandle(dirFD);
    return fileFD;
}

bool readFileContent(MMKVFileHandle_t srcFD, void *buffer, size_t size) {
    if (srcFD < 0 || (!buffer && size > 0)) {
        return false;
    }
    if (size == 0) {
        return true;
    }
    auto temporary = static_cast<uint8_t *>(malloc(size));
    if (!temporary) {
        return false;
    }
    bool ret = false;
    size_t offset = 0;
    while (offset < size) {
        auto count = ::pread(srcFD, temporary + offset, size - offset, static_cast<off_t>(offset));
        if (count > 0) {
            offset += static_cast<size_t>(count);
        } else if (count < 0 && errno == EINTR) {
            continue;
        } else {
            goto exit;
        }
    }
    memcpy(buffer, temporary, size);
    ret = true;

exit:
    free(temporary);
    return ret;
}

bool copyFileContent(MMKVFileHandle_t srcFD, MMKVFileHandle_t dstFD, bool needTruncate) {
    if (srcFD < 0 || dstFD < 0 || ::lseek(dstFD, 0, SEEK_SET) < 0) {
        return false;
    }

    const auto bufferSize = getPageSize();
    auto buffer = static_cast<uint8_t *>(malloc(bufferSize));
    if (!buffer) {
        MMKVError("fail to malloc size %zu, %d(%s)", bufferSize, errno, strerror(errno));
        return false;
    }

    bool ret = false;
    size_t readOffset = 0;
    while (true) {
        if (static_cast<uintmax_t>(readOffset) > static_cast<uintmax_t>(numeric_limits<off_t>::max())) {
            errno = EOVERFLOW;
            MMKVError("source offset exceeds off_t for fd [%d]", srcFD);
            goto exit;
        }
        auto sizeRead = ::pread(srcFD, buffer, bufferSize, static_cast<off_t>(readOffset));
        if (sizeRead == 0) {
            break;
        }
        if (sizeRead < 0) {
            if (errno == EINTR) {
                continue;
            }
            MMKVError("fail to read fd [%d], %d(%s)", srcFD, errno, strerror(errno));
            goto exit;
        }

        auto chunkSize = static_cast<size_t>(sizeRead);
        if (chunkSize > numeric_limits<size_t>::max() - readOffset ||
            static_cast<uintmax_t>(chunkSize) >
                static_cast<uintmax_t>(numeric_limits<off_t>::max()) - static_cast<uintmax_t>(readOffset)) {
            errno = EOVERFLOW;
            MMKVError("source size exceeds supported offset for fd [%d]", srcFD);
            goto exit;
        }
        size_t totalWrite = 0;
        while (totalWrite < chunkSize) {
            auto sizeWrite = ::write(dstFD, buffer + totalWrite, chunkSize - totalWrite);
            if (sizeWrite > 0) {
                totalWrite += static_cast<size_t>(sizeWrite);
            } else if (sizeWrite < 0 && errno == EINTR) {
                continue;
            } else {
                MMKVError("fail to write fd [%d], %d(%s)", dstFD, errno, strerror(errno));
                goto exit;
            }
        }
        readOffset += chunkSize;
    }

    if (needTruncate && ::ftruncate(dstFD, static_cast<off_t>(readOffset)) != 0) {
        MMKVError("fail to truncate fd [%d] to size [%zu], %d(%s)", dstFD, readOffset, errno, strerror(errno));
        goto exit;
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
    auto separator = path.rfind('/');
    if (separator == MMKVPath_t::npos) {
        dirPath = ".";
        fileName = path;
    } else {
        dirPath = separator == 0 ? "/" : path.substr(0, separator);
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

static pair<MMKVPath_t, MMKVFileHandle_t> createUniqueTempFileInDir(MMKVFileHandle_t dirFD) {
    static atomic<uint64_t> counter{0};
    uint64_t nonce = 0;
    try {
        random_device random;
        nonce = (static_cast<uint64_t>(random()) << 32U) ^ static_cast<uint64_t>(random());
    } catch (...) {
        return {"", MMKVFileHandleInvalidValue};
    }
    for (size_t attempt = 0; attempt < 128; attempt++) {
        char name[128];
        auto serial = counter.fetch_add(1, memory_order_relaxed);
        snprintf(name, sizeof(name), ".mmkv.tmp.%ld.%016llx.%llu", static_cast<long>(getpid()),
                 static_cast<unsigned long long>(nonce),
                 static_cast<unsigned long long>(serial));
        auto tmpFD = ::openat(dirFD, name, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
        if (tmpFD >= 0) {
            return {name, tmpFD};
        }
        if (errno != EEXIST) {
            break;
        }
    }
    return {"", MMKVFileHandleInvalidValue};
}

MMKVFileHandle_t createTemporaryFileInDir(MMKVFileHandle_t dirFD, const MMKVPath_t &dirPath) {
    (void) dirPath;
    if (dirFD < 0) {
        return MMKVFileHandleInvalidValue;
    }
    auto pair = createUniqueTempFileInDir(dirFD);
    auto &tmpName = pair.first;
    auto tmpFD = pair.second;
    if (tmpFD == MMKVFileHandleInvalidValue) {
        return MMKVFileHandleInvalidValue;
    }
    if (::unlinkat(dirFD, tmpName.c_str(), 0) != 0) {
        auto unlinkError = errno;
        closeFileHandle(tmpFD);
        ::unlinkat(dirFD, tmpName.c_str(), 0);
        errno = unlinkError;
        return MMKVFileHandleInvalidValue;
    }
    return tmpFD;
}

static bool copyFileRetainingDestination(MMKVFileHandle_t srcFD,
                                         MMKVFileHandle_t dstDirFD,
                                         const MMKVPath_t &dstDirPath,
                                         const MMKVPath_t &dstFileName,
                                         bool destinationWasMissing,
                                         MMKVFileHandle_t *retainedDstFD) {
    if (retainedDstFD) {
        *retainedDstFD = MMKVFileHandleInvalidValue;
    }
    if (srcFD < 0 || dstDirFD < 0 || !isDirectChildName(dstFileName)) {
        return false;
    }
    auto pair = createUniqueTempFileInDir(dstDirFD);
    auto &tmpName = pair.first;
    auto tmpFD = pair.second;
    if (tmpFD == MMKVFileHandleInvalidValue) {
        return false;
    }

    auto copied = copyFileContent(srcFD, tmpFD, false);
    bool published = false;
    if (copied) {
        if (destinationWasMissing) {
            // linkat() is an atomic no-replace publish within this directory.
            // A newcomer after the Missing inspection wins instead of being
            // silently overwritten by renameat().
            published = ::linkat(dstDirFD, tmpName.c_str(), dstDirFD, dstFileName.c_str(), 0) == 0;
            if (published) {
                ::unlinkat(dstDirFD, tmpName.c_str(), 0);
            }
        } else {
            published = ::renameat(dstDirFD, tmpName.c_str(), dstDirFD, dstFileName.c_str()) == 0;
        }
    }
    bool ret = published;
    if (published && retainedDstFD) {
        // tmpFD stays attached to the inode now published at the destination
        // name. Retaining it closes the name-to-identity race for rollback of
        // an entry that was initially absent.
        *retainedDstFD = tmpFD;
        tmpFD = MMKVFileHandleInvalidValue;
    } else if (!published && copied) {
        MMKVInfo("atomic publish fail, try copy file content instead.");
        auto dstFD = destinationWasMissing
                         ? openRegularChild(dstDirFD, dstDirPath, dstFileName, ChildOpenMode::ReadWriteExclusive)
                         : openOrCreateRegularFileInDir(dstDirFD, dstDirPath, dstFileName);
        if (dstFD != MMKVFileHandleInvalidValue) {
            ret = copyFileContent(tmpFD, dstFD);
            if (retainedDstFD) {
                *retainedDstFD = dstFD;
            } else {
                closeFileHandle(dstFD);
            }
        }
    }
    closeFileHandle(tmpFD);
    if (!published || destinationWasMissing) {
        ::unlinkat(dstDirFD, tmpName.c_str(), 0);
    }
    return ret;
}

bool copyFile(MMKVFileHandle_t srcFD,
              MMKVFileHandle_t dstDirFD,
              const MMKVPath_t &dstDirPath,
              const MMKVPath_t &dstFileName) {
    return copyFileRetainingDestination(srcFD, dstDirFD, dstDirPath, dstFileName, false, nullptr);
}

enum class RegularChildState { Missing, Regular, Unsafe, Error };

static RegularChildState inspectRegularChild(MMKVFileHandle_t dirFD, const MMKVPath_t &fileName) {
    struct stat info = {};
    if (::fstatat(dirFD, fileName.c_str(), &info, AT_SYMLINK_NOFOLLOW) == 0) {
        return S_ISREG(info.st_mode) ? RegularChildState::Regular : RegularChildState::Unsafe;
    }
    return errno == ENOENT ? RegularChildState::Missing : RegularChildState::Error;
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
    if (dirFD < 0 || firstName == secondName || !isDirectChildName(firstName) ||
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
    // POSIX has no portable unlink-by-handle operation. An fstatat()+unlinkat()
    // identity check would still have a race that could delete a concurrent
    // replacement, so leave any entry we created rather than unlinking by name.
    if (firstCreated) {
        MMKVWarning("leaving newly created file after pair-open failure [%s/%s]", dirPath.c_str(),
                    firstName.c_str());
    }
    if (secondCreated) {
        MMKVWarning("leaving newly created file after pair-open failure [%s/%s]", dirPath.c_str(),
                    secondName.c_str());
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
    if (firstSrcFD < 0 || secondSrcFD < 0 || dstDirFD < 0 || firstDstName == secondDstName ||
        !isDirectChildName(firstDstName) || !isDirectChildName(secondDstName)) {
        return false;
    }

    auto stagedFirst = createTemporaryFileInDir(dstDirFD, dstDirPath);
    auto stagedSecond = createTemporaryFileInDir(dstDirFD, dstDirPath);
    if (stagedFirst < 0 || stagedSecond < 0 || !copyFileContent(firstSrcFD, stagedFirst) ||
        !copyFileContent(secondSrcFD, stagedSecond)) {
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
    if ((firstState == RegularChildState::Regular && oldFirst < 0) ||
        (secondState == RegularChildState::Regular && oldSecond < 0)) {
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
         (savedFirst < 0 || !copyFileContent(oldFirst, savedFirst))) ||
        (secondState == RegularChildState::Regular &&
         (savedSecond < 0 || !copyFileContent(oldSecond, savedSecond)))) {
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

    auto committedFirst = MMKVFileHandleInvalidValue;
    auto committedSecond = MMKVFileHandleInvalidValue;
    auto firstCommitted = copyFileRetainingDestination(stagedFirst, dstDirFD, dstDirPath, firstDstName,
                                                       firstState == RegularChildState::Missing, &committedFirst);
    auto secondAttempted = firstCommitted;
    auto secondCommitted =
        secondAttempted &&
        copyFileRetainingDestination(stagedSecond, dstDirFD, dstDirPath, secondDstName,
                                     secondState == RegularChildState::Missing, &committedSecond);
    if (!secondCommitted) {
        auto firstRestored = firstState == RegularChildState::Regular
                                 ? (committedFirst == MMKVFileHandleInvalidValue ||
                                    copyFileContent(savedFirst, committedFirst))
                                 : committedFirst == MMKVFileHandleInvalidValue;
        auto secondRestored = !secondAttempted ||
                              (secondState == RegularChildState::Regular
                                   ? (committedSecond == MMKVFileHandleInvalidValue ||
                                      copyFileContent(savedSecond, committedSecond))
                                   : committedSecond == MMKVFileHandleInvalidValue);
        if (!firstRestored || !secondRestored) {
            MMKVWarning("failed transaction may leave a newly created destination in [%s]", dstDirPath.c_str());
        }
    }

    closeFileHandle(committedFirst);
    closeFileHandle(committedSecond);
    closeFileHandle(savedFirst);
    closeFileHandle(savedSecond);
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
    if (firstSrcFD < 0 || secondSrcFD < 0 || firstDstFD < 0 || secondDstFD < 0 || tempDirFD < 0) {
        return false;
    }

    auto stagedFirst = createTemporaryFileInDir(tempDirFD, tempDirPath);
    auto stagedSecond = createTemporaryFileInDir(tempDirFD, tempDirPath);
    auto savedFirst = createTemporaryFileInDir(tempDirFD, tempDirPath);
    auto savedSecond = createTemporaryFileInDir(tempDirFD, tempDirPath);
    if (stagedFirst < 0 || stagedSecond < 0 || savedFirst < 0 || savedSecond < 0 ||
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
    if (!ret && (firstCreated || secondCreated)) {
        // Never perform a check-then-unlink by name: a concurrent replacement
        // could be deleted between those operations. The content rollback above
        // is handle-bound; newly created names may therefore remain on failure.
        MMKVWarning("failed transaction left newly created destination entries in [%s]", tempDirPath.c_str());
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

bool walkInOpenedDir(MMKVFileHandle_t dirFD,
                     const MMKVPath_t &dirPath,
                     WalkType type,
                     const function<void(const MMKVPath_t &, WalkType)> &walker) {
    if (dirFD < 0) {
        return false;
    }
    struct stat dirInfo = {};
    if (::fstat(dirFD, &dirInfo) != 0 || !S_ISDIR(dirInfo.st_mode)) {
        return false;
    }

    // openat(".") creates an independent directory stream while retaining
    // the identity pinned by dirFD. fdopendir() owns only this scan handle.
    auto scanFD = ::openat(dirFD, ".", directoryOpenFlags());
    DIR *dir = (scanFD >= 0) ? fdopendir(scanFD) : nullptr;
    if (!dir) {
        MMKVError("opendir failed: %d(%s), %s", errno, strerror(errno), dirPath.c_str());
        if (scanFD >= 0) {
            ::close(scanFD);
        }
        return false;
    }

    vector<pair<MMKVPath_t, WalkType>> children;
    int readError = 0;
    while (true) {
        errno = 0;
        auto child = readdir(dir);
        if (!child) {
            readError = errno;
            break;
        }
        if (strcmp(child->d_name, ".") == 0 || strcmp(child->d_name, "..") == 0) {
            continue;
        }

        // d_type is an advisory hint and can be DT_UNKNOWN or stale. Always
        // classify relative to the pinned directory without following links.
        struct stat childInfo = {};
        if (::fstatat(dirFD, child->d_name, &childInfo, AT_SYMLINK_NOFOLLOW) != 0) {
            continue;
        }
        if (S_ISREG(childInfo.st_mode) && (type & WalkFile)) {
            children.emplace_back(child->d_name, WalkFile);
        } else if (S_ISDIR(childInfo.st_mode) && (type & WalkFolder)) {
            children.emplace_back(child->d_name, WalkFolder);
        }
    }

    auto closeResult = closedir(dir);
    if (readError != 0 || closeResult != 0) {
        return false;
    }
    for (const auto &child : children) {
        walker(child.first, child.second);
    }
    return true;
}

void walkInDir(const MMKVPath_t &dirPath, WalkType type, const function<void(const MMKVPath_t&, WalkType)> &walker) {
    auto dirFD = openDirectoryHandle(dirPath);
    if (dirFD == MMKVFileHandleInvalidValue) {
        MMKVError("opendir failed: %d(%s), %s", errno, strerror(errno), dirPath.c_str());
        return;
    }
    MMKVPath_t childPrefix = dirPath;
    if (childPrefix.empty() || childPrefix.back() != '/') {
        childPrefix.push_back('/');
    }
    walkInOpenedDir(dirFD, dirPath, type, [&](const MMKVPath_t &fileName, WalkType childType) {
        walker(childPrefix + fileName, childType);
    });
    closeFileHandle(dirFD);
}

bool deleteFile(const MMKVPath_t &path) {
    auto filename = path.c_str();
    if (::unlink(filename) != 0) {
        auto err = errno;
        MMKVError("fail to delete file [%s], %d (%s)", filename, err, strerror(err));
        return false;
    }
    return true;
}

#ifndef MMKV_APPLE
bool isDiskOfMMAPFileCorrupted(MemoryFile *file, bool &needReportReadFail) {
    // TODO: maybe we need reading a larger chunk than 4 byte in Android/Linux
    uint32_t info;
    auto fd = file->getFd();
    auto path = file->getPath().c_str();

    auto oldPos = lseek(fd, 0, SEEK_CUR);
    lseek(fd, 0, SEEK_SET);
    auto size = read(fd, &info, sizeof(info));
    auto err = errno;
    lseek(fd, oldPos, SEEK_SET);

    if (size <= 0) {
        needReportReadFail = true;
        MMKVError("fail to read [%s] from fd [%d], errno: %d (%s)", path, fd, err, strerror(err));
        if (err == EIO || err == EILSEQ || err == EINVAL || err == ENXIO) {
            MMKVWarning("file fail to read, consider it illegal, delete now: [%s]", path);
            return true;
        }
    }
    file->cleanMayflyFD();
    return false;
}
#endif

std::optional<MMKVPath_t> getUniqueFileName(const MMKVPath_t &folder, const MMKVPath_t &prefix) {
    fs::path folderPath(folder);
    fs::path prefixPath(prefix);

    // Ensure the directory exists
    std::error_code ec;
    if (!fs::exists(folderPath, ec)) {
        // Attempt to create it or fail if preferred.
        // GetTempFileName fails if dir doesn't exist, so we adhere to that.
        return std::nullopt;
    }

    // Behavior: Generate random unique filename, CREATE the file to reserve it.
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    constexpr int maxAttempts = 64;
    for (int i = 0; i < maxAttempts; ++i) {
        uint64_t randomVal = dis(gen);
        MMKVPath_t suffix = to_string(randomVal);
        MMKVPath_t fileName = prefix + "." + suffix + ".tmp";
        fs::path candidatePath = folderPath / fileName;

        // Atomic check and create logic "mimic"
        // std::filesystem::exists is not atomic, but standard C++17 <fstream> doesn't
        // support O_EXCL (exclusive create) easily without platform headers.
        // We check existence first to avoid clobbering existing files.
        if (fs::exists(candidatePath, ec)) {
            continue; // Collision found, try next
        }

        // Try to create the file to "reserve" it
        File file(candidatePath.native(), OpenFlag::ReadWrite | OpenFlag::Create);
        if (file.isFileValid()) {
            return candidatePath.native();
        }
    }

    // Failed to find unique name after max attempts
    return std::nullopt;
}
} // namespace mmkv

#endif // !defined(MMKV_WIN32)
