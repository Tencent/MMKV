//
//  CodedOutputData.h
//  PBCoder
//
//  Created by Ling Guo on 4/17/14.
//  Copyright (c 2014 Tencent. All rights reserved.
//

#ifdef __cplusplus

#import <Foundation/Foundation.h>

class CodedOutputData {
	uint8_t* bufferPointer;
	size_t bufferLength;
    int32_t position;
	
	void checkNoSpaceLeft();
	
public:
	CodedOutputData(void* ptr, size_t len);
	CodedOutputData(NSMutableData* odata);
	~CodedOutputData();
	
	int32_t spaceLeft();
	void seek(size_t addedSize);

	void writeRawByte(uint8_t value);

	void writeRawLittleEndian32(int32_t value);
	void writeRawLittleEndian64(int64_t value);
	
	/**
	 * Encode and write a varint.  value is treated as
	 * unsigned, so it won't be sign-extended if negative.
	 */
	void writeRawVarint32(int32_t value);
	void writeRawVarint64(int64_t value);
	
	void writeRawData(NSData* data);
	void writeRawData(NSData* data, int32_t offset, int32_t length);
	
	void writeData(int32_t fieldNumber, NSData* value);
	
	void writeDoubleNoTag(Float64 value);
	void writeFloatNoTag(Float32 value);
	void writeUInt64NoTag(int64_t value);
	void writeInt64NoTag(int64_t value);
	void writeInt32NoTag(int32_t value);
	void writeFixed32NoTag(int32_t value);
	void writeBoolNoTag(BOOL value);
	void writeStringNoTag(NSString* value);
	void writeStringNoTag(NSString* value, NSUInteger numberOfBytes);
	void writeDataNoTag(NSData* value);
	void writeUInt32NoTag(int32_t value);
};

#endif
