//
//  MiniCodedInputData.h
//  MicroMessenger
//
//  Created by Guo Ling on 4/26/13.
//  Copyright (c) 2013 Tencent. All rights reserved.
//

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
