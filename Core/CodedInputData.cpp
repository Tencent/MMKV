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
#include "PBUtility.h"
#include <stdexcept>

#ifdef MMKV_APPLE
#    if __has_feature(objc_arc)
#        error This file must be compiled with MRC. Use -fno-objc-arc flag.
#    endif
#endif // MMKV_APPLE

using namespace std;

namespace mmkv {

CodedInputData::CodedInputData(const void *oData, size_t length)
    : m_ptr((uint8_t *) oData), m_size(length), m_position(0) {
    MMKV_ASSERT(m_ptr);
}

void CodedInputData::seek(size_t addedSize) {
    if (m_position + addedSize > m_size) {
        throw out_of_range("OutOfSpace");
    }
    m_position += addedSize;
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
    throw invalid_argument("InvalidProtocolBuffer malformedInt64");
}

uint64_t CodedInputData::readUInt64() {
    return static_cast<uint64_t>(readInt64());
}

int32_t CodedInputData::readInt32() {
    return this->readRawVarint32();
}

uint32_t CodedInputData::readUInt32() {
    return static_cast<uint32_t>(readRawVarint32());
}

bool CodedInputData::readBool() {
    return this->readRawVarint32() != 0;
}

#ifndef MMKV_APPLE

string CodedInputData::readString() {
    int32_t size = readRawVarint32();
    if (size < 0) {
        throw length_error("InvalidProtocolBuffer negativeSize");
    }

    auto s_size = static_cast<size_t>(size);
    if (s_size <= m_size - m_position) {
        string result((char *) (m_ptr + m_position), s_size);
        m_position += s_size;
        return result;
    } else {
        throw out_of_range("InvalidProtocolBuffer truncatedMessage");
    }
}

void CodedInputData::readString(string &s) {
    int32_t size = readRawVarint32();
    if (size < 0) {
        throw length_error("InvalidProtocolBuffer negativeSize");
    }

    auto s_size = static_cast<size_t>(size);
    if (s_size <= m_size - m_position) {
        s.resize(s_size);
        memcpy((void *) s.data(), (char *) (m_ptr + m_position), s_size);
        m_position += s_size;
    } else {
        throw out_of_range("InvalidProtocolBuffer truncatedMessage");
    }
}

string CodedInputData::readString(KeyValueHolder &kvHolder) {
    kvHolder.offset = static_cast<uint32_t>(m_position);

    int32_t size = this->readRawVarint32();
    if (size < 0) {
        throw length_error("InvalidProtocolBuffer negativeSize");
    }

    auto s_size = static_cast<size_t>(size);
    if (s_size <= m_size - m_position) {
        kvHolder.keySize = static_cast<uint16_t>(s_size);

        auto ptr = m_ptr + m_position;
        string result((char *) (m_ptr + m_position), s_size);
        m_position += s_size;
        return result;
    } else {
        throw out_of_range("InvalidProtocolBuffer truncatedMessage");
    }
}

#endif

MMBuffer CodedInputData::readRealData(mmkv::MMBuffer & data) {
    CodedInputData input(data.getPtr(), data.length());
    return input.readData(false, true);
}

MMBuffer CodedInputData::readData(bool copy, bool exactly) {
    int32_t size = this->readRawVarint32();
    if (size < 0) {
        throw length_error("InvalidProtocolBuffer negativeSize");
    }

    auto s_size = static_cast<size_t>(size);
    bool isSizeValid = exactly ? (s_size == m_size - m_position) : (s_size <= m_size - m_position);
    if (isSizeValid) {
        size_t pos = m_position;
        m_position += s_size;
        auto copyFlag = copy ? MMBufferCopy : MMBufferNoCopy;
        return MMBuffer(((int8_t *) m_ptr) + pos, s_size, copyFlag);
    } else {
        throw out_of_range("InvalidProtocolBuffer truncatedMessage");
    }
}

void CodedInputData::readData(KeyValueHolder &kvHolder) {
    int32_t size = this->readRawVarint32();
    if (size < 0) {
        throw length_error("InvalidProtocolBuffer negativeSize");
    }

    auto s_size = static_cast<size_t>(size);
    if (s_size <= m_size - m_position) {
        kvHolder.computedKVSize = static_cast<uint16_t>(m_position - kvHolder.offset);
        kvHolder.valueSize = static_cast<uint32_t>(s_size);

        m_position += s_size;
    } else {
        throw out_of_range("InvalidProtocolBuffer truncatedMessage");
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
                    throw invalid_argument("InvalidProtocolBuffer malformed varint32");
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
           (((int64_t) b4 & 0xff) << 24) | (((int64_t) b5 & 0xff) << 32) | (((int64_t) b6 & 0xff) << 40) |
           (((int64_t) b7 & 0xff) << 48) | (((int64_t) b8 & 0xff) << 56);
}

int8_t CodedInputData::readRawByte() {
    if (m_position == m_size) {
        auto msg = "reach end, m_position: " + to_string(m_position) + ", m_size: " + to_string(m_size);
        throw out_of_range(msg);
    }
    auto *bytes = (int8_t *) m_ptr;
    return bytes[m_position++];
}

#ifdef MMKV_APPLE
#endif // MMKV_APPLE

} // namespace mmkv
