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

#include "CodedInputData.h"
#include "MMKVLog.h"
#include "PBUtility.h"
#include <cassert>

using namespace std;

namespace mmkv {

CodedInputData::CodedInputData(const void *oData, int32_t length)
    : m_ptr((uint8_t *) oData), m_size(length), m_position(0) {
    assert(m_ptr);
}

CodedInputData::~CodedInputData() {
    m_ptr = nullptr;
    m_size = 0;
}

double CodedInputData::readDouble() {
    return Int64ToFloat64(this->readRawLittleEndian64());
}

float CodedInputData::readFloat() {
    return Int32ToFloat32(this->readRawLittleEndian32());
}

int64_t CodedInputData::readInt64() {
    int32_t shift = 0;
    int64_t result = 0;
    while (shift < 64) {
        int8_t b = this->readRawByte();
        result |= (int64_t)(b & 0x7f) << shift;
        if ((b & 0x80) == 0) {
            return result;
        }
        shift += 7;
    }
    MMKVError("InvalidProtocolBuffer malformedInt64");
    return 0;
}

uint64_t CodedInputData::readUInt64() {
    return readInt64();
}

int32_t CodedInputData::readInt32() {
    return this->readRawVarint32();
}

uint32_t CodedInputData::readUInt32() {
    return this->readRawVarint32();
}

int32_t CodedInputData::readFixed32() {
    return this->readRawLittleEndian32();
}

bool CodedInputData::readBool() {
    return this->readRawVarint32() != 0;
}

string CodedInputData::readString() {
    int32_t size = this->readRawVarint32();
    if (size <= (m_size - m_position) && size > 0) {
        string result((char *) (m_ptr + m_position), size);
        m_position += size;
        return result;
    } else if (size == 0) {
        return "";
    } else {
        MMKVError("Invalid Size: %d", size);
        return "";
    }
}

MMBuffer CodedInputData::readData() {
    int32_t size = this->readRawVarint32();
    if (size < 0) {
        MMKVError("InvalidProtocolBuffer negativeSize");
        return MMBuffer(0);
    }

    if (size <= m_size - m_position) {
        MMBuffer data(((int8_t *) m_ptr) + m_position, size);
        m_position += size;
        return data;
    } else {
        MMKVError("InvalidProtocolBuffer truncatedMessage");
        return MMBuffer(0);
    }
}

int32_t CodedInputData::readRawVarint32() {
    int8_t tmp = this->readRawByte();
    if (tmp >= 0) {
        return tmp;
    }
    int32_t result = tmp & 0x7f;
    if ((tmp = this->readRawByte()) >= 0) {
        result |= tmp << 7;
    } else {
        result |= (tmp & 0x7f) << 7;
        if ((tmp = this->readRawByte()) >= 0) {
            result |= tmp << 14;
        } else {
            result |= (tmp & 0x7f) << 14;
            if ((tmp = this->readRawByte()) >= 0) {
                result |= tmp << 21;
            } else {
                result |= (tmp & 0x7f) << 21;
                result |= (tmp = this->readRawByte()) << 28;
                if (tmp < 0) {
                    // discard upper 32 bits
                    for (int i = 0; i < 5; i++) {
                        if (this->readRawByte() >= 0) {
                            return result;
                        }
                    }
                    MMKVError("InvalidProtocolBuffer malformed varint32");
                }
            }
        }
    }
    return result;
}

int32_t CodedInputData::readRawLittleEndian32() {
    int8_t b1 = this->readRawByte();
    int8_t b2 = this->readRawByte();
    int8_t b3 = this->readRawByte();
    int8_t b4 = this->readRawByte();
    return (((int32_t) b1 & 0xff)) | (((int32_t) b2 & 0xff) << 8) | (((int32_t) b3 & 0xff) << 16) |
           (((int32_t) b4 & 0xff) << 24);
}

int64_t CodedInputData::readRawLittleEndian64() {
    int8_t b1 = this->readRawByte();
    int8_t b2 = this->readRawByte();
    int8_t b3 = this->readRawByte();
    int8_t b4 = this->readRawByte();
    int8_t b5 = this->readRawByte();
    int8_t b6 = this->readRawByte();
    int8_t b7 = this->readRawByte();
    int8_t b8 = this->readRawByte();
    return (((int64_t) b1 & 0xff)) | (((int64_t) b2 & 0xff) << 8) | (((int64_t) b3 & 0xff) << 16) |
           (((int64_t) b4 & 0xff) << 24) | (((int64_t) b5 & 0xff) << 32) |
           (((int64_t) b6 & 0xff) << 40) | (((int64_t) b7 & 0xff) << 48) |
           (((int64_t) b8 & 0xff) << 56);
}

int8_t CodedInputData::readRawByte() {
    if (m_position == m_size) {
        MMKVError("reach end, m_position: %d, m_size: %d", m_position, m_size);
        return 0;
    }
    int8_t *bytes = (int8_t *) m_ptr;
    return bytes[m_position++];
}

} // namespace mmkv
