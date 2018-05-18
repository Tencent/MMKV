//
// Created by Ling Guo on 2018/4/25.
//

#include "CodedOutputData.h"
#include "WireFormat.h"
#include "PBUtility.h"
using namespace std;

CodedOutputData::CodedOutputData(void* ptr, size_t len) {
    bufferPointer = (uint8_t*)ptr;
    bufferLength = len;
    position = 0;
}

CodedOutputData::~CodedOutputData() {
    bufferPointer = NULL;
    position = 0;
}

void CodedOutputData::writeDouble(double value) {
    this->writeRawLittleEndian64(convertFloat64ToInt64(value));
}

void CodedOutputData::writeFloat(float value) {
    this->writeRawLittleEndian32(convertFloat32ToInt32(value));
}

void CodedOutputData::writeInt64(int64_t value) {
    this->writeRawVarint64(value);
}

void CodedOutputData::writeInt32(int32_t value) {
    if (value >= 0) {
        this->writeRawVarint32(value);
    } else {
        // Must sign-extend
        this->writeRawVarint64(value);
    }
}

void CodedOutputData::writeFixed32(int32_t value) {
    this->writeRawLittleEndian32(value);
}

void CodedOutputData::writeBool(bool value) {
    this->writeRawByte(value ? 1 : 0);
}

void CodedOutputData::writeString(const string& value) {
    size_t numberOfBytes = value.size();
    this->writeRawVarint32((int32_t)numberOfBytes);
    memcpy(bufferPointer + position, ((uint8_t*)value.data()), numberOfBytes);
    position += numberOfBytes;
}


void CodedOutputData::writeData(const MMBuffer& value) {
    this->writeRawVarint32((int32_t)value.length());
    this->writeRawData(value);
}

/**
 * If writing to a flat array, return the space left in the array.
 * Otherwise, throws {@code UnsupportedOperationException}.
 */
int32_t CodedOutputData::spaceLeft() {
    return int32_t(bufferLength - position);
}

void CodedOutputData::seek(size_t addedSize) {
    position += addedSize;

    if (position > bufferLength) {
        throw "OutOfSpace";
    }
}


/** Write a single byte. */
void CodedOutputData::writeRawByte(uint8_t value) {
    if (position == bufferLength) {
        throw "position: " + to_string(position) + ", bufferLength: " + to_string(bufferLength);
    }

    bufferPointer[position++] = value;
}


/** Write an array of bytes. */
void CodedOutputData::writeRawData(const MMBuffer& data) {
    size_t numberOfBytes = data.length();
    memcpy(bufferPointer + position, data.getPtr(), numberOfBytes);
    position += numberOfBytes;
}


/**
 * Encode and write a varint.  {@code value} is treated as
 * unsigned, so it won't be sign-extended if negative.
 */
void CodedOutputData::writeRawVarint32(int32_t value) {
    while (true) {
        if ((value & ~0x7F) == 0) {
            this->writeRawByte(value);
            return;
        } else {
            this->writeRawByte((value & 0x7F) | 0x80);
            value = logicalRightShift32(value, 7);
        }
    }
}


/** Encode and write a varint. */
void CodedOutputData::writeRawVarint64(int64_t value) {
    while (true) {
        if ((value & ~0x7FL) == 0) {
            this->writeRawByte((int32_t) value);
            return;
        } else {
            this->writeRawByte(((int32_t) value & 0x7F) | 0x80);
            value = logicalRightShift64(value, 7);
        }
    }
}


/** Write a little-endian 32-bit integer. */
void CodedOutputData::writeRawLittleEndian32(int32_t value) {
    this->writeRawByte((value      ) & 0xFF);
    this->writeRawByte((value >>  8) & 0xFF);
    this->writeRawByte((value >> 16) & 0xFF);
    this->writeRawByte((value >> 24) & 0xFF);
}


/** Write a little-endian 64-bit integer. */
void CodedOutputData::writeRawLittleEndian64(int64_t value) {
    this->writeRawByte((int32_t)(value      ) & 0xFF);
    this->writeRawByte((int32_t)(value >>  8) & 0xFF);
    this->writeRawByte((int32_t)(value >> 16) & 0xFF);
    this->writeRawByte((int32_t)(value >> 24) & 0xFF);
    this->writeRawByte((int32_t)(value >> 32) & 0xFF);
    this->writeRawByte((int32_t)(value >> 40) & 0xFF);
    this->writeRawByte((int32_t)(value >> 48) & 0xFF);
    this->writeRawByte((int32_t)(value >> 56) & 0xFF);
}
