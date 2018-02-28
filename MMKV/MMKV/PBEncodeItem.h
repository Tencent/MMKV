//
//  PBEncodeItem.h
//  PBCoder
//
//  Created by Guo Ling on 4/19/13.
//  Copyright (c) 2013 Guo Ling. All rights reserved.
//

#import <Foundation/Foundation.h>

#ifdef __cplusplus

enum PBEncodeItemType {
	PBEncodeItemType_None,
	PBEncodeItemType_NSString,
	PBEncodeItemType_NSData,
	PBEncodeItemType_NSDate,
	PBEncodeItemType_NSContainer,
};

struct PBEncodeItem
{
	PBEncodeItemType type;
	int32_t compiledSize;
	int32_t valueSize;
	union {
		void* objectValue;
		void* tmpObjectValue;	// this object should release on dealloc
	} value;
	
	PBEncodeItem()
		:type(PBEncodeItemType_None), compiledSize(0), valueSize(0)
	{
		memset(&value, 0, sizeof(value));
	}
	
	PBEncodeItem(const PBEncodeItem& other)
		:type(other.type), compiledSize(other.compiledSize), valueSize(other.valueSize), value(other.value)
	{
		if (type == PBEncodeItemType_NSString) {
			if (value.tmpObjectValue != NULL) {
				CFRetain(value.tmpObjectValue);
			}
		}
	}
	
	PBEncodeItem& operator = (const PBEncodeItem& other)
	{
		type = other.type;
		compiledSize = other.compiledSize;
		valueSize = other.valueSize;
		value = other.value;

		if (type == PBEncodeItemType_NSString) {
			if (value.tmpObjectValue != NULL) {
				CFRetain(value.tmpObjectValue);
			}
		}

		return *this;
	}
	
	~PBEncodeItem()
	{
		// release tmpObjectValue, currently only NSNumber will generate it
		if (type == PBEncodeItemType_NSString) {
			if (value.tmpObjectValue != NULL) {
				CFRelease(value.tmpObjectValue);
				value.tmpObjectValue = NULL;
			}
		}
	}
};

#endif
