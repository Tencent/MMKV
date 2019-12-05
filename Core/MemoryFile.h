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

#ifndef MMKV_MMAPEDFILE_H
#define MMKV_MMAPEDFILE_H

#include "MMKVPredef.h"

#include <string>
#include <sys/mman.h>

enum SyncFlag : bool { MMKV_SYNC = true, MMKV_ASYNC = false };

namespace mmkv {

class MemoryFile {
    std::string m_name;
    int m_fd;
    void *m_ptr;
    size_t m_size;

    bool mmap();

public:
    explicit MemoryFile(const std::string &path);
    ~MemoryFile() { clearMemoryCache(); }

    // just forbid it for possibly misuse
    MemoryFile(const MemoryFile &other) = delete;
    MemoryFile &operator=(const MemoryFile &other) = delete;

    size_t getFileSize() { return m_size; }

    void *getMemory() { return m_ptr; }

    std::string &getName() { return m_name; }

    int getFd() { return m_fd; }

    // the newly expanded file content will be zeroed
    bool truncate(size_t size);

    bool msync(SyncFlag syncFlag);

    // call this if clearMemoryCache() has been called
    void reloadFromFile();

    void clearMemoryCache();

    bool isFileValid() { return m_fd >= 0 && m_size > 0 && m_ptr; }
};

class MMBuffer;

extern bool mkPath(char *path);
extern bool isFileExist(const std::string &nsFilePath);
extern bool removeFile(const std::string &nsFilePath);
extern MMBuffer *readWholeFile(const char *path);
extern bool zeroFillFile(int fd, size_t startPos, size_t size);
extern bool createFile(const std::string &filePath);

} // namespace mmkv

#endif //MMKV_MMAPEDFILE_H
