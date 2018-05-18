//
//  MiniPBCoder.mm
//  PBCoder
//
//  Created by Guo Ling on 4/14/13.
//  Copyright (c) 2013 Guo Ling. All rights reserved.
//
#import "MiniPBCoder.h"
#include <sys/stat.h>
#import <vector>
#import <string>
#import "WireFormat.h"
#import "PBEncodeItem.h"
#import "CodedInputData.h"
#import "CodedOutputData.h"

#define PBError(format, ...)	NSLog(format, ##__VA_ARGS__)
#define PBWarning(format, ...)	NSLog(format, ##__VA_ARGS__)
#define PBInfo(format, ...)		NSLog(format, ##__VA_ARGS__)
#define PBDebug(format, ...)	NSLog(format, ##__VA_ARGS__)

@implementation MiniPBCoder {
	NSObject* m_obj;
	
	BOOL m_isTopObject;
	NSData* m_inputData;
	CodedInputData* m_inputStream;
	
	NSMutableData* m_outputData;
	CodedOutputData* m_outputStream;
	std::vector<PBEncodeItem>* m_encodeItems;
	
	void* m_formatBuffer;
	size_t m_formatBufferSize;
}

- (id)initForReadingWithData:(NSData *)data {
	if (self = [super init]) {
		m_isTopObject = YES;
        m_inputData = data;
		m_inputStream = new CodedInputData(data);
	}
	return self;
}

-(id)initForWritingWithTarget:(NSObject*) obj {
	if (self = [super init]) {
        m_obj = obj;
	}
	return self;
}

-(void) dealloc {
	if (m_encodeItems) {
		delete m_encodeItems;
		m_encodeItems = NULL;
	}
	m_inputData = nil;
	m_outputData = nil;
	m_obj = nil;
    
	if (m_inputStream) {
		delete m_inputStream;
		m_inputStream = NULL;
	}
	if (m_outputStream) {
		delete m_outputStream;
		m_outputStream = NULL;
	}
	
	if (m_formatBuffer) {
		free(m_formatBuffer);
		m_formatBuffer = NULL;
		m_formatBufferSize = 0;
	}
}

#pragma mark - encode

// write object using prepared m_encodeItems[]
-(void) writeRootObject {
	for (size_t index = 0, total = m_encodeItems->size(); index < total; index++) {
		PBEncodeItem* encodeItem = &(*m_encodeItems)[index];
		switch (encodeItem->type) {
			case PBEncodeItemType_NSString:
			{
				m_outputStream->writeRawVarint32(encodeItem->valueSize);
				if (encodeItem->valueSize > 0 && encodeItem->value.tmpObjectValue != NULL) {
					m_outputStream->writeRawData((__bridge NSData*)encodeItem->value.tmpObjectValue);
				}
				break;
			}
			case PBEncodeItemType_NSData:
			{
				m_outputStream->writeRawVarint32(encodeItem->valueSize);
				if (encodeItem->valueSize > 0 && encodeItem->value.objectValue != NULL) {
					m_outputStream->writeRawData((__bridge  NSData*)encodeItem->value.objectValue);
				}
				break;
			}
			case PBEncodeItemType_NSDate:
			{
				NSDate* oDate = (__bridge NSDate*)encodeItem->value.objectValue;
				m_outputStream->writeDouble(oDate.timeIntervalSince1970);
				break;
			}
			case PBEncodeItemType_NSContainer:
			{
				m_outputStream->writeRawVarint32(encodeItem->valueSize);
				break;
			}
			case PBEncodeItemType_None:
			{
				PBError(@"%d", encodeItem->type);
				break;
			}
		}
	}
}

