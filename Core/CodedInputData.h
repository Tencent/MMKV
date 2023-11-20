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

#ifndef MMKV_CODEDINPUTDATA_H
#define MMKV_CODEDINPUTDATA_H
#ifdef __cplusplus

#include "MMKVPredef.h"

#include "KeyValueHolder.h"
#include "MMBuffer.h"
#include <cstdint>

namespace mmkv {

class CodedInputData {
    uint8_t *const m_ptr;
    size_t m_size;
    size_t m_position;

    int8_t readRawByte();

    int32_t readRawVarint32();

    int32_t readRawLittleEndian32();

    int64_t readRawLittleEndian64();

public:
    CodedInputData(const void *oData, size_t length);

    bool isAtEnd() const { return m_position == m_size; };

    void seek(size_t addedSize);

    bool readBool();

    double readDouble();

    float readFloat();

    int64_t readInt64();

    uint64_t readUInt64();

    int32_t readInt32();

    uint32_t readUInt32();

    // exactly is like getValueSize(actualSize = true)
    MMBuffer readData(bool copy = true, bool exactly = false);
    void readData(KeyValueHolder &kvHolder);

    static MMBuffer readRealData(mmkv::MMBuffer & data);

#ifndef MMKV_APPLE
    std::string readString();
    void readString(std::string &s);
    std::string readString(KeyValueHolder &kvHolder);
#else
    NSString *readString();
    NSString *readString(KeyValueHolder &kvHolder);
    NSData *readNSData();
#endif
};

} // namespace mmkv

#endif
#endif //MMKV_CODEDINPUTDATA_H
