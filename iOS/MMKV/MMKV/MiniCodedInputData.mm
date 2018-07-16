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

#import "MiniCodedInputData.h"
#import "MiniPBUtility.h"

MiniCodedInputData::MiniCodedInputData(NSData *oData)
    : m_ptr((uint8_t *) oData.bytes), m_size((int32_t) oData.length), m_position(0) {
}

MiniCodedInputData::~MiniCodedInputData() {
	m_ptr = nullptr;
	m_size = 0;
}

Float64 MiniCodedInputData::readDouble() {
	return Int64ToFloat64(this->readRawLittleEndian64());
}

Float32 MiniCodedInputData::readFloat() {
	return Int32ToFloat32(this->readRawLittleEndian32());
}

uint64_t MiniCodedInputData::readUInt64() {
	return this->readRawVarint64();
}

int64_t MiniCodedInputData::readInt64() {
	return this->readRawVarint64();
}

int32_t MiniCodedInputData::readInt32() {
	return this->readRawVarint32();
}

int32_t MiniCodedInputData::readFixed32() {
	return this->readRawLittleEndian32();
}

BOOL MiniCodedInputData::readBool() {
	return this->readRawVarint32() != 0;
}

NSString *MiniCodedInputData::readString() {
	int32_t size = this->readRawVarint32();
	if (size <= (m_size - m_position) && size > 0) {
		NSString *result = [[NSString alloc] initWithBytes:(m_ptr + m_position)
		                                            length:size
		                                          encoding:NSUTF8StringEncoding];
		m_position += size;
		return result;
	} else if (size == 0) {
		return @"";
	} else if (size < 0) {
		@throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:@"negativeSize" userInfo:nil];
		return nil;
	} else {
		@throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:@"truncatedMessage" userInfo:nil];
	}
	return nil;
}

NSData *MiniCodedInputData::readData() {
	int32_t size = this->readRawVarint32();
	if (size <= m_size - m_position && size > 0) {
		NSData *result = [NSData dataWithBytes:(m_ptr + m_position) length:size];
		m_position += size;
		return result;
	} else if (size < 0) {
		@throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:@"negativeSize" userInfo:nil];
		return nil;
	}
	return nil;
}

uint32_t MiniCodedInputData::readUInt32() {
	return this->readRawVarint32();
}

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
					// discard upper 32 bits.
					for (int i = 0; i < 5; i++) {
						if (this->readRawByte() >= 0) {
							return result;
						}
					}
					@throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:@"malformedVarint" userInfo:nil];
					return -1;
				}
			}
		}
	}
	return result;
}

int64_t MiniCodedInputData::readRawVarint64() {
	int32_t shift = 0;
	int64_t result = 0;
	while (shift < 64) {
		int8_t b = this->readRawByte();
		result |= (int64_t)(b & 0x7f) << shift;
		if ((b & 0x80) == 0) {
			return result;
		}
		shift += 7;
	}
	@throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:@"malformedVarint" userInfo:nil];
	return -1;
}

int32_t MiniCodedInputData::readRawLittleEndian32() {
	int8_t b1 = this->readRawByte();
	int8_t b2 = this->readRawByte();
	int8_t b3 = this->readRawByte();
	int8_t b4 = this->readRawByte();
	return (((int32_t) b1 & 0xff)) |
	       (((int32_t) b2 & 0xff) << 8) |
	       (((int32_t) b3 & 0xff) << 16) |
	       (((int32_t) b4 & 0xff) << 24);
}

int64_t MiniCodedInputData::readRawLittleEndian64() {
	int8_t b1 = this->readRawByte();
	int8_t b2 = this->readRawByte();
	int8_t b3 = this->readRawByte();
	int8_t b4 = this->readRawByte();
	int8_t b5 = this->readRawByte();
	int8_t b6 = this->readRawByte();
	int8_t b7 = this->readRawByte();
	int8_t b8 = this->readRawByte();
	return (((int64_t) b1 & 0xff)) |
	       (((int64_t) b2 & 0xff) << 8) |
	       (((int64_t) b3 & 0xff) << 16) |
	       (((int64_t) b4 & 0xff) << 24) |
	       (((int64_t) b5 & 0xff) << 32) |
	       (((int64_t) b6 & 0xff) << 40) |
	       (((int64_t) b7 & 0xff) << 48) |
	       (((int64_t) b8 & 0xff) << 56);
}

int8_t MiniCodedInputData::readRawByte() {
	if (m_position == m_size) {
		NSString *reason = [NSString stringWithFormat:@"reach end, bufferPos: %d, bufferSize: %d", m_position, m_size];
		@throw [NSException exceptionWithName:@"InvalidProtocolBuffer" reason:reason userInfo:nil];
		return -1;
	}
	int8_t *bytes = (int8_t *) m_ptr;
	return bytes[m_position++];
}
