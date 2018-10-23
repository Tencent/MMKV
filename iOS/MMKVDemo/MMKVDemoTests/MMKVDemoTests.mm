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

#import <MMKV/MMKV.h>
#import <XCTest/XCTest.h>
#import <numeric>

using namespace std;

#define KeyNotExist @"KeyNotExist"

@interface MockNSCoding : NSObject <NSCoding>
@property NSString *string1;
@property NSString *string2;
@property NSUInteger integer;
@property NSSet *set;

- (BOOL)isEqualToObject:(MockNSCoding *)object;
@end

@implementation MockNSCoding

#pragma mark - NSCoding

- (id)initWithCoder:(NSCoder *)decoder {
	self = [super init];
	if (!self) {
		return nil;
	}

	self.string1 = [decoder decodeObjectForKey:@"string1"];
	self.string2 = [decoder decodeObjectForKey:@"string2"];
	self.integer = [decoder decodeIntegerForKey:@"integer"];
	self.set = [decoder decodeObjectForKey:@"set"];

	return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
	[encoder encodeObject:self.string1 forKey:@"string1"];
	[encoder encodeObject:self.string2 forKey:@"string2"];
	[encoder encodeInteger:self.integer forKey:@"integer"];
	[encoder encodeObject:self.set forKey:@"set"];
}

- (BOOL)isEqualToObject:(MockNSCoding *)object {
	return [self.string1 isEqualToString:object.string1] &&
	       [self.string2 isEqualToString:object.string2] &&
	       self.integer == object.integer &&
	       [self.set isEqualToSet:object.set];
}

@end

@interface MMKVDemoTests : XCTestCase

@end

@implementation MMKVDemoTests {
	MMKV *mmkv;
}

- (void)setUp {
	[super setUp];

	mmkv = [MMKV mmkvWithID:@"unit_test"];
}

- (void)tearDown {
	mmkv = nil;

	[super tearDown];
}

- (void)testBool {
	// This is an example of a functional test case.
	// Use XCTAssert and related functions to verify your tests produce the correct results.
	BOOL ret = [mmkv setBool:YES forKey:@"bool"];
	XCTAssertEqual(ret, YES);

	BOOL value = [mmkv getBoolForKey:@"bool"];
	XCTAssertEqual(value, YES);

	value = [mmkv getBoolForKey:KeyNotExist];
	XCTAssertEqual(value, NO);

	value = [mmkv getBoolForKey:KeyNotExist defaultValue:YES];
	XCTAssertEqual(value, YES);
}

- (void)testInt32 {
	// This is an example of a functional test case.
	// Use XCTAssert and related functions to verify your tests produce the correct results.
	BOOL ret = [mmkv setInt32:numeric_limits<int32_t>::max() forKey:@"int32"];
	XCTAssertEqual(ret, YES);

	int32_t value = [mmkv getInt32ForKey:@"int32"];
	XCTAssertEqual(value, numeric_limits<int32_t>::max());

	value = [mmkv getInt32ForKey:KeyNotExist];
	XCTAssertEqual(value, 0);

	value = [mmkv getInt32ForKey:KeyNotExist defaultValue:-1];
	XCTAssertEqual(value, -1);
}

- (void)testInt64 {
	// This is an example of a functional test case.
	// Use XCTAssert and related functions to verify your tests produce the correct results.
	BOOL ret = [mmkv setInt64:numeric_limits<int64_t>::max() forKey:@"int64"];
	XCTAssertEqual(ret, YES);

	int64_t value = [mmkv getInt64ForKey:@"int64"];
	XCTAssertEqual(value, numeric_limits<int64_t>::max());

	value = [mmkv getInt64ForKey:KeyNotExist];
	XCTAssertEqual(value, 0);

	value = [mmkv getInt64ForKey:KeyNotExist defaultValue:-1];
	XCTAssertEqual(value, -1);
}

- (void)testUInt32 {
	// This is an example of a functional test case.
	// Use XCTAssert and related functions to verify your tests produce the correct results.
	BOOL ret = [mmkv setUInt32:numeric_limits<uint32_t>::max() forKey:@"uint32"];
	XCTAssertEqual(ret, YES);

	uint32_t value = [mmkv getUInt32ForKey:@"uint32"];
	XCTAssertEqual(value, numeric_limits<uint32_t>::max());

	value = [mmkv getUInt32ForKey:KeyNotExist];
	XCTAssertEqual(value, 0);

	value = [mmkv getUInt32ForKey:KeyNotExist defaultValue:-1];
	XCTAssertEqual(value, -1);
}

- (void)testUInt64 {
	// This is an example of a functional test case.
	// Use XCTAssert and related functions to verify your tests produce the correct results.
	BOOL ret = [mmkv setUInt64:numeric_limits<uint64_t>::max() forKey:@"uint64"];
	XCTAssertEqual(ret, YES);

	uint64_t value = [mmkv getUInt64ForKey:@"uint64"];
	XCTAssertEqual(value, numeric_limits<uint64_t>::max());

	value = [mmkv getUInt64ForKey:KeyNotExist];
	XCTAssertEqual(value, 0);

	value = [mmkv getUInt64ForKey:KeyNotExist defaultValue:1024];
	XCTAssertEqual(value, 1024);
}

