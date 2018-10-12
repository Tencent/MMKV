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
#import <zlib.h>
//#import "Crc32.h"

#if __has_feature(objc_arc)
#error This file must be compiled with MRC. Use -fno-objc-arc flag.
#endif

MiniCodedOutputData::MiniCodedOutputData(void *ptr, size_t len)
    : m_ptr((uint8_t *) ptr), m_size(len), m_position(0), m_memoryFile(nullptr), m_curSegment(nullptr) {
}

MiniCodedOutputData::MiniCodedOutputData(MMBuffer &oData)
    : m_ptr(oData.getPtr()), m_size(oData.length()), m_position(0), m_memoryFile(nullptr), m_curSegment(nullptr) {
}

MiniCodedOutputData::MiniCodedOutputData(MemoryFile *memoryFile, size_t offset)
    : m_ptr(nullptr), m_size(memoryFile->getFileSize()), m_position(offset), m_memoryFile(memoryFile), m_curSegment(memoryFile->tryGetSegment(0)) {
}

MiniCodedOutputData::~MiniCodedOutputData() {
	m_ptr = nullptr;
	m_position = 0;
	m_memoryFile = nullptr;
	m_curSegment = nullptr;
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

void MiniCodedOutputData::writeString(NSString *__unsafe_unretained value, size_t length) {
	if (m_position + length >= m_size) {
		NSString *reason = [NSString stringWithFormat:@"position: %zd, bufferLength: %zu", m_position, length];
		@throw [NSException exceptionWithName:@"OutOfSpace" reason:reason userInfo:nil];
	}

	this->writeRawVarint32((int32_t) length);
	auto ptr = m_ptr + m_position;
	auto ret = [value getBytes:ptr maxLength:length usedLength:nullptr encoding:NSUTF8StringEncoding options:0 range:NSMakeRange(0, value.length) remainingRange:nullptr];
	if (ret) {
		m_position += length;
	} else {
		NSString *reason = [NSString stringWithFormat:@"position: %zd, bufferLength: %zu", m_position, length];
		@throw [NSException exceptionWithName:@"Fail to Write String" reason:reason userInfo:nil];
	}
}

MMBuffer MiniCodedOutputData::writeString(__unsafe_unretained NSString *value, size_t length, uint32_t &crcDigest) {
	if (m_position + length >= m_size) {
		NSString *reason = [NSString stringWithFormat:@"position: %zd, bufferLength: %zu", m_position, length];
		@throw [NSException exceptionWithName:@"OutOfSpace" reason:reason userInfo:nil];
	}
	bool ret = false;
	auto oldPosition = m_position;
	MMBuffer result;

	this->writeRawVarint32((int32_t) length);
	if (m_memoryFile) {
		if (m_curSegment && m_curSegment->inside(m_position, length)) {
			auto ptr = m_curSegment->ptr + (m_position - m_curSegment->offset);
			ret = [value getBytes:ptr maxLength:length usedLength:nullptr encoding:NSUTF8StringEncoding options:0 range:NSMakeRange(0, value.length) remainingRange:nullptr];
			result.swap(MMBuffer(ptr, length));
		} else {
			auto data = [value dataUsingEncoding:NSUTF8StringEncoding];
			ret = m_memoryFile->write(m_position, data.bytes, data.length);
			result.swap(MMBuffer((void *) data.bytes, data.length));
		}
	}

	if (ret) {
		m_position += length;
		if (m_memoryFile) {
			if (m_curSegment && m_curSegment->inside(oldPosition, m_position - oldPosition)) {
				auto ptr = m_curSegment->ptr + (oldPosition - m_curSegment->offset);
				crcDigest = (uint32_t) crc32(crcDigest, ptr, (uInt)(m_position - oldPosition));
			} else {
				crcDigest = m_memoryFile->crc32(crcDigest, oldPosition, m_position - oldPosition);
			}
		}
	} else {
		NSString *reason = [NSString stringWithFormat:@"position: %zd, bufferLength: %zu", m_position, length];
		@throw [NSException exceptionWithName:@"Fail to Write String" reason:reason userInfo:nil];
	}
	return result;
}

void MiniCodedOutputData::writeData(const MMBuffer &value, uint32_t *crcDigest) {
	auto oldPosition = m_position;

	this->writeRawVarint32((int32_t) value.length());
	this->writeRawData(value);

	if (crcDigest) {
		if (m_memoryFile) {
			if (m_curSegment && m_curSegment->inside(oldPosition, m_position - oldPosition)) {
				auto ptr = m_curSegment->ptr + (oldPosition - m_curSegment->offset);
				*crcDigest = (uint32_t) crc32(*crcDigest, ptr, (uInt)(m_position - oldPosition));
			} else {
				*crcDigest = m_memoryFile->crc32(*crcDigest, oldPosition, m_position - oldPosition);
			}
		} else {
			assert(0);
		}
	}
}

void MiniCodedOutputData::writeUInt32(uint32_t value) {
	this->writeRawVarint32(value);
}

void MiniCodedOutputData::seek(size_t addedSize) {
	m_position += addedSize;

	if (m_position > m_size) {
		@throw [NSException exceptionWithName:@"OutOfSpace" reason:@"" userInfo:nil];
	}
}

void MiniCodedOutputData::writeRawByte(uint8_t value) {
	if (m_position == m_size) {
		NSString *reason = [NSString stringWithFormat:@"position: %zd, bufferLength: %u", m_position, (unsigned int) m_size];
		@throw [NSException exceptionWithName:@"OutOfSpace" reason:reason userInfo:nil];
	}
	if (m_memoryFile) {
		if (!m_curSegment || !m_curSegment->inside(m_position)) {
			auto index = m_memoryFile->offset2index(m_position);
			m_curSegment = m_memoryFile->tryGetSegment(index);
			if (!m_curSegment) {
				NSString *reason = [NSString stringWithFormat:@"fail to get segmemt at position %zd, index %d", m_position, index];
				@throw [NSException exceptionWithName:@"Invalid MemoryFile" reason:reason userInfo:nil];
				return;
			}
		}
		m_curSegment->ptr[m_position - m_curSegment->offset] = value;
		m_position++;
	} else {
		m_ptr[m_position++] = value;
	}
}

void MiniCodedOutputData::writeRawData(__unsafe_unretained NSData *data) {
	auto length = data.length;
	if (length <= 0) {
		return;
	}
	if (m_size - m_position >= length) {
		if (m_memoryFile) {
			if (m_curSegment && m_curSegment->inside(m_position, length)) {
				memcpy(m_curSegment->ptr + (m_position - m_curSegment->offset), data.bytes, length);
				m_position += length;
			} else {
				if (m_memoryFile->write(m_position, data.bytes, length)) {
					m_position += length;
				}
			}
		} else {
			memcpy(m_ptr + m_position, data.bytes, length);
			m_position += length;
		}
	} else {
		[NSException exceptionWithName:@"Space" reason:@"too much data than calc" userInfo:nil];
	}
}

void MiniCodedOutputData::writeRawData(const MMBuffer &data) {
	auto length = data.length();
	if (length <= 0) {
		return;
	}
	if (m_size - m_position >= length) {
		if (m_memoryFile) {
			if (m_curSegment && m_curSegment->inside(m_position, length)) {
				memcpy(m_curSegment->ptr + (m_position - m_curSegment->offset), data.getPtr(), length);
				m_position += length;
			} else {
				if (m_memoryFile->write(m_position, data.getPtr(), length)) {
					m_position += length;
				}
			}
		} else {
			memcpy(m_ptr + m_position, data.getPtr(), length);
			m_position += length;
		}
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
