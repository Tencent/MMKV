//
//  MiniPBUtility.mm
//  PBCoder
//
//  Created by Guo Ling on 4/26/13.
//  Copyright (c) 2013 Tencent. All rights reserved.
//

#import "MiniPBUtility.h"

int32_t pbUInt64Size(int64_t value) {
	return pbRawVarint64Size(value);
}

int32_t pbInt64Size(int64_t value) {
	return pbRawVarint64Size(value);
}

int32_t pbInt32Size(int32_t value) {
	if (value >= 0) {
		return pbRawVarint32Size(value);
	} else {
		return 10;
	}
}

int32_t pbStringSize(NSString *value) {
	//  NSData* data = [value dataUsingEncoding:NSUTF8StringEncoding);
	//  return pbRawVarint32Size(data.length) + data.length;
	NSUInteger numberOfBytes = [value lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
	return int32_t(pbRawVarint32Size((int32_t) numberOfBytes) + numberOfBytes);
}

int32_t pbRawStringSize(NSString *value) {
	//  NSData* data = [value dataUsingEncoding:NSUTF8StringEncoding);
	//  return pbRawVarint32Size(data.length) + data.length;
	NSUInteger numberOfBytes = [value lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
	return (int32_t) numberOfBytes;
}

int32_t pbDataSize(NSData *value) {
	return int32_t(pbRawVarint32Size((int32_t) value.length) + value.length);
}

int32_t pbUInt32Size(int32_t value) {
	return pbRawVarint32Size(value);
}

int32_t pbRawVarint32Size(int32_t value) {
	if ((value & (0xffffffff << 7)) == 0)
		return 1;
	if ((value & (0xffffffff << 14)) == 0)
		return 2;
	if ((value & (0xffffffff << 21)) == 0)
		return 3;
	if ((value & (0xffffffff << 28)) == 0)
		return 4;
	return 5;
}

int32_t pbRawVarint64Size(int64_t value) {
	if ((value & (0xffffffffffffffffL << 7)) == 0)
		return 1;
	if ((value & (0xffffffffffffffffL << 14)) == 0)
		return 2;
	if ((value & (0xffffffffffffffffL << 21)) == 0)
		return 3;
	if ((value & (0xffffffffffffffffL << 28)) == 0)
		return 4;
	if ((value & (0xffffffffffffffffL << 35)) == 0)
		return 5;
	if ((value & (0xffffffffffffffffL << 42)) == 0)
		return 6;
	if ((value & (0xffffffffffffffffL << 49)) == 0)
		return 7;
	if ((value & (0xffffffffffffffffL << 56)) == 0)
		return 8;
	if ((value & (0xffffffffffffffffL << 63)) == 0)
		return 9;
	return 10;
}
