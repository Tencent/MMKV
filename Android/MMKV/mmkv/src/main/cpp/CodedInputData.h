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

#include "MMBuffer.h"
#include <cstdint>
#include <string>

class CodedInputData {
    uint8_t *m_ptr;
    int32_t m_size;
    int32_t m_position;

    int8_t readRawByte();

    int32_t readRawVarint32();

    int32_t readRawLittleEndian32();

    int64_t readRawLittleEndian64();

public:
    CodedInputData(const void *oData, int32_t length);

    ~CodedInputData();

    bool isAtEnd() { return m_position == m_size; };

    bool readBool();

    double readDouble();

    float readFloat();

    int64_t readInt64();

    int32_t readInt32();

    int32_t readFixed32();

    std::string readString();

    MMBuffer readData();
};

#endif //MMKV_CODEDINPUTDATA_H
