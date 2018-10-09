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

#import "LRUCache.hpp"
#import "MMBuffer.h"
#import <Foundation/Foundation.h>

extern const int DEFAULT_MMAP_SIZE;

class MemoryFile {
    NSString *m_name;
    int m_fd;
    size_t m_size;

public:
    struct Segment {
        uint8_t *ptr;
        size_t size;
        size_t offset;

        Segment(void *source, size_t size, size_t offset) noexcept;
        Segment(Segment &&other) noexcept;
        ~Segment();

        void zeroFill();

        inline bool inside(size_t offset) const {
            return offset >= this->offset && offset < this->offset + this->size;
        }
        inline bool inside(size_t offset, size_t size) const {
            return offset >= this->offset && offset + size <= this->offset + this->size;
        }

    private:
        // just forbid it for possibly misuse
        Segment(const Segment &other) = delete;
        Segment &operator=(const Segment &other) = delete;
    };

private:
    std::shared_ptr<Segment> m_ptr;
    uint32_t offset2index(size_t offset) const;
    // index -> segment
    LRUCache<uint32_t, std::shared_ptr<Segment>> m_segmentCache;
    void prepareSegments();

    Segment *internalTryGetSegment(uint32_t index);

    // just forbid it for possibly misuse
    MemoryFile(const MemoryFile &other) = delete;
    MemoryFile &operator=(const MemoryFile &other) = delete;

    friend class MiniCodedInputData;
    friend class MiniCodedOutputData;

public:
    MemoryFile(NSString *path);
    ~MemoryFile();

    size_t getFileSize() { return m_size; }

    NSString *getName() { return m_name; }

    int getFd() { return m_fd; }

    std::shared_ptr<Segment> tryGetSegment(uint32_t index);

    MMBuffer read(size_t offset, size_t size);

    bool write(size_t offset, const void *source, size_t size);

    bool memmove(size_t targetOffset, size_t sourceOffset, size_t size, uint32_t *crcPtr = nullptr);

    bool truncate(size_t size);

    uint32_t crc32(uint32_t digest, size_t offset, size_t size);

    void sync(int syncFlag);
};

extern bool isFileExist(NSString *nsFilePath);
extern bool createFile(NSString *nsFilePath);
extern bool removeFile(NSString *nsFilePath);
extern void tryResetFileProtection(NSString *path);

#endif /* MemoryFile_h */
