/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2018 THL A29 Limited, a Tencent company.
 * All rights reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use
 * this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *       https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