- (void)testFloat {
	// This is an example of a functional test case.
	// Use XCTAssert and related functions to verify your tests produce the correct results.
	BOOL ret = [mmkv setFloat:numeric_limits<float>::max() forKey:@"float"];
	XCTAssertEqual(ret, YES);

	float value = [mmkv getFloatForKey:@"float"];
	XCTAssertEqualWithAccuracy(value, numeric_limits<float>::max(), 0.001);

	value = [mmkv getFloatForKey:KeyNotExist];
	XCTAssertEqualWithAccuracy(value, 0, 0.001);

	value = [mmkv getFloatForKey:KeyNotExist defaultValue:-1];
	XCTAssertEqualWithAccuracy(value, -1, 0.001);
}

- (void)testDouble {
	// This is an example of a functional test case.
	// Use XCTAssert and related functions to verify your tests produce the correct results.
	BOOL ret = [mmkv setDouble:numeric_limits<double>::max() forKey:@"double"];
	XCTAssertEqual(ret, YES);

	double value = [mmkv getDoubleForKey:@"double"];
	XCTAssertEqualWithAccuracy(value, numeric_limits<double>::max(), 0.001);

	value = [mmkv getDoubleForKey:KeyNotExist];
	XCTAssertEqualWithAccuracy(value, 0, 0.001);

	value = [mmkv getDoubleForKey:KeyNotExist defaultValue:-1];
	XCTAssertEqualWithAccuracy(value, -1, 0.001);
}

- (void)testNSString {
	NSString *str = @"Hello 2018 world cup 世界杯";
	BOOL ret = [mmkv setObject:str forKey:@"string"];
	XCTAssertEqual(ret, YES);

	NSString *value = [mmkv getObjectOfClass:NSString.class forKey:@"string"];
	XCTAssertEqualObjects(value, str);

	value = [mmkv getObjectOfClass:NSString.class forKey:KeyNotExist];
	XCTAssertEqualObjects(value, nil);
}

- (void)testNSData {
	NSString *str = @"Hello 2018 world cup 世界杯";
	NSData *data = [str dataUsingEncoding:NSUTF8StringEncoding];
	BOOL ret = [mmkv setObject:data forKey:@"data"];
	XCTAssertEqual(ret, YES);

	NSData *value = [mmkv getObjectOfClass:NSData.class forKey:@"data"];
	XCTAssertEqualObjects(value, data);

	value = [mmkv getObjectOfClass:NSData.class forKey:KeyNotExist];
	XCTAssertEqualObjects(value, nil);
}

- (void)testNSDate {
	NSDate *date = [NSDate date];
	BOOL ret = [mmkv setObject:date forKey:@"date"];
	XCTAssertEqual(ret, YES);

	NSDate *value = [mmkv getObjectOfClass:NSDate.class forKey:@"date"];
	XCTAssertEqualWithAccuracy(date.timeIntervalSince1970, value.timeIntervalSince1970, 0.001);

	value = [mmkv getObjectOfClass:NSDate.class forKey:KeyNotExist];
	XCTAssertEqualObjects(value, nil);
}

- (void)testNSDictionary {
	NSDictionary *dic = @{@"key1" : @"value1",
		                  @"key2" : @(2)};
	BOOL ret = [mmkv setObject:dic forKey:@"dictionary"];
	XCTAssertTrue(ret);

	NSDictionary *value = [mmkv getObjectOfClass:[NSDictionary class] forKey:@"dictionary"];
	XCTAssertTrue([@"value1" isEqualToString:value[@"key1"]]);
	XCTAssertTrue(2 == [value[@"key2"] intValue]);
}

- (void)testNSMutableDictionary {
	NSMutableDictionary *dic = [@{@"key1" : @"value1",
		                          @"key2" : @(2)} mutableCopy];
	BOOL ret = [mmkv setObject:dic forKey:@"dictionary"];
	XCTAssertTrue(ret);

	NSDictionary *value = [mmkv getObjectOfClass:[NSDictionary class] forKey:@"dictionary"];
	XCTAssertTrue([@"value1" isEqualToString:value[@"key1"]]);
	XCTAssertTrue(2 == [value[@"key2"] intValue]);
}

- (void)testNSArray {
	NSArray *array = @[ @"0", @"1", @"2" ];
	BOOL ret = [mmkv setObject:array forKey:@"array"];
	XCTAssertTrue(ret);

	NSArray *value = [mmkv getObjectOfClass:[NSDictionary class] forKey:@"array"];
	XCTAssertTrue([@"0" isEqualToString:value[0]]);
	XCTAssertTrue([@"1" isEqualToString:value[1]]);
	XCTAssertTrue([@"2" isEqualToString:value[2]]);
}

