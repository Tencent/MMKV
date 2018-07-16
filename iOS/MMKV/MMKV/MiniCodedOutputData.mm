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

#import "MiniCodedOutputData.h"
#import "MiniPBUtility.h"

MiniCodedOutputData::MiniCodedOutputData(void *ptr, size_t len)
    : m_ptr((uint8_t *) ptr), m_size(len), m_position(0) {
}

MiniCodedOutputData::MiniCodedOutputData(NSMutableData *oData)
    : m_ptr((uint8_t *) oData.mutableBytes), m_size(oData.length), m_position(0) {
}

MiniCodedOutputData::~MiniCodedOutputData() {
	m_ptr = nullptr;
	m_position = 0;
}

void MiniCodedOutputData::writeDouble(Float64 value) {
	this->writeRawLittleEndian64(Float64ToInt64(value));
}

void MiniCodedOutputData::writeFloat(Float32 value) {
	this->writeRawLittleEndian32(Float32ToInt32(value));
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
		this->writeRawVarint64(value);
	}
}

void MiniCodedOutputData::writeFixed32(int32_t value) {
	this->writeRawLittleEndian32(value);
}

void MiniCodedOutputData::writeBool(BOOL value) {
	this->writeRawByte(value ? 1 : 0);
}

void MiniCodedOutputData::writeString(NSString *value) {
	NSUInteger numberOfBytes = [value lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
	this->writeRawVarint32((int32_t) numberOfBytes);
	[value getBytes:m_ptr + m_position
	         maxLength:numberOfBytes
	        usedLength:0
	          encoding:NSUTF8StringEncoding
	           options:0
	             range:NSMakeRange(0, value.length)
	    remainingRange:nullptr];
	m_position += numberOfBytes;
}

void MiniCodedOutputData::writeData(NSData *value) {
	this->writeRawVarint32((int32_t) value.length);
	this->writeRawData(value);
}

void MiniCodedOutputData::writeUInt32(uint32_t value) {
	this->writeRawVarint32(value);
}

int32_t MiniCodedOutputData::spaceLeft() {
	return int32_t(m_size - m_position);
}

void MiniCodedOutputData::seek(size_t addedSize) {
	m_position += addedSize;

	if (m_position > m_size) {
		@throw [NSException exceptionWithName:@"OutOfSpace" reason:@"" userInfo:nil];
	}
}

void MiniCodedOutputData::writeRawByte(uint8_t value) {
	if (m_position == m_size) {
		NSString *reason = [NSString stringWithFormat:@"position: %d, bufferLength: %u", m_position, (unsigned int) m_size];
		@throw [NSException exceptionWithName:@"OutOfSpace" reason:reason userInfo:nil];
	}

	m_ptr[m_position++] = value;
}

void MiniCodedOutputData::writeRawData(NSData *data) {
	this->writeRawData(data, 0, (int32_t) data.length);
}

void MiniCodedOutputData::writeRawData(NSData *value, int32_t offset, int32_t length) {
	if (length <= 0) {
		return;
	}
	if (m_size - m_position >= length) {
		memcpy(m_ptr + m_position, ((uint8_t *) value.bytes) + offset, length);
		m_position += length;
	} else {
		[NSException exceptionWithName:@"Space" reason:@"too much data than calc" userInfo:nil];
	}
}

void MiniCodedOutputData::writeRawVarint32(int32_t value) {
	while (YES) {
		if ((value & ~0x7f) == 0) {
			this->writeRawByte(value);
			return;
		} else {
			this->writeRawByte((value & 0x7f) | 0x80);
			value = logicalRightShift32(value, 7);
		}
	}
}

void MiniCodedOutputData::writeRawVarint64(int64_t value) {
	while (YES) {
		if ((value & ~0x7FL) == 0) {
			this->writeRawByte((int32_t) value);
			return;
		} else {
			this->writeRawByte(((int32_t) value & 0x7f) | 0x80);
			value = logicalRightShift64(value, 7);
		}
	}
}

void MiniCodedOutputData::writeRawLittleEndian32(int32_t value) {
	this->writeRawByte((value) &0xff);
	this->writeRawByte((value >> 8) & 0xff);
	this->writeRawByte((value >> 16) & 0xff);
	this->writeRawByte((value >> 24) & 0xff);
}

void MiniCodedOutputData::writeRawLittleEndian64(int64_t value) {
	this->writeRawByte((int32_t)(value) &0xff);
	this->writeRawByte((int32_t)(value >> 8) & 0xff);
	this->writeRawByte((int32_t)(value >> 16) & 0xff);
	this->writeRawByte((int32_t)(value >> 24) & 0xff);
	this->writeRawByte((int32_t)(value >> 32) & 0xff);
	this->writeRawByte((int32_t)(value >> 40) & 0xff);
	this->writeRawByte((int32_t)(value >> 48) & 0xff);
	this->writeRawByte((int32_t)(value >> 56) & 0xff);
}
