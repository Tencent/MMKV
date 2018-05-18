//
//  CodedInputData.h
//  MicroMessenger
//
//  Created by Guo Ling on 4/26/13.
//  Copyright (c) 2013 Tencent. All rights reserved.
//

#ifdef __cplusplus

#import <Foundation/Foundation.h>

class CodedInputData {
	uint8_t* bufferPointer;
	int32_t bufferSize;
	int32_t bufferSizeAfterLimit;
	int32_t bufferPos;
	int32_t lastTag;
	
	/** The absolute position of the end of the current message. */
	int32_t currentLimit;
	
	/** See setRecursionLimit() */
	int32_t recursionDepth;
	int32_t recursionLimit;
	
	/** See setSizeLimit() */
	int32_t sizeLimit;

public:
	CodedInputData(NSData* oData);
	~CodedInputData();
	
	bool isAtEnd() { return bufferPos == bufferSize; };
	
	BOOL readBool();
	Float64 readDouble();
	Float32 readFloat();
	uint64_t readUInt64();
	uint32_t readUInt32();
	int64_t readInt64();
	int32_t readInt32();
	int32_t readFixed32();
	
	NSString* readString();
	NSData* readData();
	
	/**
	 * Read one byte from the input.
	 *
	 * @throws InvalidProtocolBuffer The end of the stream or the current
	 *                                        limit was reached.
	 */
	int8_t readRawByte();
	
	/**
	 * Read a raw Varint from the stream.  If larger than 32 bits, discard the
	 * upper bits.
	 */
	int32_t readRawVarint32();
	int64_t readRawVarint64();
	int32_t readRawLittleEndian32();
	int64_t readRawLittleEndian64();
	
	/**
	 * Read a fixed size of bytes from the input.
	 *
	 * @throws InvalidProtocolBuffer The end of the stream or the current
	 *                                        limit was reached.
	 */
	NSData* readRawData(int32_t size);
	
	BOOL skipField(int32_t tag);
	
	int32_t setSizeLimit(int32_t limit);
	int32_t pushLimit(int32_t byteLimit);
	void recomputeBufferSizeAfterLimit();
	void popLimit(int32_t oldLimit);
	int32_t bytesUntilLimit();
	
	int8_t readRawByte(int8_t* bufferPointer, int32_t* bufferPos, int32_t bufferSize);
	void skipRawData(int32_t size);
};

#endif
