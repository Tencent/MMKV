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

- (void)testNSStringForNewGetSet {
    NSString *str = @"Hello 2018 world cup 世界杯";
    BOOL ret = [mmkv setString:str forKey:@"string"];
    XCTAssertEqual(ret, YES);

    NSString *value = [mmkv getStringForKey:@"string"];
    XCTAssertEqualObjects(value, str);

    value = [mmkv getStringForKey:KeyNotExist];
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

- (void)testNSDataForNewGetSet {
    NSString *str = @"Hello 2018 world cup 世界杯";
    NSData *data = [str dataUsingEncoding:NSUTF8StringEncoding];
    BOOL ret = [mmkv setData:data forKey:@"data"];
    XCTAssertEqual(ret, YES);

    NSData *value = [mmkv getDataForKey:@"data"];
    XCTAssertEqualObjects(value, data);

    value = [mmkv getDataForKey:KeyNotExist];
    XCTAssertEqualObjects(value, nil);
}

- (void)testNSDate {
    NSDate *date = [NSDate date];
    BOOL ret = [mmkv setObject:date forKey:@"date"];
    XCTAssertEqual(ret, YES);

    NSDate *value = [mmkv getObjectOfClass:NSDate.class forKey:@"date"];
    [self compareDate:date withDate:value];

    value = [mmkv getObjectOfClass:NSDate.class forKey:KeyNotExist];
    XCTAssertEqualObjects(value, nil);
}

- (void)testNSDateForNewGetSet {
    NSDate *date = [NSDate date];
    BOOL ret = [mmkv setDate:date forKey:@"date"];
    XCTAssertEqual(ret, YES);

    NSDate *value = [mmkv getDateForKey:@"date"];
    [self compareDate:date withDate:value];

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

- (void)compareDate:(NSDate *)date withDate:(NSDate *)other {
    XCTAssertEqualWithAccuracy(date.timeIntervalSince1970, other.timeIntervalSince1970, 0.001);
}

- (void)testImportFromNSUserDefaults {
    NSUserDefaults *userDefault = [[NSUserDefaults alloc] initWithSuiteName:@"testNSUserDefaults"];
    [userDefault setBool:YES forKey:@"bool"];
    [userDefault setInteger:std::numeric_limits<NSInteger>::max() forKey:@"NSInteger"];
    [userDefault setFloat:3.14 forKey:@"float"];
    [userDefault setDouble:std::numeric_limits<double>::max() forKey:@"double"];
    [userDefault setObject:@"hello, NSUserDefaults" forKey:@"string"];
    [userDefault setObject:[NSDate date] forKey:@"date"];
    [userDefault setObject:[@"hello, NSUserDefaults again" dataUsingEncoding:NSUTF8StringEncoding] forKey:@"data"];
    [userDefault setURL:[NSURL URLWithString:@"https://mail.qq.com"] forKey:@"url"];

    NSNumber *number = [NSNumber numberWithBool:YES];
    [userDefault setObject:number forKey:@"number_bool"];

    number = [NSNumber numberWithChar:std::numeric_limits<char>::min()];
    [userDefault setObject:number forKey:@"number_char"];

    number = [NSNumber numberWithUnsignedChar:std::numeric_limits<unsigned char>::max()];
    [userDefault setObject:number forKey:@"number_unsigned_char"];

    number = [NSNumber numberWithShort:std::numeric_limits<short>::min()];
    [userDefault setObject:number forKey:@"number_short"];

    number = [NSNumber numberWithUnsignedShort:std::numeric_limits<unsigned short>::max()];
    [userDefault setObject:number forKey:@"number_unsigned_short"];

    number = [NSNumber numberWithInt:std::numeric_limits<int>::min()];
    [userDefault setObject:number forKey:@"number_int"];

    number = [NSNumber numberWithUnsignedInt:std::numeric_limits<unsigned int>::max()];
    [userDefault setObject:number forKey:@"number_unsigned_int"];

    number = [NSNumber numberWithLong:std::numeric_limits<long>::min()];
    [userDefault setObject:number forKey:@"number_long"];

    number = [NSNumber numberWithUnsignedLong:std::numeric_limits<unsigned long>::max()];
    [userDefault setObject:number forKey:@"number_unsigned_long"];

    number = [NSNumber numberWithLongLong:std::numeric_limits<long long>::min()];
    [userDefault setObject:number forKey:@"number_long_long"];

    number = [NSNumber numberWithUnsignedLongLong:std::numeric_limits<unsigned long long>::max()];
    [userDefault setObject:number forKey:@"number_unsigned_long_long"];

    number = [NSNumber numberWithFloat:3.1415];
    [userDefault setObject:number forKey:@"number_float"];

    number = [NSNumber numberWithDouble:std::numeric_limits<double>::max()];
    [userDefault setObject:number forKey:@"number_double"];

    number = [NSNumber numberWithInteger:std::numeric_limits<NSInteger>::min()];
    [userDefault setObject:number forKey:@"number_NSInteger"];

    number = [NSNumber numberWithUnsignedInteger:std::numeric_limits<NSUInteger>::max()];
    [userDefault setObject:number forKey:@"number_NSUInteger"];

    [mmkv migrateFromUserDefaults:userDefault];

    XCTAssertEqual([mmkv getBoolForKey:@"bool"], [userDefault boolForKey:@"bool"]);
    XCTAssertEqual([mmkv getInt64ForKey:@"NSInteger"], [userDefault integerForKey:@"NSInteger"]);
    XCTAssertEqualWithAccuracy([mmkv getFloatForKey:@"float"], [userDefault floatForKey:@"float"], 0.001);
    XCTAssertEqualWithAccuracy([mmkv getDoubleForKey:@"double"], [userDefault doubleForKey:@"double"], 0.001);
    XCTAssertEqualObjects([mmkv getStringForKey:@"string"], [userDefault stringForKey:@"string"]);
    [self compareDate:[mmkv getDateForKey:@"date"] withDate:[userDefault objectForKey:@"date"]];
    XCTAssertEqualObjects([mmkv getDataForKey:@"data"], [userDefault dataForKey:@"data"]);
    XCTAssertEqualObjects([NSKeyedUnarchiver unarchivedObjectOfClass:NSURL.class fromData:[mmkv getDataForKey:@"url"] error:nil], [userDefault URLForKey:@"url"]);

    number = [userDefault objectForKey:@"number_bool"];
    XCTAssertEqual([mmkv getBoolForKey:@"number_bool"], number.boolValue);

    number = [userDefault objectForKey:@"number_char"];
    XCTAssertEqual([mmkv getInt32ForKey:@"number_char"], number.charValue);

    number = [userDefault objectForKey:@"number_unsigned_char"];
    XCTAssertEqual([mmkv getInt32ForKey:@"number_unsigned_char"], number.unsignedCharValue);

    number = [userDefault objectForKey:@"number_short"];
    XCTAssertEqual([mmkv getInt32ForKey:@"number_short"], number.shortValue);

    number = [userDefault objectForKey:@"number_unsigned_short"];
    XCTAssertEqual([mmkv getInt32ForKey:@"number_unsigned_short"], number.unsignedShortValue);

    number = [userDefault objectForKey:@"number_int"];
    XCTAssertEqual([mmkv getInt32ForKey:@"number_int"], number.intValue);

    number = [userDefault objectForKey:@"number_unsigned_int"];
    XCTAssertEqual([mmkv getUInt32ForKey:@"number_unsigned_int"], number.unsignedIntValue);

    number = [userDefault objectForKey:@"number_long"];
    XCTAssertEqual([mmkv getInt64ForKey:@"number_long"], number.longValue);

    number = [userDefault objectForKey:@"number_unsigned_long"];
    XCTAssertEqual([mmkv getUInt64ForKey:@"number_unsigned_long"], number.unsignedLongValue);

    number = [userDefault objectForKey:@"number_long_long"];
    XCTAssertEqual([mmkv getInt64ForKey:@"number_long_long"], number.longLongValue);

    number = [userDefault objectForKey:@"number_unsigned_long_long"];
    XCTAssertEqual([mmkv getUInt64ForKey:@"number_unsigned_long_long"], number.unsignedLongLongValue);

    number = [userDefault objectForKey:@"number_float"];
    XCTAssertEqualWithAccuracy([mmkv getFloatForKey:@"number_float"], number.floatValue, 0.001);

    number = [userDefault objectForKey:@"number_double"];
    XCTAssertEqualWithAccuracy([mmkv getDoubleForKey:@"number_double"], number.doubleValue, 0.001);

    number = [userDefault objectForKey:@"number_NSInteger"];
    XCTAssertEqual([mmkv getInt64ForKey:@"number_NSInteger"], number.integerValue);

    number = [userDefault objectForKey:@"number_NSUInteger"];
    XCTAssertEqual([mmkv getUInt64ForKey:@"number_NSUInteger"], number.unsignedIntegerValue);
}

- (void)testMultiTimesOverwriteValue {
    NSData *data;
    int loops = 1000000;
    for (int index = 0; index < loops; index++) {
        NSString *str = [NSString stringWithFormat:@"%s-%d", __FILE__, rand()];
        data = [str dataUsingEncoding:NSUTF8StringEncoding];
        BOOL ret = [mmkv setData:data forKey:@"data"];
        XCTAssertEqual(ret, YES);
    }
    NSData *value = [mmkv getObjectOfClass:NSData.class forKey:@"data"];
    XCTAssertEqualObjects(value, data);

    value = [mmkv getObjectOfClass:NSData.class forKey:KeyNotExist];
    XCTAssertEqualObjects(value, nil);
}

@end
