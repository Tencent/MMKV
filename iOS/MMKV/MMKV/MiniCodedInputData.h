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

class MiniCodedInputData {
	uint8_t *m_ptr;
	int32_t m_size;
	int32_t m_position;

	int8_t readRawByte();

	int32_t readRawVarint32();

	int64_t readRawVarint64();

	int32_t readRawLittleEndian32();

	int64_t readRawLittleEndian64();

public:
	MiniCodedInputData(NSData *oData);

	~MiniCodedInputData();

	bool isAtEnd() { return m_position == m_size; };

	BOOL readBool();

	Float64 readDouble();

	Float32 readFloat();

	uint64_t readUInt64();

	uint32_t readUInt32();

	int64_t readInt64();

	int32_t readInt32();

	int32_t readFixed32();

	NSString *readString();

	NSData *readData();
};

#endif
