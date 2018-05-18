//
// Created by Ling Guo on 2018/4/25.
//

#ifndef MMKV_WIREFORMAT_H
#define MMKV_WIREFORMAT_H

#include "PBUtility.h"

typedef enum {
    PBWireFormatVarint = 0,
    PBWireFormatFixed64 = 1,
    PBWireFormatLengthDelimited = 2,
    PBWireFormatFixed32 = 5,

    PBWireFormatTagTypeBits = 3,
    PBWireFormatTagTypeMask = 7 /* = (1 << PBWireFormatTagTypeBits) - 1*/,
} PBWireFormat;

#ifdef __cplusplus
extern "C" {
#endif

static inline int32_t PBWireFormatMakeTag(int32_t fieldNumber, int32_t wireType) {
    return (fieldNumber << PBWireFormatTagTypeBits) | wireType;
}


static inline int32_t PBWireFormatGetTagWireType(int32_t tag) {
    return tag & PBWireFormatTagTypeMask;
}


static inline int32_t PBWireFormatGetTagFieldNumber(int32_t tag) {
    return logicalRightShift32(tag, PBWireFormatTagTypeBits);
}

#ifdef __cplusplus
}
#endif


#endif //MMKV_WIREFORMAT_H
