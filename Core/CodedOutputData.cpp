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

#include "CodedOutputData.h"
#include "PBUtility.h"
#include <cstring>
#include <limits>
#include <stdexcept>

#ifdef MMKV_APPLE
#    if __has_feature(objc_arc)
#        error This file must be compiled with MRC. Use -fno-objc-arc flag.
#    endif
#endif // MMKV_APPLE

using namespace std;

namespace mmkv {

constexpr size_t MaxVarint32Size = 5;
constexpr size_t MaxVarint64Size = 10;

CodedOutputData::CodedOutputData(void *ptr, size_t len) : m_ptr((uint8_t *) ptr), m_size(len), m_position(0) {
    MMKV_ASSERT(m_ptr);
}

uint8_t *CodedOutputData::curWritePointer() {
    return m_ptr + m_position;
}

void CodedOutputData::requireSpace(size_t length) const {
    if (m_position > m_size || length > m_size - m_position) {
        throw out_of_range("m_position: " + to_string(m_position) + ", length: " + to_string(length) +
                           ", m_size: " + to_string(m_size));
    }
}

void CodedOutputData::writeDouble(double value) {
    this->writeRawLittleEndian64(Float64ToInt64(value));
}

void CodedOutputData::writeFloat(float value) {
    this->writeRawLittleEndian32(Float32ToInt32(value));
}

void CodedOutputData::writeInt64(int64_t value) {
    this->writeRawVarint64(value);
}

void CodedOutputData::writeUInt64(uint64_t value) {
    writeRawVarint64(static_cast<int64_t>(value));
}

void CodedOutputData::writeInt32(int32_t value) {
    if (value >= 0) {
        this->writeRawVarint32(value);
    } else {
        this->writeRawVarint64(value);
    }
}

void CodedOutputData::writeUInt32(uint32_t value) {
    writeRawVarint32(static_cast<int32_t>(value));
}

void CodedOutputData::writeBool(bool value) {
    this->writeRawByte(static_cast<uint8_t>(value ? 1 : 0));
}

void CodedOutputData::writeData(const MMBuffer &value) {
    const auto length = value.length();
    if (length > numeric_limits<uint32_t>::max()) {
        throw length_error("MMBuffer is too large to encode");
    }
    const auto prefixSize = static_cast<size_t>(pbRawVarint32Size(static_cast<uint32_t>(length)));
    const auto available = spaceLeft();
    if (prefixSize > available || length > available - prefixSize) {
        throw out_of_range("length-delimited MMBuffer exceeds output capacity");
    }
    writeRawVarint32Unchecked(static_cast<uint32_t>(length));
    memcpy(m_ptr + m_position, value.getPtr(), length);
    m_position += length;
}

void CodedOutputData::writeString(const string &value) {
    const auto numberOfBytes = value.size();
    if (numberOfBytes > numeric_limits<uint32_t>::max()) {
        throw length_error("string is too large to encode");
    }
    const auto prefixSize = static_cast<size_t>(pbRawVarint32Size(static_cast<uint32_t>(numberOfBytes)));
    const auto available = spaceLeft();
    if (prefixSize > available || numberOfBytes > available - prefixSize) {
        throw out_of_range("length-delimited string exceeds output capacity");
    }
    writeRawVarint32Unchecked(static_cast<uint32_t>(numberOfBytes));
    memcpy(m_ptr + m_position, ((uint8_t *) value.data()), numberOfBytes);
    m_position += numberOfBytes;
}

size_t CodedOutputData::spaceLeft() {
    if (m_size <= m_position) {
        return 0;
    }
    return m_size - m_position;
}

void CodedOutputData::seek(size_t addedSize) {
    requireSpace(addedSize);
    m_position += addedSize;
}

void CodedOutputData::reset() {
    m_position = 0;
}

size_t CodedOutputData::getPosition() {
    return m_position;
}

void CodedOutputData::setPosition(size_t position) {
    if (position > m_size) {
        throw out_of_range("position: " + to_string(position) + ", m_size: " + to_string(m_size));
    }
    m_position = position;
}

void CodedOutputData::writeRawByte(uint8_t value) {
    if (m_position >= m_size) {
        throw out_of_range("m_position: " + to_string(m_position) + " m_size: " + to_string(m_size));
    }

    m_ptr[m_position++] = value;
}

void CodedOutputData::writeRawData(const MMBuffer &data) {
    size_t numberOfBytes = data.length();
    requireSpace(numberOfBytes);
    memcpy(m_ptr + m_position, data.getPtr(), numberOfBytes);
    m_position += numberOfBytes;
}

void CodedOutputData::writeRawVarint32(int32_t value) {
    auto bits = static_cast<uint32_t>(value);
    if (m_position > m_size || m_size - m_position < MaxVarint32Size) {
        requireSpace(pbRawVarint32Size(bits));
    }
    writeRawVarint32Unchecked(bits);
}

void CodedOutputData::writeRawVarint32Unchecked(uint32_t bits) {
    while (bits > 0x7f) {
        m_ptr[m_position++] = static_cast<uint8_t>((bits & 0x7f) | 0x80);
        bits >>= 7;
    }
    m_ptr[m_position++] = static_cast<uint8_t>(bits);
}

void CodedOutputData::writeRawVarint64(int64_t value) {
    auto bits = static_cast<uint64_t>(value);
    if (m_position > m_size || m_size - m_position < MaxVarint64Size) {
        requireSpace(pbUInt64Size(bits));
    }
    writeRawVarint64Unchecked(bits);
}

void CodedOutputData::writeRawVarint64Unchecked(uint64_t bits) {
    while (bits > 0x7f) {
        m_ptr[m_position++] = static_cast<uint8_t>((bits & 0x7f) | 0x80);
        bits >>= 7;
    }
    m_ptr[m_position++] = static_cast<uint8_t>(bits);
}

void CodedOutputData::writeRawLittleEndian32(int32_t value) {
    requireSpace(sizeof(uint32_t));
    auto bits = static_cast<uint32_t>(value);
    for (size_t shift = 0; shift < 32; shift += 8) {
        m_ptr[m_position++] = static_cast<uint8_t>(bits >> shift);
    }
}

void CodedOutputData::writeRawLittleEndian64(int64_t value) {
    requireSpace(sizeof(uint64_t));
    auto bits = static_cast<uint64_t>(value);
    for (size_t shift = 0; shift < 64; shift += 8) {
        m_ptr[m_position++] = static_cast<uint8_t>(bits >> shift);
    }
}

} // namespace mmkv
