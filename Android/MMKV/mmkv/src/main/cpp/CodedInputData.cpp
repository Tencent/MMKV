//
// Created by Ling Guo on 2018/4/25.
//

#include "CodedInputData.h"
#include "PBUtility.h"
#include "WireFormat.h"
#include <limits.h>
#include "MMKVLog.h"

using namespace std;

static const int32_t DEFAULT_RECURSION_LIMIT = 64;

CodedInputData::CodedInputData(const void * oData, int32_t length)
        : bufferPointer((uint8_t*)oData), bufferSize(length), bufferSizeAfterLimit(0), bufferPos(0), lastTag(0), currentLimit(INT_MAX), recursionDepth(0), recursionLimit(DEFAULT_RECURSION_LIMIT)
{
}

CodedInputData::~CodedInputData() {
    bufferPointer = NULL;
    bufferSize = 0;
}

/** Read a {@code double} field value from the stream. */
double CodedInputData::readDouble() {
    return convertInt64ToFloat64(this->readRawLittleEndian64());
}


/** Read a {@code float} field value from the stream. */
float CodedInputData::readFloat() {
    return convertInt32ToFloat32(this->readRawLittleEndian32());
}

/** Read an {@code int64} field value from the stream. */
int64_t CodedInputData::readInt64() {
    return this->readRawVarint64();
}


/** Read an {@code int32} field value from the stream. */
int32_t CodedInputData::readInt32() {
    return this->readRawVarint32();
}


/** Read a {@code fixed32} field value from the stream. */
int32_t CodedInputData::readFixed32() {
    return this->readRawLittleEndian32();
}


/** Read a {@code bool} field value from the stream. */
bool CodedInputData::readBool() {
    return this->readRawVarint32() != 0;
}


/** Read a {@code string} field value from the stream. */
string CodedInputData::readString() {
    int32_t size = this->readRawVarint32();
    if (size <= (bufferSize - bufferPos) && size > 0) {
        string result((char *) (bufferPointer + bufferPos), size);
        bufferPos += size;
        return result;
    } else if (size == 0) {
        return "";
    } else {
        MMKVError("Inavlid Size: %d", size);
        return "";
    }
}


/** Read a {@code bytes} field value from the stream. */
MMBuffer CodedInputData::readData() {
    int32_t size = this->readRawVarint32();
    if (size < bufferSize - bufferPos && size > 0) {
        // Fast path:  We already have the bytes in a contiguous buffer, so
        //   just copy directly from it.
        MMBuffer result(bufferPointer + bufferPos, size);
        bufferPos += size;
        return result;
    } else {
        // Slow path:  Build a byte array first then copy it.
        return this->readRawData(size);
    }
}

// =================================================================

/**
 * Read a raw Varint from the stream.  If larger than 32 bits, discard the
 * upper bits.
 */
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
                    // Discard upper 32 bits.
                    for (int i = 0; i < 5; i++) {
                        if (this->readRawByte() >= 0) {
                            return result;
                        }
                    }
                    MMKVError("InvalidProtocolBuffer-malformedVarint");
                }
            }
        }
    }
    return result;
}


/** Read a raw Varint from the stream. */
int64_t CodedInputData::readRawVarint64() {
    int32_t shift = 0;
    int64_t result = 0;
    while (shift < 64) {
        int8_t b = this->readRawByte();
        result |= (int64_t)(b & 0x7F) << shift;
        if ((b & 0x80) == 0) {
            return result;
        }
        shift += 7;
    }
    MMKVError("InvalidProtocolBuffer-malformedVarint");
    return 0;
}


/** Read a 32-bit little-endian integer from the stream. */
int32_t CodedInputData::readRawLittleEndian32() {
    int8_t b1 = this->readRawByte();
    int8_t b2 = this->readRawByte();
    int8_t b3 = this->readRawByte();
    int8_t b4 = this->readRawByte();
    return
            (((int32_t)b1 & 0xff)      ) |
            (((int32_t)b2 & 0xff) <<  8) |
            (((int32_t)b3 & 0xff) << 16) |
            (((int32_t)b4 & 0xff) << 24);
}


