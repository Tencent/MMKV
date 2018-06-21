//
//  MiniPBEncodeItem.h
//  PBCoder
//
//  Created by Guo Ling on 4/19/13.
//  Copyright (c) 2013 Guo Ling. All rights reserved.
//

#import <Foundation/Foundation.h>

#ifdef __cplusplus

enum MiniPBEncodeItemType {
    PBEncodeItemType_None,
    PBEncodeItemType_NSString,
    PBEncodeItemType_NSData,
    PBEncodeItemType_NSDate,
    PBEncodeItemType_NSContainer,
};

struct MiniPBEncodeItem {
    MiniPBEncodeItemType type;
    int32_t compiledSize;
    int32_t valueSize;
    union {
        void *objectValue;
        void *tmpObjectValue; // this object should release on dealloc
    } value;

    MiniPBEncodeItem() : type(PBEncodeItemType_None), compiledSize(0), valueSize(0) {
        memset(&value, 0, sizeof(value));
    }

    MiniPBEncodeItem(const MiniPBEncodeItem &other)
        : type(other.type)
        , compiledSize(other.compiledSize)
        , valueSize(other.valueSize)
        , value(other.value) {
        if (type == PBEncodeItemType_NSString) {
            if (value.tmpObjectValue != nullptr) {
                CFRetain(value.tmpObjectValue);
            }
        }
    }

    MiniPBEncodeItem &operator=(const MiniPBEncodeItem &other) {
        type = other.type;
        compiledSize = other.compiledSize;
        valueSize = other.valueSize;
        value = other.value;

        if (type == PBEncodeItemType_NSString) {
            if (value.tmpObjectValue != nullptr) {
                CFRetain(value.tmpObjectValue);
            }
        }

        return *this;
    }

    ~MiniPBEncodeItem() {
        if (type == PBEncodeItemType_NSString) {
            if (value.tmpObjectValue != nullptr) {
                CFRelease(value.tmpObjectValue);
                value.tmpObjectValue = nullptr;
            }
        }
    }
};

#endif
