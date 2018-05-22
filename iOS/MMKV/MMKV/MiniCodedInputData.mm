//
//  MiniCodedInputData.mm
//  MicroMessenger
//
//  Created by Guo Ling on 4/26/13.
//  Copyright (c) 2013 Tencent. All rights reserved.
//

#import "MiniCodedInputData.h"
#import "MiniPBUtility.h"
#import "MiniWireFormat.h"

static const int32_t DEFAULT_RECURSION_LIMIT = 64;
static const int32_t DEFAULT_SIZE_LIMIT = 64 << 20;  // 64MB

MiniCodedInputData::MiniCodedInputData(NSData* oData)
	: bufferPointer((uint8_t*)oData.bytes), bufferSize((int32_t)oData.length), bufferSizeAfterLimit(0), bufferPos(0), lastTag(0), currentLimit(INT_MAX), recursionDepth(0), recursionLimit(DEFAULT_RECURSION_LIMIT), sizeLimit(DEFAULT_SIZE_LIMIT)
{
}

MiniCodedInputData::~MiniCodedInputData() {
	bufferPointer = NULL;
	bufferSize = 0;
}

BOOL MiniCodedInputData::skipField(int32_t tag) {
	switch (PBWireFormatGetTagWireType(tag)) {
		case PBWireFormatVarint:
			this->readInt32();
			return YES;
		case PBWireFormatFixed64:
			this->readRawLittleEndian64();
			return YES;
		case PBWireFormatLengthDelimited:
			this->skipRawData(this->readRawVarint32());
			return YES;
		case PBWireFormatFixed32:
			this->readRawLittleEndian32();
			return YES;
		default:
            NSString *reason = [NSString stringWithFormat:@"Invalid Wire Type, %d", tag];
			@throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:reason userInfo:nil];
            return NO;
	}
}


/** Read a {@code double} field value from the stream. */
Float64 MiniCodedInputData::readDouble() {
	return convertInt64ToFloat64(this->readRawLittleEndian64());
}


/** Read a {@code float} field value from the stream. */
Float32 MiniCodedInputData::readFloat() {
	return convertInt32ToFloat32(this->readRawLittleEndian32());
}


/** Read a {@code uint64} field value from the stream. */
uint64_t MiniCodedInputData::readUInt64() {
	return this->readRawVarint64();
}


/** Read an {@code int64} field value from the stream. */
int64_t MiniCodedInputData::readInt64() {
	return this->readRawVarint64();
}


/** Read an {@code int32} field value from the stream. */
int32_t MiniCodedInputData::readInt32() {
	return this->readRawVarint32();
}


/** Read a {@code fixed32} field value from the stream. */
int32_t MiniCodedInputData::readFixed32() {
	return this->readRawLittleEndian32();
}


/** Read a {@code bool} field value from the stream. */
BOOL MiniCodedInputData::readBool() {
	return this->readRawVarint32() != 0;
}


/** Read a {@code string} field value from the stream. */
NSString* MiniCodedInputData::readString() {
	int32_t size = this->readRawVarint32();
	if (size <= (bufferSize - bufferPos) && size > 0) {
		// Fast path:  We already have the bytes in a contiguous buffer, so
		//   just copy directly from it.
		//  new String(buffer, bufferPos, size, "UTF-8");
        NSString* result = [[NSString alloc] initWithBytes:(bufferPointer + bufferPos)
													 length:size
												   encoding:NSUTF8StringEncoding];
		bufferPos += size;
		return result;
	} else if (size == 0) {
		return @"";
	} else {
		// Slow path:  Build a byte array first then copy it.
		NSData* data = this->readRawData(size);
        return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
	}
}


/** Read a {@code bytes} field value from the stream. */
NSData* MiniCodedInputData::readData() {
	int32_t size = this->readRawVarint32();
	if (size < bufferSize - bufferPos && size > 0) {
		// Fast path:  We already have the bytes in a contiguous buffer, so
		//   just copy directly from it.
		NSData* result = [NSData dataWithBytes:(bufferPointer + bufferPos) length:size];
		bufferPos += size;
		return result;
	} else {
		// Slow path:  Build a byte array first then copy it.
		return this->readRawData(size);
	}
}


/** Read a {@code uint32} field value from the stream. */
uint32_t MiniCodedInputData::readUInt32() {
	return this->readRawVarint32();
}

// =================================================================

/**
 * Read a raw Varint from the stream.  If larger than 32 bits, discard the
 * upper bits.
 */
int32_t MiniCodedInputData::readRawVarint32() {
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
					//NSLog(@"InvalidProtocolBuffer-malformedVarint");
					//return -1;
					@throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:@"malformedVarint" userInfo:nil];
                    return -1;
				}
			}
		}
	}
	return result;
}


/** Read a raw Varint from the stream. */
int64_t MiniCodedInputData::readRawVarint64() {
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
	//NSLog(@"InvalidProtocolBuffer-malformedVarint");
	//return -1;
	@throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:@"malformedVarint" userInfo:nil];
    return -1;
}


