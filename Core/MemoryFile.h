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

#include "MMKVPredef.h"

#include <string>

enum SyncFlag : bool { MMKV_SYNC = true, MMKV_ASYNC = false };

#ifdef MMKV_ANDROID
constexpr auto ASHMEM_NAME_DEF = "/dev/ashmem";

namespace mmkv {
extern int g_android_api;

enum FileType : bool { MMFILE_TYPE_FILE = false, MMFILE_TYPE_ASHMEM = true };
} // namespace mmkv
#endif

namespace mmkv {

class MemoryFile {
    std::string m_name;
    int m_fd;
    void *m_ptr;
    size_t m_size;

    bool mmap();

    void doCleanMemoryCache(bool forceClean);

public:
#ifndef MMKV_ANDROID
    explicit MemoryFile(const std::string &path);
#else
    MemoryFile(const std::string &path, size_t size, FileType fileType);
    explicit MemoryFile(int ashmemFD);

    const FileType m_fileType;
#endif

    ~MemoryFile() { doCleanMemoryCache(true); }

    size_t getFileSize() { return m_size; }

    // get the actual file size on disk
    size_t getActualFileSize();

    void *getMemory() { return m_ptr; }

    const std::string &getName() { return m_name; }

    int getFd() { return m_fd; }

    // the newly expanded file content will be zeroed
    bool truncate(size_t size);

    bool msync(SyncFlag syncFlag);

    // call this if clearMemoryCache() has been called
    void reloadFromFile();

    void clearMemoryCache() { doCleanMemoryCache(false); }

    bool isFileValid() { return m_fd >= 0 && m_size > 0 && m_ptr; }

    // just forbid it for possibly misuse
    MemoryFile(const MemoryFile &other) = delete;
    MemoryFile &operator=(const MemoryFile &other) = delete;
};

class MMBuffer;

extern bool mkPath(char *path);
extern bool isFileExist(const std::string &nsFilePath);
extern bool removeFile(const std::string &nsFilePath);
extern MMBuffer *readWholeFile(const char *path);
extern bool zeroFillFile(int fd, size_t startPos, size_t size);
extern bool createFile(const std::string &filePath);
extern int getPageSize();

} // namespace mmkv

#endif //MMKV_MAMERYFILE_H