/** Read a 64-bit little-endian integer from the stream. */
int64_t CodedInputData::readRawLittleEndian64() {
    int8_t b1 = this->readRawByte();
    int8_t b2 = this->readRawByte();
    int8_t b3 = this->readRawByte();
    int8_t b4 = this->readRawByte();
    int8_t b5 = this->readRawByte();
    int8_t b6 = this->readRawByte();
    int8_t b7 = this->readRawByte();
    int8_t b8 = this->readRawByte();
    return
            (((int64_t)b1 & 0xff)      ) |
            (((int64_t)b2 & 0xff) <<  8) |
            (((int64_t)b3 & 0xff) << 16) |
            (((int64_t)b4 & 0xff) << 24) |
            (((int64_t)b5 & 0xff) << 32) |
            (((int64_t)b6 & 0xff) << 40) |
            (((int64_t)b7 & 0xff) << 48) |
            (((int64_t)b8 & 0xff) << 56);
}


/**
 * Sets {@code currentLimit} to (current position) + {@code byteLimit}.  This
 * is called when descending into a length-delimited embedded message.
 *
 * @return the old limit.
 */
int32_t CodedInputData::pushLimit(int32_t byteLimit) {
    if (byteLimit < 0) {
        MMKVError("InvalidProtocolBuffer-negativeSize");
        return currentLimit;
    }
    byteLimit += bufferPos;
    int32_t oldLimit = currentLimit;
    if (byteLimit > oldLimit) {
        MMKVError("InvalidProtocolBuffer-truncatedMessage");
        return currentLimit;
    }
    currentLimit = byteLimit;

    this->recomputeBufferSizeAfterLimit();

    return oldLimit;
}


void CodedInputData::recomputeBufferSizeAfterLimit() {
    bufferSize += bufferSizeAfterLimit;
    int32_t bufferEnd = bufferSize;
    if (bufferEnd > currentLimit) {
        // Limit is in current buffer.
        bufferSizeAfterLimit = bufferEnd - currentLimit;
        bufferSize -= bufferSizeAfterLimit;
    } else {
        bufferSizeAfterLimit = 0;
    }
}


/**
 * Discards the current limit, returning to the previous limit.
 *
 * @param oldLimit The old limit, as returned by {@code pushLimit}.
 */
void CodedInputData::popLimit(int32_t oldLimit) {
    currentLimit = oldLimit;
    this->recomputeBufferSizeAfterLimit();
}

/**
 * Read one byte from the input.
 *
 * @throws InvalidProtocolBufferException The end of the stream or the current
 *                                        limit was reached.
 */
int8_t CodedInputData::readRawByte() {
    if (bufferPos == bufferSize) {
        MMKVError("reach end, bufferPos: %d, bufferSize: %d", bufferPos, bufferSize);
        return 0;
    }
    int8_t* bytes = (int8_t*)bufferPointer;
    return bytes[bufferPos++];
}


/**
 * Read a fixed size of bytes from the input.
 *
 * @throws InvalidProtocolBufferException The end of the stream or the current
 *                                        limit was reached.
 */
MMBuffer CodedInputData::readRawData(int32_t size) {
    if (size < 0) {
        MMKVError("InvalidProtocolBuffer negativeSize");
        return MMBuffer(0);
    }

    if (bufferPos + size > currentLimit) {
        // Read to the end of the stream anyway.
        this->skipRawData(currentLimit - bufferPos);
        // Then fail.
        MMKVError("InvalidProtocolBuffer truncatedMessage");
        return MMBuffer(0);
    }

    if (size <= bufferSize - bufferPos) {
        // We have all the bytes we need already.
        MMBuffer data(((int8_t *) bufferPointer) + bufferPos, size);
        bufferPos += size;
        return data;
    } else {
        MMKVError("InvalidProtocolBuffer truncatedMessage");
        return MMBuffer(0);
    }
}


/**
 * Reads and discards {@code size} bytes.
 *
 * @throws InvalidProtocolBufferException The end of the stream or the current
 *                                        limit was reached.
 */
void CodedInputData::skipRawData(int32_t size) {
    if (size < 0) {
        MMKVError("InvalidProtocolBuffer-negativeSize");
    }

    if (bufferPos + size > currentLimit) {
        // Read to the end of the stream anyway.
        this->skipRawData(currentLimit - bufferPos);
        // Then fail.
        MMKVError("InvalidProtocolBuffer-truncatedMessage");
    }

    if (size <= (bufferSize - bufferPos)) {
        // We have all the bytes we need already.
        bufferPos += size;
    } else {
        MMKVError("InvalidProtocolBuffer-truncatedMessage");
    }
}