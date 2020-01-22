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

#ifndef MMKV_CODEDOUTPUTDATA_H
#define MMKV_CODEDOUTPUTDATA_H

#include "MMKVPredef.h"

#include "MMBuffer.h"
#include <cstdint>
#include <string>

namespace mmkv {

class CodedOutputData {
    uint8_t *const m_ptr;
    size_t m_size;
    size_t m_position;

public:
    CodedOutputData(void *ptr, size_t len);

    size_t spaceLeft();

    uint8_t *curWritePointer();

    void seek(size_t addedSize);

    void writeRawByte(uint8_t value);

    void writeRawLittleEndian32(int32_t value);

    void writeRawLittleEndian64(int64_t value);

    void writeRawVarint32(int32_t value);

    void writeRawVarint64(int64_t value);

    void writeRawData(const MMBuffer &data);

    void writeDouble(double value);

    void writeFloat(float value);

    void writeInt64(int64_t value);

    void writeUInt64(uint64_t value);

    void writeInt32(int32_t value);

    void writeUInt32(uint32_t value);

    void writeBool(bool value);

    void writeData(const MMBuffer &value);

#ifndef MMKV_IOS_OR_MAC
    void writeString(const std::string &value);
#else
    void writeString(__unsafe_unretained NSString *value);
#endif
};

} // namespace mmkv

#endif //MMKV_CODEDOUTPUTDATA_H