/** Read a 32-bit little-endian integer from the stream. */
int32_t MiniCodedInputData::readRawLittleEndian32() {
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
int64_t MiniCodedInputData::readRawLittleEndian64() {
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
 * Set the maximum message size.  In order to prevent malicious
 * messages from exhausting memory or causing integer overflows,
 * {@code MiniCodedInputData} limits how large a message may be.
 * The default limit is 64MB.  You should set this limit as small
 * as you can without harming your app's functionality.  Note that
 * size limits only apply when reading from an {@code InputStream}, not
 * when constructed around a raw byte array.
 *
 * @return the old limit.
 */
int32_t MiniCodedInputData::setSizeLimit(int32_t limit) {
	if (limit < 0) {
		//NSLog(@"InvalidProtocolBuffer-size limit cannot be negative");
		//return -1;
		@throw [NSException exceptionWithName:@"IllegalArgument" reason:@"Size limit cannot be negative:" userInfo:nil];
        return -1;
	}
	int32_t oldLimit = sizeLimit;
	sizeLimit = limit;
	return oldLimit;
}

/**
 * Sets {@code currentLimit} to (current position) + {@code byteLimit}.  This
 * is called when descending into a length-delimited embedded message.
 *
 * @return the old limit.
 */
int32_t MiniCodedInputData::pushLimit(int32_t byteLimit) {
	if (byteLimit < 0) {
		// NSLog(@"InvalidProtocolBuffer-negativeSize");
		//return -1;
        @throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:@"negativeSize" userInfo:nil];
        return -1;
	}
	byteLimit +=  bufferPos;
	int32_t oldLimit = currentLimit;
	if (byteLimit > oldLimit) {
		//NSLog(@"InvalidProtocolBuffer-truncatedMessage");
		//return -1;
		@throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:@"truncatedMessage" userInfo:nil];
        return -1;
	}
	currentLimit = byteLimit;
	
	this->recomputeBufferSizeAfterLimit();
	
	return oldLimit;
}


void MiniCodedInputData::recomputeBufferSizeAfterLimit() {
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
void MiniCodedInputData::popLimit(int32_t oldLimit) {
	currentLimit = oldLimit;
	this->recomputeBufferSizeAfterLimit();
}


/**
 * Returns the number of bytes to be read before the current limit.
 * If no limit is set, returns -1.
 */
int32_t MiniCodedInputData::bytesUntilLimit() {
	if (currentLimit == INT_MAX) {
		return -1;
	}
	
	int32_t currentAbsolutePosition = bufferPos;
	return currentLimit - currentAbsolutePosition;
}

/**
 * Read one byte from the input.
 *
 * @throws InvalidProtocolBufferException The end of the stream or the current
 *                                        limit was reached.
 */
int8_t MiniCodedInputData::readRawByte() {
	if (bufferPos == bufferSize) {
        NSString *reason = [NSString stringWithFormat:@"reach end, bufferPos: %d, bufferSize: %d", bufferPos, bufferSize];
		@throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:reason userInfo:nil];
        return -1;
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
NSData* MiniCodedInputData::readRawData(int32_t size) {
	if (size < 0) {
		//NSLog(@"InvalidProtocolBuffer-negativeSize");
		//return nil;
        @throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:@"negativeSize" userInfo:nil];
        return nil;
	}
	
	if (bufferPos + size > currentLimit) {
		// Read to the end of the stream anyway.
		this->skipRawData(currentLimit - bufferPos);
		// Then fail.
		//NSLog(@"InvalidProtocolBuffer-truncatedMessage");
		//return nil;
		@throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:@"truncatedMessage" userInfo:nil];
        return nil;
	}
	
	if (size <= bufferSize - bufferPos) {
		// We have all the bytes we need already.
		NSData* data = [NSData dataWithBytes:(((int8_t*) bufferPointer) + bufferPos) length:size];
		bufferPos += size;
		return data;
	} else {
		@throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:@"truncatedMessage" userInfo:nil];
	}
	return nil;
}


/**
 * Reads and discards {@code size} bytes.
 *
 * @throws InvalidProtocolBufferException The end of the stream or the current
 *                                        limit was reached.
 */
void MiniCodedInputData::skipRawData(int32_t size) {
	if (size < 0) {
		//NSLog(@"InvalidProtocolBuffer-negativeSize");
		//return;
		@throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:@"negativeSize" userInfo:nil];
        return;
	}
	
	if (bufferPos + size > currentLimit) {
		// Read to the end of the stream anyway.
		this->skipRawData(currentLimit - bufferPos);
		// Then fail.
		//NSLog(@"InvalidProtocolBuffer-truncatedMessage");
		//return;
		@throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:@"truncatedMessage" userInfo:nil];
        return;
	}
	
	if (size <= (bufferSize - bufferPos)) {
		// We have all the bytes we need already.
		bufferPos += size;
	} else {
		@throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:@"truncatedMessage" userInfo:nil];
	}
}
