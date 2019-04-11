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

#import "MiniPBCoder.h"
#import "MMKVLog.h"
#import "MiniCodedInputData.h"
#import "MiniCodedOutputData.h"
#import "MiniPBEncodeItem.h"
#import "MiniPBUtility.h"
#import <string>
#import <sys/stat.h>
#import <vector>

@implementation MiniPBCoder {
	NSObject *m_obj;

	NSData *m_inputBuffer;
	MiniCodedInputData *m_inputData;

	NSMutableData *m_outputBuffer;
	MiniCodedOutputData *m_outputData;
	std::vector<MiniPBEncodeItem> *m_encodeItems;
}

- (id)initForReadingWithData:(NSData *)data {
	if (self = [super init]) {
		m_inputBuffer = data;
		m_inputData = new MiniCodedInputData(data);
	}
	return self;
}

- (id)initForWritingWithTarget:(NSObject *)obj {
	if (self = [super init]) {
		m_obj = obj;
	}
	return self;
}

- (void)dealloc {
	if (m_encodeItems) {
		delete m_encodeItems;
		m_encodeItems = nullptr;
	}
	m_inputBuffer = nil;
	m_outputBuffer = nil;
	m_obj = nil;

	if (m_inputData) {
		delete m_inputData;
		m_inputData = nullptr;
	}
	if (m_outputData) {
		delete m_outputData;
		m_outputData = nullptr;
	}
}

#pragma mark - encode

// write object using prepared m_encodeItems[]
- (void)writeRootObject {
	for (size_t index = 0, total = m_encodeItems->size(); index < total; index++) {
		MiniPBEncodeItem *encodeItem = &(*m_encodeItems)[index];
		switch (encodeItem->type) {
			case PBEncodeItemType_NSString: {
				m_outputData->writeRawVarint32(encodeItem->valueSize);
				if (encodeItem->valueSize > 0 && encodeItem->value.tmpObjectValue != nullptr) {
					m_outputData->writeRawData((__bridge NSData *) encodeItem->value.tmpObjectValue);
				}
				break;
			}
			case PBEncodeItemType_NSData: {
				m_outputData->writeRawVarint32(encodeItem->valueSize);
				if (encodeItem->valueSize > 0 && encodeItem->value.objectValue != nullptr) {
					m_outputData->writeRawData((__bridge NSData *) encodeItem->value.objectValue);
				}
				break;
			}
			case PBEncodeItemType_NSDate: {
				NSDate *oDate = (__bridge NSDate *) encodeItem->value.objectValue;
				m_outputData->writeDouble(oDate.timeIntervalSince1970);
				break;
			}
			case PBEncodeItemType_NSContainer: {
				m_outputData->writeRawVarint32(encodeItem->valueSize);
				break;
			}
			case PBEncodeItemType_None: {
				MMKVError(@"%d", encodeItem->type);
				break;
			}
		}
	}
}

// prepare size and value
- (size_t)prepareObjectForEncode:(NSObject *)obj {
	if (!obj) {
		return m_encodeItems->size();
	}
	m_encodeItems->push_back(MiniPBEncodeItem());
	MiniPBEncodeItem *encodeItem = &(m_encodeItems->back());
	size_t index = m_encodeItems->size() - 1;

	if ([obj isKindOfClass:[NSString class]]) {
		NSString *str = (NSString *) obj;
		encodeItem->type = PBEncodeItemType_NSString;
		NSData *buffer = [str dataUsingEncoding:NSUTF8StringEncoding];
		encodeItem->value.tmpObjectValue = (__bridge_retained void *) buffer;
		encodeItem->valueSize = static_cast<int32_t>(buffer.length);
	} else if ([obj isKindOfClass:[NSDate class]]) {
		NSDate *oDate = (NSDate *) obj;
		encodeItem->type = PBEncodeItemType_NSDate;
		encodeItem->value.objectValue = (__bridge void *) oDate;
		encodeItem->valueSize = pbDoubleSize(oDate.timeIntervalSince1970);
		encodeItem->compiledSize = encodeItem->valueSize;
		return index; // double has fixed compilesize
	} else if ([obj isKindOfClass:[NSData class]]) {
		NSData *oData = (NSData *) obj;
		encodeItem->type = PBEncodeItemType_NSData;
		encodeItem->value.objectValue = (__bridge void *) oData;
		encodeItem->valueSize = static_cast<int32_t>(oData.length);
	} else if ([obj isKindOfClass:[NSDictionary class]]) {
		encodeItem->type = PBEncodeItemType_NSContainer;
		encodeItem->value.objectValue = nullptr;

		[(NSDictionary *) obj enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL *stop) {
			NSString *nsKey = (NSString *) key; // assume key is NSString
			if (nsKey.length <= 0 || value == nil) {
				return;
			}
#ifdef DEBUG
			if (![nsKey isKindOfClass:NSString.class]) {
				MMKVError(@"NSDictionary has key[%@], only NSString is allowed!", NSStringFromClass(nsKey.class));
			}
#endif

			size_t keyIndex = [self prepareObjectForEncode:key];
			if (keyIndex < self->m_encodeItems->size()) {
				size_t valueIndex = [self prepareObjectForEncode:value];
				if (valueIndex < self->m_encodeItems->size()) {
					(*self->m_encodeItems)[index].valueSize += (*self->m_encodeItems)[keyIndex].compiledSize;
					(*self->m_encodeItems)[index].valueSize += (*self->m_encodeItems)[valueIndex].compiledSize;
				} else {
					self->m_encodeItems->pop_back(); // pop key
				}
			}
		}];

		encodeItem = &(*m_encodeItems)[index];
	} else {
		m_encodeItems->pop_back();
		MMKVError(@"%@ not recognized as container", NSStringFromClass(obj.class));
		return m_encodeItems->size();
	}
	encodeItem->compiledSize = pbRawVarint32Size(encodeItem->valueSize) + encodeItem->valueSize;

	return index;
}

