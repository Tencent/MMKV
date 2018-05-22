//
//  MiniCodedOutputData.mm
//  StorageProfile
//
//  Created by Ling Guo on 4/17/14.
//  Copyright (c) 2014 Tencent. All rights reserved.
//

#import "MiniCodedOutputData.h"
#import "MiniWireFormat.h"
#import "MiniPBUtility.h"

MiniCodedOutputData::MiniCodedOutputData(void* ptr, size_t len) {
	bufferPointer = (uint8_t*)ptr;
	bufferLength = len;
	position = 0;
}

MiniCodedOutputData::MiniCodedOutputData(NSMutableData* oData) {
	bufferPointer = (uint8_t*)oData.mutableBytes;
	bufferLength = oData.length;
	position = 0;
}

MiniCodedOutputData::~MiniCodedOutputData() {
	bufferPointer = NULL;
	position = 0;
}

void MiniCodedOutputData::writeDouble(Float64 value) {
	this->writeRawLittleEndian64(convertFloat64ToInt64(value));
}

void MiniCodedOutputData::writeFloat(Float32 value) {
	this->writeRawLittleEndian32(convertFloat32ToInt32(value));
}

void MiniCodedOutputData::writeUInt64(uint64_t value) {
	this->writeRawVarint64(value);
}

void MiniCodedOutputData::writeInt64(int64_t value) {
	this->writeRawVarint64(value);
}

void MiniCodedOutputData::writeInt32(int32_t value) {
	if (value >= 0) {
		this->writeRawVarint32(value);
	} else {
		// Must sign-extend
		this->writeRawVarint64(value);
	}
}

void MiniCodedOutputData::writeFixed32(int32_t value) {
	this->writeRawLittleEndian32(value);
}

void MiniCodedOutputData::writeBool(BOOL value) {
	this->writeRawByte(value ? 1 : 0);
}

void MiniCodedOutputData::writeString(NSString* value) {
	NSUInteger numberOfBytes = [value lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
	this->writeRawVarint32((int32_t)numberOfBytes);
	//	memcpy(bufferPointer + position, ((uint8_t*)value.bytes), numberOfBytes);
	[value getBytes:bufferPointer + position
		  maxLength:numberOfBytes
		 usedLength:0
		   encoding:NSUTF8StringEncoding
			options:0
			  range:NSMakeRange(0, value.length)
	 remainingRange:NULL];
	position += numberOfBytes;
}

void MiniCodedOutputData::writeString(NSString* value, NSUInteger numberOfBytes) {
	[value getBytes:bufferPointer + position
		  maxLength:numberOfBytes
		 usedLength:0
		   encoding:NSUTF8StringEncoding
			options:0
			  range:NSMakeRange(0, value.length)
	 remainingRange:NULL];
	position += numberOfBytes;
}


void MiniCodedOutputData::writeData(NSData* value) {
	this->writeRawVarint32((int32_t)value.length);
	this->writeRawData(value);
}

void MiniCodedOutputData::writeUInt32(uint32_t value) {
	this->writeRawVarint32(value);
}

/**
 * If writing to a flat array, return the space left in the array.
 * Otherwise, throws {@code UnsupportedOperationException}.
 */
int32_t MiniCodedOutputData::spaceLeft() {
	return int32_t(bufferLength - position);
}

void MiniCodedOutputData::seek(size_t addedSize) {
	position += addedSize;
	
	if (position > bufferLength) {
		@throw [NSException exceptionWithName:@"OutOfSpace" reason:@"" userInfo:nil];
	}
}


/** Write a single byte. */
void MiniCodedOutputData::writeRawByte(uint8_t value) {
	if (position == bufferLength) {
        NSString *reason = [NSString stringWithFormat:@"position: %d, bufferLength: %u", position, (unsigned int)bufferLength];
		@throw [NSException exceptionWithName:@"OutOfSpace" reason:reason userInfo:nil];
	}
	
	bufferPointer[position++] = value;
}


/** Write an array of bytes. */
void MiniCodedOutputData::writeRawData(NSData* data) {
	this->writeRawData(data, 0, (int32_t)data.length);
}


void MiniCodedOutputData::writeRawData(NSData* value, int32_t offset, int32_t length) {
	if (bufferLength - position >= length) {
		// We have room in the current buffer.
		memcpy(bufferPointer + position, ((uint8_t*)value.bytes) + offset, length);
		position += length;
	} else {
		[NSException exceptionWithName:@"Space" reason:@"too much data than calc" userInfo:nil];
	}
}

/**
 * Encode and write a varint.  {@code value} is treated as
 * unsigned, so it won't be sign-extended if negative.
 */
void MiniCodedOutputData::writeRawVarint32(int32_t value) {
	while (YES) {
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
void MiniCodedOutputData::writeRawVarint64(int64_t value) {
	while (YES) {
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
void MiniCodedOutputData::writeRawLittleEndian32(int32_t value) {
	this->writeRawByte((value      ) & 0xFF);
	this->writeRawByte((value >>  8) & 0xFF);
	this->writeRawByte((value >> 16) & 0xFF);
	this->writeRawByte((value >> 24) & 0xFF);
}


/** Write a little-endian 64-bit integer. */
void MiniCodedOutputData::writeRawLittleEndian64(int64_t value) {
	this->writeRawByte((int32_t)(value      ) & 0xFF);
	this->writeRawByte((int32_t)(value >>  8) & 0xFF);
	this->writeRawByte((int32_t)(value >> 16) & 0xFF);
	this->writeRawByte((int32_t)(value >> 24) & 0xFF);
	this->writeRawByte((int32_t)(value >> 32) & 0xFF);
	this->writeRawByte((int32_t)(value >> 40) & 0xFF);
	this->writeRawByte((int32_t)(value >> 48) & 0xFF);
	this->writeRawByte((int32_t)(value >> 56) & 0xFF);
}
