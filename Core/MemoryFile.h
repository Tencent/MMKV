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

#ifndef MMKV_MAMERYFILE_H
#define MMKV_MAMERYFILE_H
#ifdef __cplusplus

#include "MMKVPredef.h"
#include <array>
#include <cstdint>
#include <functional>
#include <optional>

#ifdef MMKV_ANDROID
MMKVPath_t ashmemMMKVPathWithID(const MMKVPath_t &mmapID);

long long getFileModifyTimeInMS(const char *path);

namespace mmkv {
extern int g_android_api;
extern std::string g_android_tmpDir;

enum FileType : bool { MMFILE_TYPE_FILE = false, MMFILE_TYPE_ASHMEM = true };
} // namespace mmkv
#endif // MMKV_ANDROID

namespace mmkv {

enum class OpenFlag : uint32_t {
    ReadOnly = 1 << 0,
    WriteOnly = 1 << 1,
    ReadWrite = ReadOnly | WriteOnly,
    Create = 1 << 2,
    Excel = 1 << 3, // fail if Create is set but the file already exist
    Truncate = 1 << 4,
};
constexpr uint32_t OpenFlagRWMask = 0x3; // mask for Read Write mode

static inline OpenFlag operator | (OpenFlag left, OpenFlag right) {
    return static_cast<OpenFlag>(static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}

static inline bool operator & (OpenFlag left, OpenFlag right) {
    return ((static_cast<uint32_t>(left) & static_cast<uint32_t>(right)) != 0);
}

static inline OpenFlag operator & (OpenFlag left, uint32_t right) {
    return static_cast<OpenFlag>(static_cast<uint32_t>(left) & right);
}

template <typename T>
T roundUp(T numToRound, T multiple) {
    return ((numToRound + multiple - 1) / multiple) * multiple;
}

class FileLock;

class File {
    MMKVPath_t m_path;
#ifdef MMKV_WIN32
    std::string m_utf8Path;
#endif
    MMKVFileHandle_t m_fd;

public:
    const OpenFlag m_flag;
#ifndef MMKV_ANDROID
    explicit File(MMKVPath_t path, OpenFlag flag);
#else
    File(MMKVPath_t path, OpenFlag flag, size_t size = 0, FileType fileType = MMFILE_TYPE_FILE);
    explicit File(MMKVFileHandle_t ashmemFD);

    size_t m_size;
    const FileType m_fileType;
#endif // MMKV_ANDROID

    ~File() { close(); }

    // existingOnly suppresses create/truncate semantics. It is used when a
    // live mapping needs a temporary descriptor for its original file.
    bool open(bool existingOnly = false);

    void close();

    MMKVFileHandle_t getFd() const { return m_fd; }

    const MMKVPath_t &getPath() const { return m_path; }

#ifndef MMKV_WIN32
    bool isFileValid() const { return m_fd >= 0; }

    const std::string &getUTF8Path() const { return m_path; }
#else
    bool isFileValid() const { return m_fd != MMKVFileHandleInvalidValue; }

    const std::string &getUTF8Path() const { return m_utf8Path; }
#endif

    // get the actual file size on disk
    size_t getActualFileSize() const;

    // just forbid it for possibly misuse
    explicit File(const File &other) = delete;
    File &operator=(const File &other) = delete;

    friend class MemoryFile;
};

class MemoryFile {
    File m_diskFile;
#ifdef MMKV_WIN32
    HANDLE m_fileMapping;
#endif
    void *m_ptr;
    size_t m_size;
    const bool m_readOnly;
    const bool m_isMayflyFD;
    std::array<uint64_t, 2> m_mappedFileIdentity{};
    bool m_hasMappedFileIdentity = false;
    std::array<uint64_t, 2> m_expectedFileIdentity{};
    bool m_hasExpectedFileIdentity = false;
    bool m_keepFileHandleForReload = false;

    bool mmapOrCleanup(FileLock *fileLock);

    bool captureMappedFileIdentity();

    void clearMappedFileIdentity();

    void doCleanMemoryCache(bool forceClean);

    bool openIfNeeded();

public:
#ifndef MMKV_ANDROID
    explicit MemoryFile(MMKVPath_t path, size_t expectedCapacity = 0, bool readOnly = false, bool mayflyFD = false);
#else
    MemoryFile(MMKVPath_t path, FileType fileType, size_t expectedCapacity = 0, bool readOnly = false, bool mayflyFD = false);
    explicit MemoryFile(MMKVFileHandle_t ashmemFD);

    const FileType m_fileType;
#endif // MMKV_ANDROID

    ~MemoryFile() { doCleanMemoryCache(true); }

    size_t getFileSize() const { return m_size; }

    // get the actual file size on disk
    size_t getActualFileSize();

    void *getMemory() { return m_ptr; }

    const MMKVPath_t &getPath() { return m_diskFile.getPath(); }

    const std::string &getUTF8Path() const { return m_diskFile.getUTF8Path(); }

    MMKVFileHandle_t getFd();

    // Compare a pinned handle with the exact file used to create the live or
    // most recently lazy-cleared mapping. This never reopens getPath(), which
    // is important after a path or one of its parents is renamed or replaced.
    bool isMappedFile(MMKVFileHandle_t fileHandle) const;

    // forceClean releases a handle retained by reloadFromFileHandle() after
    // the caller's complete load/validation sequence has finished.
    void cleanMayflyFD(bool forceClean = false);

    // the newly expanded file content will be zeroed
    bool truncate(size_t size, FileLock *fileLock = nullptr);

    bool msync(SyncFlag syncFlag);

    // call this if clearMemoryCache() has been called
    void reloadFromFile(size_t expectedCapacity = 0);

    // Remap from a duplicate of an already pinned handle instead of resolving
    // getPath(). A successful mayfly reload retains a duplicate through the
    // caller's immediate load/size check; call cleanMayflyFD() afterward. On
    // failure, retain the intended identity (and a duplicate when possible)
    // so a retry cannot silently switch to a replacement at the old path.
    bool reloadFromFileHandle(MMKVFileHandle_t fileHandle, size_t expectedCapacity = 0);

    // resetFileIdentity is reserved for intentional delete/recreate recovery.
    // Ordinary lazy clears retain the last mapped identity and fail closed if
    // the path is replaced before the next load.
    void clearMemoryCache(bool resetFileIdentity = false) { doCleanMemoryCache(resetFileIdentity); }

#ifndef MMKV_WIN32
    bool isFileValid() { return (m_isMayflyFD || m_diskFile.isFileValid()) && m_size > 0 && m_ptr; }
#else
    bool isFileValid() { return (m_isMayflyFD || (m_diskFile.isFileValid() && m_fileMapping)) && m_size > 0 && m_ptr; }
#endif

    // just forbid it for possibly misuse
    explicit MemoryFile(const MemoryFile &other) = delete;
    MemoryFile &operator=(const MemoryFile &other) = delete;
};

class MMBuffer;

extern bool mkPath(const MMKVPath_t &path);
extern bool isFileExist(const MMKVPath_t &nsFilePath);
extern MMBuffer *readWholeFile(const MMKVPath_t &path);
extern bool zeroFillFile(MMKVFileHandle_t fd, size_t startPos, size_t size);
extern size_t getPageSize();
extern MMKVPath_t absolutePath(const MMKVPath_t &path);
#ifndef MMKV_WIN32
extern bool getFileSize(int fd, size_t &size);
#endif
extern bool tryAtomicRename(const MMKVPath_t &srcPath, const MMKVPath_t &dstPath);

// Open a directory without following symlinks/reparse points. Every path
// component is opened relative to the previously verified component.
// The caller owns the returned handle and must close it with closeFileHandle().
extern MMKVFileHandle_t openDirectoryHandle(const MMKVPath_t &dirPath);

// As above, but creates missing directory components relative to the pinned
// parent. Existing symlinks/reparse points and lexical parent traversal are
// rejected before any child is created through them.
extern MMKVFileHandle_t openOrCreateDirectoryHandle(const MMKVPath_t &dirPath);

// Open one direct child directory relative to an already pinned parent.
// dirPath identifies dirFD for Windows identity revalidation; POSIX uses
// dirFD directly. Existing symlinks/reparse points are never followed.
extern MMKVFileHandle_t openDirectoryInDir(MMKVFileHandle_t dirFD,
                                           const MMKVPath_t &dirPath,
                                           const MMKVPath_t &childName,
                                           bool create);

// Open one direct child of a pinned directory without following
// symlinks/reparse points. dirPath identifies dirFD for Windows identity
// revalidation; POSIX uses dirFD directly. The paired form guarantees both
// children are opened from the same pinned parent and closes both on failure.
extern MMKVFileHandle_t openRegularFileInDir(MMKVFileHandle_t dirFD,
                                             const MMKVPath_t &dirPath,
                                             const MMKVPath_t &fileName);
extern bool openRegularFilePairInDir(MMKVFileHandle_t dirFD,
                                     const MMKVPath_t &dirPath,
                                     const MMKVPath_t &firstName,
                                     const MMKVPath_t &secondName,
                                     MMKVFileHandle_t &firstFD,
                                     MMKVFileHandle_t &secondFD,
                                     bool writable = false);
extern bool openOrCreateRegularFilePairInDir(MMKVFileHandle_t dirFD,
                                             const MMKVPath_t &dirPath,
                                             const MMKVPath_t &firstName,
                                             const MMKVPath_t &secondName,
                                             MMKVFileHandle_t &firstFD,
                                             MMKVFileHandle_t &secondFD,
                                             bool &firstCreated,
                                             bool &secondCreated);

// Open an existing regular child for read/write, or create it if missing.
// Existing symlinks/reparse points and all non-regular entries are rejected.
// No truncation happens until the returned descriptor is explicitly copied to.
extern MMKVFileHandle_t openOrCreateRegularFileInDir(MMKVFileHandle_t dirFD,
                                                     const MMKVPath_t &dirPath,
                                                     const MMKVPath_t &fileName);

// Convenience path form. It pins and verifies dirPath internally.
extern MMKVFileHandle_t openRegularFileInDir(const MMKVPath_t &dirPath, const MMKVPath_t &fileName);
extern void closeFileHandle(MMKVFileHandle_t handle);
extern bool isSameFile(MMKVFileHandle_t left, MMKVFileHandle_t right);
extern bool syncFile(MMKVFileHandle_t handle);
// The destination buffer is left unchanged unless the full read succeeds.
extern bool readFileContent(MMKVFileHandle_t srcFD, void *buffer, size_t size);

// copy file by potentially renaming target file, might change file inode
extern bool copyFile(const MMKVPath_t &srcPath, const MMKVPath_t &dstPath);
extern bool copyFile(MMKVFileHandle_t srcFD, const MMKVPath_t &dstPath);
extern bool copyFile(MMKVFileHandle_t srcFD,
                     MMKVFileHandle_t dstDirFD,
                     const MMKVPath_t &dstDirPath,
                     const MMKVPath_t &dstFileName);

// Stage both source files before replacing either destination. If a
// pre-existing regular destination pair is present, best-effort rollback
// restores both files when either replacement fails.
extern bool copyFilePair(MMKVFileHandle_t firstSrcFD,
                         MMKVFileHandle_t secondSrcFD,
                         MMKVFileHandle_t dstDirFD,
                         const MMKVPath_t &dstDirPath,
                         const MMKVPath_t &firstDstName,
                         const MMKVPath_t &secondDstName);

// copy file by source file content, keep file inode the same
extern bool copyFileContent(const MMKVPath_t &srcPath, const MMKVPath_t &dstPath);
extern bool copyFileContent(const MMKVPath_t &srcPath, MMKVFileHandle_t dstFD);
extern bool copyFileContent(const MMKVPath_t &srcPath, MMKVFileHandle_t dstFD, bool needTruncate);
extern bool copyFileContent(MMKVFileHandle_t srcFD, const MMKVPath_t &dstPath);
extern bool copyFileContent(MMKVFileHandle_t srcFD, MMKVFileHandle_t dstFD);
extern bool copyFileContent(MMKVFileHandle_t srcFD, MMKVFileHandle_t dstFD, bool needTruncate);
extern bool copyFileContent(MMKVFileHandle_t srcFD,
                            MMKVFileHandle_t dstDirFD,
                            const MMKVPath_t &dstDirPath,
                            const MMKVPath_t &dstFileName);

// Stage both sources and snapshot both destinations before writing either
// destination. A late failure rolls both destination contents back.
extern bool copyFileContentPair(MMKVFileHandle_t firstSrcFD,
                                MMKVFileHandle_t secondSrcFD,
                                MMKVFileHandle_t firstDstFD,
                                MMKVFileHandle_t secondDstFD,
                                MMKVFileHandle_t tempDirFD,
                                const MMKVPath_t &tempDirPath);

// As above for destinations created by openOrCreateRegularFilePairInDir().
// Content rollback is handle-bound. On platforms without an atomic
// unlink-by-handle operation, a failed transaction may leave a newly created
// entry in place rather than risk deleting a concurrent replacement by name.
extern bool copyFileContentPair(MMKVFileHandle_t firstSrcFD,
                                MMKVFileHandle_t secondSrcFD,
                                MMKVFileHandle_t firstDstFD,
                                MMKVFileHandle_t secondDstFD,
                                MMKVFileHandle_t tempDirFD,
                                const MMKVPath_t &tempDirPath,
                                const MMKVPath_t &firstDstName,
                                const MMKVPath_t &secondDstName,
                                bool firstCreated,
                                bool secondCreated);

// Return a read/write temporary file pinned beneath dirFD. The directory entry
// is already unlinked/delete-pending, so closing the handle cleans it up.
extern MMKVFileHandle_t createTemporaryFileInDir(MMKVFileHandle_t dirFD, const MMKVPath_t &dirPath);

//#if defined(MMKV_APPLE) || defined(MMKV_WIN32)
bool isDiskOfMMAPFileCorrupted(MemoryFile *file, bool &needReportReadFail);
//#endif

bool deleteFile(const MMKVPath_t &path);

std::optional<MMKVPath_t> getUniqueFileName(const MMKVPath_t &folder, const MMKVPath_t &prefix);

enum WalkType : uint32_t {
    WalkFile = 1 << 0,
    WalkFolder = 1 << 1,
};
// Enumerate direct children of a pinned directory. The callback receives a
// basename, not a reconstructed path. It is invoked only after a successful,
// stable enumeration.
extern bool walkInOpenedDir(MMKVFileHandle_t dirFD,
                            const MMKVPath_t &dirPath,
                            WalkType type,
                            const std::function<void(const MMKVPath_t &, WalkType)> &walker);
extern void walkInDir(const MMKVPath_t &dirPath, WalkType type, const std::function<void(const MMKVPath_t&, WalkType)> &walker);

} // namespace mmkv

#endif
#endif //MMKV_MAMERYFILE_H