- (void)testNSMutableArray {
	NSMutableArray *array = [@[ @"0", @"1", @"2" ] copy];
	BOOL ret = [mmkv setObject:array forKey:@"array"];
	XCTAssertTrue(ret);

	NSArray *value = [mmkv getObjectOfClass:[NSDictionary class] forKey:@"array"];
	XCTAssertTrue([@"0" isEqualToString:value[0]]);
	XCTAssertTrue([@"1" isEqualToString:value[1]]);
	XCTAssertTrue([@"2" isEqualToString:value[2]]);
}

- (void)testNSSet {
	NSSet *set = [NSSet setWithObjects:@1, @2, @3, @4, @4, nil];
	BOOL ret = [mmkv setObject:set forKey:@"set"];
	XCTAssertTrue(ret);

	NSSet *value = [mmkv getObjectOfClass:[NSSet class] forKey:@"set"];
	XCTAssertTrue([value isEqualToSet:set]);
}

- (void)testNSMutableSet {
	NSMutableSet *set = [NSMutableSet setWithObjects:@1, @2, @3, @4, @4, nil];
	BOOL ret = [mmkv setObject:set forKey:@"set"];
	XCTAssertTrue(ret);

	NSSet *value = [mmkv getObjectOfClass:[NSSet class] forKey:@"set"];
	XCTAssertTrue([value isEqualToSet:set]);
}

- (void)testCustomNSCodingObject {
	MockNSCoding *object = [[MockNSCoding alloc] init];
	object.string1 = @"hello";
	object.string2 = @"world";
	object.integer = 1024;
	object.set = [NSSet setWithObjects:@1, @2, @3, @4, @4, nil];

	BOOL ret = [mmkv setObject:object forKey:@"MockNSCoding"];
	XCTAssertTrue(ret);

	MockNSCoding *value = [mmkv getObjectOfClass:[MockNSCoding class] forKey:@"MockNSCoding"];
	XCTAssertTrue([value isEqualToObject:object]);
}

- (void)testCustomNSCodingObjectInArray {
	MockNSCoding *object1 = [[MockNSCoding alloc] init];
	object1.string1 = @"hello";
	object1.string2 = @"world";
	object1.integer = 1024;
	object1.set = [NSSet setWithObjects:@1, @2, @3, @4, @4, nil];

	MockNSCoding *object2 = [[MockNSCoding alloc] init];
	object2.string1 = @"hello";
	object2.string2 = @"100mango";
	object2.integer = 1023;
	object2.set = [NSSet setWithObjects:@1, @2, @3, @4, @4, nil];

	MockNSCoding *object3 = [[MockNSCoding alloc] init];
	object3.string1 = @"hello";
	object3.string2 = @"apple";
	object3.integer = 1023;
	object3.set = [NSSet setWithObjects:@1, @2, @3, @4, @4, nil];

	BOOL ret = [mmkv setObject:@[ object1, object2, object3 ] forKey:@"MockNSCoding"];
	XCTAssertTrue(ret);

	NSArray *value = [mmkv getObjectOfClass:[NSArray class] forKey:@"MockNSCoding"];
	XCTAssertTrue([value[0] isEqualToObject:object1]);
	XCTAssertTrue([value[1] isEqualToObject:object2]);
	XCTAssertTrue([value[2] isEqualToObject:object3]);
}

- (void)testRemove {
	BOOL ret = [mmkv setBool:YES forKey:@"bool_1"];
	ret &= [mmkv setInt32:numeric_limits<int32_t>::max() forKey:@"int_1"];
	ret &= [mmkv setInt64:numeric_limits<int64_t>::max() forKey:@"long_1"];
	ret &= [mmkv setFloat:numeric_limits<float>::min() forKey:@"float_1"];
	ret &= [mmkv setDouble:numeric_limits<double>::min() forKey:@"double_1"];
	ret &= [mmkv setObject:@"hello" forKey:@"string_1"];
	ret &= [mmkv setObject:@{@"key" : @"value"} forKey:@"dictionary"];
	XCTAssertEqual(ret, YES);

	{
		long count = mmkv.count;

		[mmkv removeValueForKey:@"bool_1"];
		[mmkv removeValuesForKeys:@[ @"int_1", @"long_1" ]];

		long newCount = mmkv.count;
		XCTAssertEqual(count, newCount + 3);
	}

	BOOL bValue = [mmkv getBoolForKey:@"bool_1"];
	XCTAssertEqual(bValue, NO);

	int32_t iValue = [mmkv getInt32ForKey:@"int_1"];
	XCTAssertEqual(iValue, 0);

	int64_t lValue = [mmkv getInt64ForKey:@"long_1"];
	XCTAssertEqual(lValue, 0);

	float fValue = [mmkv getFloatForKey:@"float_1"];
	XCTAssertEqualWithAccuracy(fValue, numeric_limits<float>::min(), 0.001);

	double dValue = [mmkv getDoubleForKey:@"double_1"];
	XCTAssertEqualWithAccuracy(dValue, numeric_limits<double>::min(), 0.001);

	NSString *sValue = [mmkv getObjectOfClass:NSString.class forKey:@"string_1"];
	XCTAssertEqualObjects(sValue, @"hello");
}

@end
