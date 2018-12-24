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

#include <string>

namespace mmkv {

extern const size_t DEFAULT_MMAP_SIZE;

class MmapedFile {
    std::wstring m_name;
    HANDLE m_file;
    HANDLE m_fileMapping;
    void *m_segmentPtr;
    size_t m_segmentSize;

    // just forbid it for possibly misuse
    MmapedFile(const MmapedFile &other) = delete;
    MmapedFile &operator=(const MmapedFile &other) = delete;

public:
    MmapedFile(const std::wstring &path, size_t size = static_cast<size_t>(DEFAULT_MMAP_SIZE));
    ~MmapedFile();

    size_t getFileSize() { return m_segmentSize; }

    void *getMemory() { return m_segmentPtr; }

    std::wstring &getName() { return m_name; }

    HANDLE getFd() { return m_file; }
};

class MMBuffer;

extern size_t getpagesize();
extern std::wstring string2wstring(const std::string &str);
extern bool mkPath(const std::wstring &str);
extern bool isFileExist(const std::wstring &nsFilePath);
extern bool removeFile(const std::wstring &nsFilePath);
extern MMBuffer *readWholeFile(const std::wstring &nsFilePath);
extern bool zeroFillFile(HANDLE file, size_t startPos, size_t size);
extern bool ftruncate(HANDLE file, size_t size);
extern bool getfilesize(HANDLE file, size_t &size);

} // namespace mmkv

#endif //MMKV_MMAPEDFILE_H