- (NSData *)getEncodeData {
	if (m_outputBuffer != nil) {
		return m_outputBuffer;
	}

	m_encodeItems = new std::vector<MiniPBEncodeItem>();
	size_t index = [self prepareObjectForEncode:m_obj];
	MiniPBEncodeItem *oItem = (index < m_encodeItems->size()) ? &(*m_encodeItems)[index] : nullptr;
	if (oItem && oItem->compiledSize > 0) {
		// non-protobuf object(NSString/NSArray, etc) need to to write SIZE as well as DATA,
		// so compiledSize is used,
		m_outputBuffer = [NSMutableData dataWithLength:oItem->compiledSize];
		m_outputData = new MiniCodedOutputData(m_outputBuffer);

		[self writeRootObject];

		return m_outputBuffer;
	}

	return nil;
}

+ (NSData *)encodeDataWithObject:(NSObject *)obj {
	if (obj) {
		@try {
			MiniPBCoder *oCoder = [[MiniPBCoder alloc] initForWritingWithTarget:obj];
			NSData *oData = [oCoder getEncodeData];

			return oData;
		} @catch (NSException *exception) {
			MMKVError(@"%@", exception);
			return nil;
		}
	}
	return nil;
}

#pragma mark - decode

- (NSMutableDictionary *)decodeOneDictionaryOfValueClass:(Class)cls {
	if (cls == nullptr) {
		return nil;
	}

	NSMutableDictionary *dic = [NSMutableDictionary dictionary];

	m_inputData->readInt32();

	while (!m_inputData->isAtEnd()) {
		NSString *nsKey = m_inputData->readString();
		if (nsKey) {
			id value = [self decodeOneObject:nil ofClass:cls];
			if (value) {
				[dic setObject:value forKey:nsKey];
			} else {
				[dic removeObjectForKey:nsKey];
			}
		}
	}

	return dic;
}

- (id)decodeOneObject:(id)obj ofClass:(Class)cls {
	if (!cls && !obj) {
		return nil;
	}
	if (!cls) {
		cls = [(NSObject *) obj class];
	}

	if (cls == [NSString class]) {
		return m_inputData->readString();
	} else if (cls == [NSMutableString class]) {
		return [NSMutableString stringWithString:m_inputData->readString()];
	} else if (cls == [NSData class]) {
		return m_inputData->readData();
	} else if (cls == [NSMutableData class]) {
		return [NSMutableData dataWithData:m_inputData->readData()];
	} else if (cls == [NSDate class]) {
		return [NSDate dateWithTimeIntervalSince1970:m_inputData->readDouble()];
	} else {
		MMKVError(@"%@ does not respond -[getValueTypeTable] and no basic type, can't handle", NSStringFromClass(cls));
	}

	return nil;
}

+ (id)decodeObjectOfClass:(Class)cls fromData:(NSData *)oData {
	if (!cls || oData.length <= 0) {
		return nil;
	}

	id obj = nil;
	@try {
		MiniPBCoder *oCoder = [[MiniPBCoder alloc] initForReadingWithData:oData];
		obj = [oCoder decodeOneObject:nil ofClass:cls];
	} @catch (NSException *exception) {
		MMKVError(@"%@", exception);
	}
	return obj;
}

+ (id)decodeContainerOfClass:(Class)cls withValueClass:(Class)valueClass fromData:(NSData *)oData {
	if (!cls || !valueClass || oData.length <= 0) {
		return nil;
	}

	id obj = nil;
	@try {
		MiniPBCoder *oCoder = [[MiniPBCoder alloc] initForReadingWithData:oData];
		if (cls == [NSMutableDictionary class] || cls == [NSDictionary class]) {
			obj = [oCoder decodeOneDictionaryOfValueClass:valueClass];
		} else {
			MMKVError(@"%@ not recognized as container", NSStringFromClass(cls));
		}
	} @catch (NSException *exception) {
		MMKVError(@"%@", exception);
	}
	return obj;
}

#pragma mark -Helper

+ (BOOL)isMiniPBCoderCompatibleObject:(id)object {
	if ([object isKindOfClass:[NSString class]]) {
		return YES;
	}
	if ([object isKindOfClass:[NSData class]]) {
		return YES;
	}
	if ([object isKindOfClass:[NSDate class]]) {
		return YES;
	}

	return NO;
}

+ (BOOL)isMiniPBCoderCompatibleType:(Class)cls {
	if (cls == [NSString class]) {
		return YES;
	}
	if (cls == [NSMutableString class]) {
		return YES;
	}
	if (cls == [NSData class]) {
		return YES;
	}
	if (cls == [NSMutableData class]) {
		return YES;
	}
	if (cls == [NSDate class]) {
		return YES;
	}

	return NO;
}

@end
