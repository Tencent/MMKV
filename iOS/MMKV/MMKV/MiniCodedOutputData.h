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

#ifdef __cplusplus

#import <Foundation/Foundation.h>

class MiniCodedOutputData {
	uint8_t *m_ptr;
	size_t m_size;
	int32_t m_position;

	void writeRawByte(uint8_t value);

	void writeRawLittleEndian32(int32_t value);

	void writeRawLittleEndian64(int64_t value);

	void writeRawVarint64(int64_t value);

	void writeRawData(NSData *data, int32_t offset, int32_t length);

public:
	MiniCodedOutputData(void *ptr, size_t len);

	MiniCodedOutputData(NSMutableData *odata);

	~MiniCodedOutputData();

	int32_t spaceLeft();

	void seek(size_t addedSize);

	void writeBool(BOOL value);

	void writeRawVarint32(int32_t value);

	void writeInt32(int32_t value);

	void writeInt64(int64_t value);

	void writeFloat(Float32 value);

	void writeUInt32(uint32_t value);

	void writeUInt64(uint64_t value);

	void writeFixed32(int32_t value);

	void writeDouble(Float64 value);

	void writeString(NSString *value);

	void writeRawData(NSData *data);

	void writeData(NSData *value);
};

#endif
