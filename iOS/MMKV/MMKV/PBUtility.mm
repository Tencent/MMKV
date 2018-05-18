//
//  PBUtility.mm
//  PBCoder
//
//  Created by Guo Ling on 4/26/13.
//  Copyright (c) 2013 Tencent. All rights reserved.
//

#import "PBUtility.h"
#import "WireFormat.h"


static const int32_t LITTLE_ENDIAN_32_SIZE = 4;
static const int32_t LITTLE_ENDIAN_64_SIZE = 8;


/**
 * Compute the number of bytes that would be needed to encode a
 * {@code double} field, including tag.
 */
int32_t computeDoubleSize(Float64 value) {
	return LITTLE_ENDIAN_64_SIZE;
}


/**
 * Compute the number of bytes that would be needed to encode a
 * {@code float} field, including tag.
 */
int32_t computeFloatSize(Float32 value) {
	return LITTLE_ENDIAN_32_SIZE;
}


/**
 * Compute the number of bytes that would be needed to encode a
 * {@code uint64} field, including tag.
 */
int32_t computeUInt64Size(int64_t value) {
	return computeRawVarint64Size(value);
}


/**
 * Compute the number of bytes that would be needed to encode an
 * {@code int64} field, including tag.
 */
int32_t computeInt64Size(int64_t value) {
	return computeRawVarint64Size(value);
}


/**
 * Compute the number of bytes that would be needed to encode an
 * {@code int32} field, including tag.
 */
int32_t computeInt32Size(int32_t value) {
	if (value >= 0) {
		return computeRawVarint32Size(value);
	} else {
		// Must sign-extend.
		return 10;
	}
}


/**
 * Compute the number of bytes that would be needed to encode a
 * {@code fixed32} field, including tag.
 */
int32_t computeFixed32Size(int32_t value) {
	return LITTLE_ENDIAN_32_SIZE;
}


/**
 * Compute the number of bytes that would be needed to encode a
 * {@code bool} field, including tag.
 */
int32_t computeBoolSize(BOOL value) {
	return 1;
}


/**
 * Compute the number of bytes that would be needed to encode a
 * {@code string} field, including tag.
 */
int32_t computeStringSize(NSString* value) {
	//  NSData* data = [value dataUsingEncoding:NSUTF8StringEncoding);
	//  return computeRawVarint32Size(data.length) + data.length;
	NSUInteger numberOfBytes = [value lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
	return int32_t(computeRawVarint32Size((int32_t)numberOfBytes) + numberOfBytes);
}

int32_t computeRawStringSize(NSString* value) {
	//  NSData* data = [value dataUsingEncoding:NSUTF8StringEncoding);
	//  return computeRawVarint32Size(data.length) + data.length;
	NSUInteger numberOfBytes = [value lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
	return (int32_t)numberOfBytes;
}


/**
 * Compute the number of bytes that would be needed to encode a
 * {@code bytes} field, including tag.
 */
int32_t computeDataSize(NSData* value) {
	return int32_t(computeRawVarint32Size((int32_t)value.length) + value.length);
}


/**
 * Compute the number of bytes that would be needed to encode a
 * {@code uint32} field, including tag.
 */
int32_t computeUInt32Size(int32_t value) {
	return computeRawVarint32Size(value);
}


/**
 * Compute the number of bytes that would be needed to encode a varint.
 * {@code value} is treated as unsigned, so it won't be sign-extended if
 * negative.
 */
int32_t computeRawVarint32Size(int32_t value) {
	if ((value & (0xffffffff <<  7)) == 0) return 1;
	if ((value & (0xffffffff << 14)) == 0) return 2;
	if ((value & (0xffffffff << 21)) == 0) return 3;
	if ((value & (0xffffffff << 28)) == 0) return 4;
	return 5;
}


/** Compute the number of bytes that would be needed to encode a varint. */
int32_t computeRawVarint64Size(int64_t value) {
	if ((value & (0xffffffffffffffffL <<  7)) == 0) return 1;
	if ((value & (0xffffffffffffffffL << 14)) == 0) return 2;
	if ((value & (0xffffffffffffffffL << 21)) == 0) return 3;
	if ((value & (0xffffffffffffffffL << 28)) == 0) return 4;
	if ((value & (0xffffffffffffffffL << 35)) == 0) return 5;
	if ((value & (0xffffffffffffffffL << 42)) == 0) return 6;
	if ((value & (0xffffffffffffffffL << 49)) == 0) return 7;
	if ((value & (0xffffffffffffffffL << 56)) == 0) return 8;
	if ((value & (0xffffffffffffffffL << 63)) == 0) return 9;
	return 10;
}
