//
//  CodedOutputData.mm
//  StorageProfile
//
//  Created by Ling Guo on 4/17/14.
//  Copyright (c) 2014 Tencent. All rights reserved.
//

#import "CodedOutputData.h"
#import "WireFormat.h"
#import "PBUtility.h"

CodedOutputData::CodedOutputData(void* ptr, size_t len) {
	bufferPointer = (uint8_t*)ptr;
	bufferLength = len;
	position = 0;
}

CodedOutputData::CodedOutputData(NSMutableData* oData) {
	bufferPointer = (uint8_t*)oData.mutableBytes;
	bufferLength = oData.length;
	position = 0;
}

CodedOutputData::~CodedOutputData() {
	bufferPointer = NULL;
	position = 0;
}

void CodedOutputData::writeDouble(Float64 value) {
	this->writeRawLittleEndian64(convertFloat64ToInt64(value));
}

void CodedOutputData::writeFloat(Float32 value) {
	this->writeRawLittleEndian32(convertFloat32ToInt32(value));
}

void CodedOutputData::writeUInt64(int64_t value) {
	this->writeRawVarint64(value);
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

void CodedOutputData::writeBool(BOOL value) {
	this->writeRawByte(value ? 1 : 0);
}

void CodedOutputData::writeString(NSString* value) {
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

void CodedOutputData::writeString(NSString* value, NSUInteger numberOfBytes) {
	[value getBytes:bufferPointer + position
		  maxLength:numberOfBytes
		 usedLength:0
		   encoding:NSUTF8StringEncoding
			options:0
			  range:NSMakeRange(0, value.length)
	 remainingRange:NULL];
	position += numberOfBytes;
}


void CodedOutputData::writeData(NSData* value) {
	this->writeRawVarint32((int32_t)value.length);
	this->writeRawData(value);
}

void CodedOutputData::writeUInt32(int32_t value) {
	this->writeRawVarint32(value);
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
		@throw [NSException exceptionWithName:@"OutOfSpace" reason:@"" userInfo:nil];
	}
}

/**
 * Verifies that {@link #spaceLeft()} returns zero.  It's common to create
 * a byte array that is exactly big enough to hold a message, then write to
 * it with a {@code WXPBCodedOutputStream}.  Calling {@code checkNoSpaceLeft()}
 * after writing verifies that the message was actually as big as expected,
 * which can help catch bugs.
 */
void CodedOutputData::checkNoSpaceLeft() {
	if (this->spaceLeft() != 0) {
		//NSLog(@"IllegalState-Did not write as much data as expected.");
		@throw [NSException exceptionWithName:@"IllegalState" reason:@"Did not write as much data as expected." userInfo:nil];
	}
}


/** Write a single byte. */
void CodedOutputData::writeRawByte(uint8_t value) {
	if (position == bufferLength) {
        NSString *reason = [NSString stringWithFormat:@"position: %d, bufferLength: %u", position, (unsigned int)bufferLength];
		@throw [NSException exceptionWithName:@"OutOfSpace" reason:reason userInfo:nil];
	}
	
	bufferPointer[position++] = value;
}


/** Write an array of bytes. */
void CodedOutputData::writeRawData(NSData* data) {
	this->writeRawData(data, 0, (int32_t)data.length);
}


void CodedOutputData::writeRawData(NSData* value, int32_t offset, int32_t length) {
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
void CodedOutputData::writeRawVarint32(int32_t value) {
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
void CodedOutputData::writeRawVarint64(int64_t value) {
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