// prepare size and value
-(size_t) prepareObjectForEncode:(NSObject*)obj {
	if (!obj) {
		return m_encodeItems->size();
	}
	m_encodeItems->push_back(PBEncodeItem());
	PBEncodeItem* encodeItem = &(m_encodeItems->back());
	size_t index = m_encodeItems->size() - 1;
    
	if ([obj isKindOfClass:[NSString class]])
	{
		NSString* str = (NSString*) obj;
		encodeItem->type = PBEncodeItemType_NSString;
		size_t maxSize = MAX(1, [str maximumLengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
		if (m_formatBufferSize < maxSize) {
			m_formatBufferSize = maxSize;
			if (m_formatBuffer) {
				free(m_formatBuffer);
			}
			m_formatBuffer = malloc(m_formatBufferSize);
		}
		NSUInteger realSize = 0;
		[str getBytes:m_formatBuffer maxLength:maxSize usedLength:&realSize encoding:NSUTF8StringEncoding options:0 range:NSMakeRange(0, str.length) remainingRange:NULL];
		NSData* buffer = [NSData dataWithBytes:m_formatBuffer length:realSize];
		encodeItem->value.tmpObjectValue = (void*)CFBridgingRetain(buffer);
		encodeItem->valueSize = static_cast<int32_t>(buffer.length);
	}
	else if ([obj isKindOfClass:[NSDate class]])
	{
		NSDate* oDate = (NSDate*)obj;
		encodeItem->type = PBEncodeItemType_NSDate;
		encodeItem->value.objectValue = (__bridge void*)oDate;
		encodeItem->valueSize = computeDoubleSize(oDate.timeIntervalSince1970);
		encodeItem->compiledSize = encodeItem->valueSize;
		return index;	// double has fixed compilesize
	}
	else if ([obj isKindOfClass:[NSData class]])
	{
		NSData* oData = (NSData*)obj;
		encodeItem->type = PBEncodeItemType_NSData;
		encodeItem->value.objectValue = (__bridge void*)oData;
		encodeItem->valueSize = static_cast<int32_t>(oData.length);
	}
	else if ([obj isKindOfClass:[NSDictionary class]])
	{
		encodeItem->type = PBEncodeItemType_NSContainer;
		encodeItem->value.objectValue = NULL;
		
		[(NSDictionary*)obj enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL *stop) {
			NSString* nsKey = (NSString*)key;	// assume key is NSString
			if (nsKey.length <= 0 || value == nil) {
				return;
			}
#ifdef DEBUG
			if (![nsKey isKindOfClass:NSString.class]) {
				PBError(@"NSDictionary has key[%@], only NSString is allowed!", NSStringFromClass(nsKey.class));
			}
#endif
            
			size_t keyIndex = [self prepareObjectForEncode:key];
			if (keyIndex < m_encodeItems->size()) {
				size_t valueIndex = [self prepareObjectForEncode:value];
				if (valueIndex < m_encodeItems->size()) {
					(*m_encodeItems)[index].valueSize += (*m_encodeItems)[keyIndex].compiledSize;
					(*m_encodeItems)[index].valueSize += (*m_encodeItems)[valueIndex].compiledSize;
				} else {
					m_encodeItems->pop_back();	// pop key
				}
			}
		}];
		
		encodeItem = &(*m_encodeItems)[index];
	}
	else
	{
		m_encodeItems->pop_back();
		PBError(@"%@ not recognized as container", NSStringFromClass(obj.class));
		return m_encodeItems->size();
	}
	encodeItem->compiledSize = computeRawVarint32Size(encodeItem->valueSize) + encodeItem->valueSize;
    
	return index;
}

-(NSData*) getEncodeData {
	if (m_outputData != nil) {
		return m_outputData;
	}
    
	m_encodeItems = new std::vector<PBEncodeItem>();
	size_t index = [self prepareObjectForEncode:m_obj];
	PBEncodeItem* oItem = (index < m_encodeItems->size()) ? &(*m_encodeItems)[index] : NULL;
	if (oItem && oItem->compiledSize > 0) {
		// non-protobuf object(NSString/NSArray, etc) need to to write SIZE as well as DATA,
		// so compiledSize is used,
		m_outputData = [NSMutableData dataWithLength:oItem->compiledSize];
		m_outputStream = new CodedOutputData(m_outputData);

		[self writeRootObject];
		
		return m_outputData;
	}
    
	return nil;
}

+(NSData*) encodeDataWithObject:(NSObject*)obj {
	if (obj) {
		@try {
			MiniPBCoder* oCoder = [[MiniPBCoder alloc] initForWritingWithTarget:obj];
            NSData* oData = [oCoder getEncodeData];
			
			return oData;
		} @catch(NSException *exception) {
			PBError(@"%@", exception);
			return nil;
		}
	}
	return nil;
}

#pragma mark - decode

-(NSMutableDictionary*) decodeOneDictionaryOfValueClass:(Class)cls {
	m_isTopObject = NO;
	if (cls == NULL) {
		return nil;
	}
	
	NSMutableDictionary* dic = [NSMutableDictionary dictionary];
	
	int32_t length = m_inputStream->readRawVarint32();
	int32_t	limit = m_inputStream->pushLimit(static_cast<int32_t>(m_inputData.length) - computeRawVarint32Size(length));
	
	while (!m_inputStream->isAtEnd()) {
		NSString* nsKey = m_inputStream->readString();
		if (nsKey) {
			id value = [self decodeOneObject:nil ofClass:cls];
			[dic setObject:value forKey:nsKey];
		}
	}
	m_inputStream->popLimit(limit);
	
	return dic;
}

-(id) decodeOneObject:(id)obj ofClass:(Class)cls {
	if (!cls && !obj) {
		return nil;
	}
	if (!cls) {
		cls = [(NSObject*)obj class];
	}
    
	m_isTopObject = NO;
	
	if (cls == [NSString class]) {
		return m_inputStream->readString();
	} else if (cls == [NSMutableString class]) {
		return [NSMutableString stringWithString:m_inputStream->readString()];
	} else if (cls == [NSData class]) {
		return m_inputStream->readData();
	} else if (cls == [NSMutableData class]) {
		return [NSMutableData dataWithData:m_inputStream->readData()];
	} else if (cls == [NSDate class]) {
		return [NSDate dateWithTimeIntervalSince1970:m_inputStream->readDouble()];
	} else {
		PBError(@"%@ does not respond -[getValueTypeTable] and no basic type, can't handle", NSStringFromClass(cls));
	}
	
	return nil;
}

+(id) decodeObjectOfClass:(Class)cls fromData:(NSData*)oData {
	if (!cls || oData.length <= 0) {
		return nil;
	}
	
	id obj = nil;
	@try {
		MiniPBCoder* oCoder = [[MiniPBCoder alloc] initForReadingWithData:oData];
		obj = [oCoder decodeOneObject:nil ofClass:cls];
	} @catch(NSException *exception) {
		PBError(@"%@", exception);
	}
	return obj;
}

+(id) decodeContainerOfClass:(Class)cls withValueClass:(Class)valueClass fromData:(NSData*)oData {
	if (!cls || !valueClass || oData.length <= 0) {
		return nil;
	}
    
	id obj = nil;
	@try {
		MiniPBCoder* oCoder = [[MiniPBCoder alloc] initForReadingWithData:oData];
		if (cls == [NSDictionary class] || cls == [NSMutableDictionary class]) {
			obj = [oCoder decodeOneDictionaryOfValueClass:valueClass];
		} else {
			PBError(@"%@ not recognized as container", NSStringFromClass(cls));
		}
	} @catch(NSException *exception) {
		PBError(@"%@", exception);
	}
	return obj;
}

@end
