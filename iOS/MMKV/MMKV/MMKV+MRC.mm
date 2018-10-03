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

#import "MMKV+Internal.h"

#if __has_feature(objc_arc)
#error This file must be compiled with MRC. Use -fno-objc-arc flag.
#endif

using namespace std;

@implementation MMKV (MRC)

#pragma mark - set & get

- (BOOL)setObject:(id __unsafe_unretained)obj forKey:(NSString *__unsafe_unretained)key {
	if (obj == nil || key.length <= 0) {
		return FALSE;
	}
	auto data = [MiniPBCoder encodeDataWithObject:obj];
	return [self setData:data forKey:key];
}

- (BOOL)setBool:(BOOL)value forKey:(NSString *__unsafe_unretained)key {
	if (key.length <= 0) {
		return FALSE;
	}
	size_t size = pbBoolSize(value);
	MMBuffer data(size);
	MiniCodedOutputData output(data);
	output.writeBool(value);

	return [self setData:data forKey:key];
}

- (BOOL)setUInt32:(uint32_t)value forKey:(NSString *__unsafe_unretained)key {
	if (key.length <= 0) {
		return FALSE;
	}
	size_t size = pbUInt32Size(value);
	MMBuffer data(size);
	MiniCodedOutputData output(data);
	output.writeUInt32(value);

	return [self setData:data forKey:key];
}

- (BOOL)setInt64:(int64_t)value forKey:(NSString *__unsafe_unretained)key {
	if (key.length <= 0) {
		return FALSE;
	}
	size_t size = pbInt64Size(value);
	MMBuffer data(size);
	MiniCodedOutputData output(data);
	output.writeInt64(value);

	return [self setData:data forKey:key];
}

- (BOOL)setUInt64:(uint64_t)value forKey:(NSString *__unsafe_unretained)key {
	if (key.length <= 0) {
		return FALSE;
	}
	size_t size = pbUInt64Size(value);
	MMBuffer data(size);
	MiniCodedOutputData output(data);
	output.writeUInt64(value);

	return [self setData:data forKey:key];
}

- (BOOL)setFloat:(float)value forKey:(NSString *__unsafe_unretained)key {
	if (key.length <= 0) {
		return FALSE;
	}
	size_t size = pbFloatSize(value);
	MMBuffer data(size);
	MiniCodedOutputData output(data);
	output.writeFloat(value);

	return [self setData:data forKey:key];
}

- (BOOL)setDouble:(double)value forKey:(NSString *__unsafe_unretained)key {
	if (key.length <= 0) {
		return FALSE;
	}
	size_t size = pbDoubleSize(value);
	MMBuffer data(size);
	MiniCodedOutputData output(data);
	output.writeDouble(value);

	return [self setData:data forKey:key];
}

- (id)getObjectOfClass:(Class)cls forKey:(NSString *__unsafe_unretained)key {
	if (key.length <= 0) {
		return nil;
	}
	auto data = [self getDataForKey:key];
	if (data.length() > 0) {
		return [MiniPBCoder decodeObjectOfClass:cls fromData:data];
	}
	return nil;
}

