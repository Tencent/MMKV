//
// Created by Ling Guo on 2018/4/25.
//

#ifndef MMKV_CODEDOUTPUTDATA_H
#define MMKV_CODEDOUTPUTDATA_H

#include "MMBuffer.h"
#include <cstdint>
#include <string>

class CodedOutputData {
    uint8_t *m_ptr;
    size_t m_size;
    int32_t m_position;

public:
    CodedOutputData(void *ptr, size_t len);

    ~CodedOutputData();

    int32_t spaceLeft();

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

    void writeInt32(int32_t value);

    void writeBool(bool value);

    void writeString(const std::string &value);

    void writeData(const MMBuffer &value);
};

#endif //MMKV_CODEDOUTPUTDATA_H
