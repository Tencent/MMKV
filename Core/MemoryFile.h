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

#ifdef MMKV_ANDROID
MMKVPath_t ashmemMMKVPathWithID(const MMKVPath_t &mmapID);

namespace mmkv {
extern int g_android_api;

enum FileType : bool { MMFILE_TYPE_FILE = false, MMFILE_TYPE_ASHMEM = true };
} // namespace mmkv
#endif // MMKV_ANDROID

namespace mmkv {

class MemoryFile {
    MMKVPath_t m_name;
    MMKVFileHandle_t m_fd;
#ifdef MMKV_WIN32
    HANDLE m_fileMapping;
#endif
    void *m_ptr;
    size_t m_size;

    bool mmap();

    void doCleanMemoryCache(bool forceClean);

public:
#ifndef MMKV_ANDROID
    explicit MemoryFile(const MMKVPath_t &path);
#else
    MemoryFile(const MMKVPath_t &path, size_t size, FileType fileType);
    explicit MemoryFile(MMKVFileHandle_t ashmemFD);

    const FileType m_fileType;
#endif // MMKV_ANDROID

    ~MemoryFile() { doCleanMemoryCache(true); }

    size_t getFileSize() const { return m_size; }

    // get the actual file size on disk
    size_t getActualFileSize();

    void *getMemory() { return m_ptr; }

    const MMKVPath_t &getName() { return m_name; }

    MMKVFileHandle_t getFd() { return m_fd; }

    // the newly expanded file content will be zeroed
    bool truncate(size_t size);

    bool msync(SyncFlag syncFlag);

    // call this if clearMemoryCache() has been called
    void reloadFromFile();

    void clearMemoryCache() { doCleanMemoryCache(false); }

#ifndef MMKV_WIN32
    bool isFileValid() { return m_fd >= 0 && m_size > 0 && m_ptr; }
#else
    bool isFileValid() { return m_fd != INVALID_HANDLE_VALUE && m_size > 0 && m_fileMapping && m_ptr; }
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
} // namespace mmkv

#endif
#endif //MMKV_MAMERYFILE_H
