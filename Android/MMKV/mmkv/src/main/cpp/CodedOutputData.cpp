//
// Created by Ling Guo on 2018/4/25.
//

#include "CodedOutputData.h"
#include "MMKVLog.h"
#include "PBUtility.h"

using namespace std;

CodedOutputData::CodedOutputData(void *ptr, size_t len)
    : m_ptr((uint8_t *) ptr), m_size(len), m_position(0) {
    assert(m_ptr);
}

CodedOutputData::~CodedOutputData() {
    m_ptr = nullptr;
    m_position = 0;
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

void CodedOutputData::writeInt32(int32_t value) {
    if (value >= 0) {
        this->writeRawVarint32(value);
    } else {
        this->writeRawVarint64(value);
    }
}

void CodedOutputData::writeBool(bool value) {
    this->writeRawByte(static_cast<uint8_t>(value ? 1 : 0));
}

void CodedOutputData::writeString(const string &value) {
    size_t numberOfBytes = value.size();
    this->writeRawVarint32((int32_t) numberOfBytes);
    memcpy(m_ptr + m_position, ((uint8_t *) value.data()), numberOfBytes);
    m_position += numberOfBytes;
}

void CodedOutputData::writeData(const MMBuffer &value) {
    this->writeRawVarint32((int32_t) value.length());
    this->writeRawData(value);
}

int32_t CodedOutputData::spaceLeft() {
    return int32_t(m_size - m_position);
}

void CodedOutputData::seek(size_t addedSize) {
    m_position += addedSize;

    if (m_position > m_size) {
        MMKVError("OutOfSpace");
    }
}

void CodedOutputData::writeRawByte(uint8_t value) {
    if (m_position == m_size) {
        MMKVError("m_position: %d, m_size: %zd", m_position, m_size);
        return;
    }

    m_ptr[m_position++] = value;
}

void CodedOutputData::writeRawData(const MMBuffer &data) {
    size_t numberOfBytes = data.length();
    memcpy(m_ptr + m_position, data.getPtr(), numberOfBytes);
    m_position += numberOfBytes;
}

void CodedOutputData::writeRawVarint32(int32_t value) {
    while (true) {
        if ((value & ~0x7f) == 0) {
            this->writeRawByte(static_cast<uint8_t>(value));
            return;
        } else {
            this->writeRawByte(static_cast<uint8_t>((value & 0x7F) | 0x80));
            value = logicalRightShift32(value, 7);
        }
    }
}

void CodedOutputData::writeRawVarint64(int64_t value) {
    while (true) {
        if ((value & ~0x7f) == 0) {
            this->writeRawByte(static_cast<uint8_t>(value));
            return;
        } else {
            this->writeRawByte(static_cast<uint8_t>((value & 0x7f) | 0x80));
            value = logicalRightShift64(value, 7);
        }
    }
}

void CodedOutputData::writeRawLittleEndian32(int32_t value) {
    this->writeRawByte(static_cast<uint8_t>((value) &0xff));
    this->writeRawByte(static_cast<uint8_t>((value >> 8) & 0xff));
    this->writeRawByte(static_cast<uint8_t>((value >> 16) & 0xff));
    this->writeRawByte(static_cast<uint8_t>((value >> 24) & 0xff));
}

void CodedOutputData::writeRawLittleEndian64(int64_t value) {
    this->writeRawByte(static_cast<uint8_t>((value) &0xff));
    this->writeRawByte(static_cast<uint8_t>((value >> 8) & 0xff));
    this->writeRawByte(static_cast<uint8_t>((value >> 16) & 0xff));
    this->writeRawByte(static_cast<uint8_t>((value >> 24) & 0xff));
    this->writeRawByte(static_cast<uint8_t>((value >> 32) & 0xff));
    this->writeRawByte(static_cast<uint8_t>((value >> 40) & 0xff));
    this->writeRawByte(static_cast<uint8_t>((value >> 48) & 0xff));
    this->writeRawByte(static_cast<uint8_t>((value >> 56) & 0xff));
}
