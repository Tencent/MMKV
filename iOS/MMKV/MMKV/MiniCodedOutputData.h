//
//  MiniCodedOutputData.h
//  PBCoder
//
//  Created by Ling Guo on 4/17/14.
//  Copyright (c 2014 Tencent. All rights reserved.
//

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
