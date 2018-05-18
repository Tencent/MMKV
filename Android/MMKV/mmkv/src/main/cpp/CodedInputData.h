//
// Created by Ling Guo on 2018/4/25.
//

#ifndef MMKV_CODEDINPUTDATA_H
#define MMKV_CODEDINPUTDATA_H

#include <stdint.h>
#include <string>
#include "MMBuffer.h"

class CodedInputData {
    uint8_t* bufferPointer;
    int32_t bufferSize;
    int32_t bufferSizeAfterLimit;
    int32_t bufferPos;
    int32_t lastTag;

    /** The absolute position of the end of the current message. */
    int32_t currentLimit;

    /** See setRecursionLimit() */
    int32_t recursionDepth;
    int32_t recursionLimit;

public:
    CodedInputData(const void* oData, int32_t length);
    ~CodedInputData();

    bool isAtEnd() { return bufferPos == bufferSize; };

    bool readBool();

    double readDouble();
    float readFloat();
    int64_t readInt64();
    int32_t readInt32();
    int32_t readFixed32();

    std::string readString();
    MMBuffer readData();

    /**
     * Read one byte from the input.
     *
     * @throws InvalidProtocolBuffer The end of the stream or the current
     *                                        limit was reached.
     */
    int8_t readRawByte();

    /**
     * Read a raw Varint from the stream.  If larger than 32 bits, discard the
     * upper bits.
     */
    int32_t readRawVarint32();
    int64_t readRawVarint64();
    int32_t readRawLittleEndian32();
    int64_t readRawLittleEndian64();

    // Read a fixed size of bytes from the input.
    MMBuffer readRawData(int32_t size);

    int32_t pushLimit(int32_t byteLimit);
    void recomputeBufferSizeAfterLimit();
    void popLimit(int32_t oldLimit);

    void skipRawData(int32_t size);
};

#endif //MMKV_CODEDINPUTDATA_H
