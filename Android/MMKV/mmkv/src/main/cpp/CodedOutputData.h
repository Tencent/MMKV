//
// Created by Ling Guo on 2018/4/25.
//

#ifndef MMKV_CODEDOUTPUTDATA_H
#define MMKV_CODEDOUTPUTDATA_H

#include <stdint.h>
#include <string>
#include "MMBuffer.h"

class CodedOutputData {
    uint8_t* bufferPointer;
    size_t bufferLength;
    int32_t position;

public:
    CodedOutputData(void* ptr, size_t len);
    ~CodedOutputData();

    int32_t spaceLeft();
    void seek(size_t addedSize);

    void writeRawByte(uint8_t value);

    void writeRawLittleEndian32(int32_t value);
    void writeRawLittleEndian64(int64_t value);

    /**
     * Encode and write a varint.  value is treated as
     * unsigned, so it won't be sign-extended if negative.
     */
    void writeRawVarint32(int32_t value);
    void writeRawVarint64(int64_t value);

    void writeRawData(const MMBuffer& data);

    void writeDouble(double value);
    void writeFloat(float value);
    void writeInt64(int64_t value);
    void writeInt32(int32_t value);
    void writeFixed32(int32_t value);
    void writeBool(bool value);
    void writeString(const std::string& value);
    void writeData(const MMBuffer& value);
};



#endif //MMKV_CODEDOUTPUTDATA_H
