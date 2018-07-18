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

#ifndef MemoryFile_h
#define MemoryFile_h

#import <Foundation/Foundation.h>

class MemoryFile {
    NSString *m_name;
    int m_fd;
    void *m_segmentPtr;
    size_t m_segmentSize;

    // just forbid it for possibly misuse
    MemoryFile(const MemoryFile &other) = delete;
    MemoryFile &operator=(const MemoryFile &other) = delete;

public:
    MemoryFile(NSString *path);
    ~MemoryFile();

    size_t getFileSize() { return m_segmentSize; }

    void *getMemory() { return m_segmentPtr; }

    NSString *getName() { return m_name; }

    int getFd() { return m_fd; }
};

extern bool isFileExist(NSString *nsFilePath);
extern bool createFile(NSString *nsFilePath);
extern bool removeFile(NSString *nsFilePath);
extern bool zeroFillFile(int fd, size_t startPos, size_t size);
extern void tryResetFileProtection(NSString *path);

#endif /* MemoryFile_h */
