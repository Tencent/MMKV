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

#ifdef __cplusplus

#import "KeyValueHolder.h"
#import "MMBuffer.h"
#import "MemoryFile.h"
#import <Foundation/Foundation.h>
#import <memory>

class MiniCodedOutputData {
    uint8_t *m_ptr;
    size_t m_size;
    size_t m_position;
    MemoryFile *m_memoryFile;
    std::shared_ptr<MemoryFile::Segment> m_curSegment;

    void writeRawByte(uint8_t value);

    void writeRawLittleEndian32(int32_t value);

    void writeRawLittleEndian64(int64_t value);

    void writeRawVarint64(int64_t value);

public:
    MiniCodedOutputData(void *ptr, size_t len);

    MiniCodedOutputData(MMBuffer &oData);

    MiniCodedOutputData(MemoryFile *memoryFile, size_t offset = 0);

    ~MiniCodedOutputData();

    size_t spaceLeft() const { return m_size - m_position; }

    void seek(size_t addedSize);

    void writeBool(BOOL value);

    void writeRawVarint32(int32_t value);

    void writeInt32(int32_t value);

    void writeInt64(int64_t value);

    void writeFloat(Float32 value);

    void writeUInt32(uint32_t value);

    void writeUInt64(uint64_t value);

    void writeFixed32(int32_t value);

    void writeDouble(Float64 value);

    void writeString(__unsafe_unretained NSString *value, size_t length);

    MMBuffer writeString(__unsafe_unretained NSString *value, size_t length, uint32_t &crcDigest);

    void writeRawData(__unsafe_unretained NSData *data);

    void writeRawData(const MMBuffer &data);

    void writeData(const MMBuffer &value, uint32_t *crcDigest = nullptr);
};

#endif