- (BOOL)getBoolForKey:(NSString *__unsafe_unretained)key {
	return [self getBoolForKey:key defaultValue:FALSE];
}
- (BOOL)getBoolForKey:(NSString *__unsafe_unretained)key defaultValue:(BOOL)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	auto data = [self getDataForKey:key];
	if (data.length() > 0) {
		@try {
			MiniCodedInputData input(data);
			return input.readBool();
		} @catch (NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

- (uint32_t)getUInt32ForKey:(NSString *__unsafe_unretained)key {
	return [self getUInt32ForKey:key defaultValue:0];
}
- (uint32_t)getUInt32ForKey:(NSString *__unsafe_unretained)key defaultValue:(uint32_t)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	auto data = [self getDataForKey:key];
	if (data.length() > 0) {
		@try {
			MiniCodedInputData input(data);
			return input.readUInt32();
		} @catch (NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

- (int64_t)getInt64ForKey:(NSString *__unsafe_unretained)key {
	return [self getInt64ForKey:key defaultValue:0];
}
- (int64_t)getInt64ForKey:(NSString *__unsafe_unretained)key defaultValue:(int64_t)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	auto data = [self getDataForKey:key];
	if (data.length() > 0) {
		@try {
			MiniCodedInputData input(data);
			return input.readInt64();
		} @catch (NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

- (uint64_t)getUInt64ForKey:(NSString *__unsafe_unretained)key {
	return [self getUInt64ForKey:key defaultValue:0];
}
- (uint64_t)getUInt64ForKey:(NSString *__unsafe_unretained)key defaultValue:(uint64_t)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	auto data = [self getDataForKey:key];
	if (data.length() > 0) {
		@try {
			MiniCodedInputData input(data);
			return input.readUInt64();
		} @catch (NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

- (float)getFloatForKey:(NSString *__unsafe_unretained)key {
	return [self getFloatForKey:key defaultValue:0];
}
- (float)getFloatForKey:(NSString *__unsafe_unretained)key defaultValue:(float)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	auto data = [self getDataForKey:key];
	if (data.length() > 0) {
		@try {
			MiniCodedInputData input(data);
			return input.readFloat();
		} @catch (NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

- (double)getDoubleForKey:(NSString *__unsafe_unretained)key {
	return [self getDoubleForKey:key defaultValue:0];
}
- (double)getDoubleForKey:(NSString *__unsafe_unretained)key defaultValue:(double)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	auto data = [self getDataForKey:key];
	if (data.length() > 0) {
		@try {
			MiniCodedInputData input(data);
			return input.readDouble();
		} @catch (NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

- (BOOL)setInt32:(int32_t)value forKey:(NSString *__unsafe_unretained)key {
	if (key.length <= 0) {
		return FALSE;
	}
	size_t size = pbInt32Size(value);
	MMBuffer data(size);
	MiniCodedOutputData output(data);
	output.writeInt32(value);

	return [self setData:data forKey:key];
}

- (int32_t)getInt32ForKey:(NSString *__unsafe_unretained)key {
	return [self getInt32ForKey:key defaultValue:0];
}
- (int32_t)getInt32ForKey:(NSString *__unsafe_unretained)key defaultValue:(int32_t)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	auto data = [self getDataForKey:key];
	if (data.length() > 0) {
		@try {
			MiniCodedInputData input(data);
			return input.readInt32();
		} @catch (NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

- (void)removeValueForKey:(NSString *__unsafe_unretained)key {
	if (key.length <= 0) {
		return;
	}
	CScopedLock lock(m_lock);
	[self checkLoadData];
	m_kvItemsWrap.erase(key);

	static MMBuffer data(0);
	[self appendData:data forKey:key];
}

- (void)removeValuesForKeys:(NSArray *__unsafe_unretained)arrKeys {
	if (arrKeys.count <= 0) {
		return;
	}
	CScopedLock lock(m_lock);
	[self checkLoadData];
	for (NSString *key in arrKeys) {
		m_kvItemsWrap.erase(key);
	}

	MMKVInfo(@"remove [%@] %lu keys, %zu remain", m_mmapID, (unsigned long) arrKeys.count, m_kvItemsWrap.size());

	[self fullWriteback];
}

#pragma mark - detail operation

- (BOOL)setData:(MMBuffer &)data forKey:(NSString *__unsafe_unretained)key {
	if (data.length() <= 0 || key.length <= 0) {
		return NO;
	}
	CScopedLock lock(m_lock);

	auto kvPair = [self appendData:data forKey:key];
	if (kvPair.first.length() > 0) {
		m_kvItemsWrap.emplace(std::move(kvPair.first), std::move(kvPair.second));
		return YES;
	}

	return NO;
}


// TODO: encryption
- (pair<KeyHolder, KeyValueHolder>)appendData:(MMBuffer &)data forKey:(NSString *__unsafe_unretained)key {
	auto ptr = (char *) key.UTF8String;
	KeyHolder keyHolder(ptr, strlen(ptr));
	auto keyLength = keyHolder.length();
	KeyValueHolder kvHolder = {0};
	kvHolder.keySize = static_cast<uint32_t>(keyLength + pbRawVarint32Size((int32_t) keyLength) + pbRawVarint32Size((int32_t) data.length()));
	kvHolder.valueSize = static_cast<uint32_t>(data.length());
	auto size = kvHolder.keySize + kvHolder.valueSize;

	auto hasEnoughSize = [self ensureMemorySize:size];
	if (hasEnoughSize == NO || [self isFileValid] == NO) {
		return make_pair(KeyHolder(), kvHolder);
	}

	bool isFirstWrite = false;
	if (m_actualSize == 0) {
		isFirstWrite = true;
		size += ItemSizeHolderSize;
		m_output->writeInt32(ItemSizeHolder);
	}
	if ([self writeAcutalSize:m_actualSize + size]) {
		// TODO: background mlock
		m_output->writeData(keyHolder, &m_crcDigest);
		m_output->writeData(data, &m_crcDigest);

		[self writeBackCRCDigest];

		auto offset = Fixed32Size + (m_actualSize - size);
		kvHolder.offset = static_cast<uint32_t>(isFirstWrite ? offset + ItemSizeHolderSize : offset);
		return make_pair(std::move(keyHolder), kvHolder);
	}
	return make_pair(KeyHolder(), kvHolder);
}

- (MMBuffer)getDataForKey:(NSString *__unsafe_unretained)key {
	CScopedLock lock(m_lock);
	[self checkLoadData];
	auto kvHolder = m_kvItemsWrap.find(key);
	if (!kvHolder) {
		return MMBuffer(0);
	}
	return m_memoryFile->read(kvHolder->offset + kvHolder->keySize, kvHolder->valueSize);
}

@end
