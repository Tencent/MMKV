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

#import "ViewController.h"
#import "MMKVDemo-Swift.h"
#import <MMKV/MMKV.h>

@interface TestNSArchive : NSObject <NSCoding>
@property(nonatomic, strong) NSString *m_username;
@property(nonatomic, assign) int32_t m_age;
@property(nonatomic, assign) float m_score;
@end

@implementation TestNSArchive

- (id)initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (!self) {
        return nil;
    }

    self.m_username = [decoder decodeObjectForKey:@"m_username"];
    self.m_age = [decoder decodeInt32ForKey:@"m_age"];
    self.m_score = [decoder decodeFloatForKey:@"m_score"];

    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:self.m_username forKey:@"m_username"];
    [encoder encodeInteger:self.m_age forKey:@"m_age"];
    [encoder encodeFloat:self.m_score forKey:@"m_score"];
}

@end

@implementation ViewController {
    NSMutableArray *m_arrStrings;
    NSMutableArray *m_arrStrKeys;
    NSMutableArray *m_arrIntKeys;
    NSMutableArray *m_arrObjKeys;
    NSMutableArray *m_arrNSCodingObjs;

    int m_loops;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    [self funcionalTest];
    [self testReKey];
    [self testImportFromUserDefault];
    //[self testCornerSize];
    //[self testFastRemoveCornerSize];

    DemoSwiftUsage *swiftUsageDemo = [[DemoSwiftUsage alloc] init];
    [swiftUsageDemo testSwiftFunctionality];

    [self testMultiProcess];
    //[self testMultiProcess];

    m_loops = 10000;
    m_arrStrings = [NSMutableArray arrayWithCapacity:m_loops];
    m_arrStrKeys = [NSMutableArray arrayWithCapacity:m_loops];
    m_arrIntKeys = [NSMutableArray arrayWithCapacity:m_loops];
    m_arrObjKeys = [NSMutableArray arrayWithCapacity:m_loops];
    m_arrNSCodingObjs = [NSMutableArray arrayWithCapacity:m_loops];
    for (size_t index = 0; index < m_loops; index++) {
        NSString *str = [NSString stringWithFormat:@"%s-%d", __FILE__, rand()];
        [m_arrStrings addObject:str];

        NSString *strKey = [NSString stringWithFormat:@"str-%zu", index];
        [m_arrStrKeys addObject:strKey];

        NSString *intKey = [NSString stringWithFormat:@"int-%zu", index];
        [m_arrIntKeys addObject:intKey];
        /*
        NSString *objKey = [NSString stringWithFormat:@"obj-%zu", index];
        [m_arrObjKeys addObject:objKey];
        
        TestNSArchive *obj = [[TestNSArchive alloc] init];
        obj.m_username = str;
        obj.m_age = rand();
        obj.m_age = rand() * rand() * 0.5;
        [m_arrNSCodingObjs addObject:obj];*/
    }
    //getMMKVForBatchTest();
}

- (void)funcionalTest {
    auto path = [MMKV mmkvBasePath];
    path = [path stringByDeletingLastPathComponent];
    path = [path stringByAppendingPathComponent:@"mmkv_2"];
    auto mmkv = [MMKV mmkvWithID:@"test/case1" relativePath:path];

    [mmkv setBool:YES forKey:@"bool"];
    NSLog(@"bool:%d", [mmkv getBoolForKey:@"bool"]);

    [mmkv setInt32:-1024 forKey:@"int32"];
    NSLog(@"int32:%d", [mmkv getInt32ForKey:@"int32"]);

    [mmkv setUInt32:std::numeric_limits<uint32_t>::max() forKey:@"uint32"];
    NSLog(@"uint32:%u", [mmkv getUInt32ForKey:@"uint32"]);

    [mmkv setInt64:std::numeric_limits<int64_t>::min() forKey:@"int64"];
    NSLog(@"int64:%lld", [mmkv getInt64ForKey:@"int64"]);

    [mmkv setUInt64:std::numeric_limits<uint64_t>::max() forKey:@"uint64"];
    NSLog(@"uint64:%llu", [mmkv getInt64ForKey:@"uint64"]);

    [mmkv setFloat:-3.1415926 forKey:@"float"];
    NSLog(@"float:%f", [mmkv getFloatForKey:@"float"]);

    [mmkv setDouble:std::numeric_limits<double>::max() forKey:@"double"];
    NSLog(@"double:%f", [mmkv getDoubleForKey:@"double"]);

    [mmkv setString:@"hello, mmkv" forKey:@"string"];
    NSLog(@"string:%@", [mmkv getStringForKey:@"string"]);

    [mmkv setObject:nil forKey:@"string"];
    NSLog(@"string after set nil:%@, containsKey:%d",
          [mmkv getObjectOfClass:NSString.class
                          forKey:@"string"],
          [mmkv containsKey:@"string"]);

    [mmkv setDate:[NSDate date] forKey:@"date"];
    NSLog(@"date:%@", [mmkv getDateForKey:@"date"]);

    [mmkv setData:[@"hello, mmkv again and again" dataUsingEncoding:NSUTF8StringEncoding] forKey:@"data"];
    NSData *data = [mmkv getDataForKey:@"data"];
    NSLog(@"data:%@", [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
    NSLog(@"data length:%zu, value size consumption:%zu", data.length, [mmkv getValueSizeForKey:@"data"]);

    [mmkv setObject:[NSData data] forKey:@"data"];
    NSLog(@"data after set empty data:%@, containsKey:%d",
          [mmkv getObjectOfClass:NSData.class
                          forKey:@"data"],
          [mmkv containsKey:@"data"]);

    [mmkv removeValueForKey:@"bool"];
    NSLog(@"bool:%d", [mmkv getBoolForKey:@"bool"]);
    [mmkv removeValuesForKeys:@[ @"int32", @"uint64" ]];
    NSLog(@"allKeys %@", [mmkv allKeys]);

    [mmkv close];
}

- (void)testCornerSize {
    auto mmkv = [MMKV mmkvWithID:@"test/cornerSize" cryptKey:[@"crypt" dataUsingEncoding:NSUTF8StringEncoding]];
    [mmkv clearAll];
    auto size = getpagesize() - 2;
    size -= 4;
    NSString *key = @"key";
    auto keySize = 3 + 1;
    size -= keySize;
    auto valueSize = 3;
    size -= valueSize;
    NSData *value = [NSMutableData dataWithLength:size];
    [mmkv setObject:value forKey:key];
}

- (void)testFastRemoveCornerSize {
    auto mmkv = [MMKV mmkvWithID:@"test/FastRemoveCornerSize" cryptKey:[@"crypt" dataUsingEncoding:NSUTF8StringEncoding]];
    [mmkv clearAll];
    auto size = getpagesize() - 4;
    size -= 4;
    NSString *key = @"key";
    auto keySize = 3 + 1;
    size -= keySize;
    auto valueSize = 3;
    size -= valueSize;
    size -= (keySize + 1); // total size of fast remove
    size /= 16;
    NSMutableData *value = [NSMutableData dataWithLength:size];
    auto ptr = (char *) value.mutableBytes;
    for (int i = 0; i < value.length; i++) {
        ptr[i] = 'A';
    }
    for (int i = 0; i < 16; i++) {
        [mmkv setObject:value forKey:key]; // when a full write back is occur, here's corruption happens
        [mmkv removeValueForKey:key];
    }
}

- (void)testMMKV:(NSString *)mmapID withCryptKey:(NSData *)cryptKey decodeOnly:(BOOL)decodeOnly {
    MMKV *mmkv = [MMKV mmkvWithID:mmapID cryptKey:cryptKey];

    if (!decodeOnly) {
        [mmkv setInt32:-1024 forKey:@"int32"];
    }
    NSLog(@"int32:%d", [mmkv getInt32ForKey:@"int32"]);

    if (!decodeOnly) {
        [mmkv setUInt32:std::numeric_limits<uint32_t>::max() forKey:@"uint32"];
    }
    NSLog(@"uint32:%u", [mmkv getUInt32ForKey:@"uint32"]);

    if (!decodeOnly) {
        [mmkv setInt64:std::numeric_limits<int64_t>::min() forKey:@"int64"];
    }
    NSLog(@"int64:%lld", [mmkv getInt64ForKey:@"int64"]);

    if (!decodeOnly) {
        [mmkv setUInt64:std::numeric_limits<uint64_t>::max() forKey:@"uint64"];
    }
    NSLog(@"uint64:%llu", [mmkv getInt64ForKey:@"uint64"]);

    if (!decodeOnly) {
        [mmkv setFloat:-3.1415926 forKey:@"float"];
    }
    NSLog(@"float:%f", [mmkv getFloatForKey:@"float"]);

    if (!decodeOnly) {
        [mmkv setDouble:std::numeric_limits<double>::max() forKey:@"double"];
    }
    NSLog(@"double:%f", [mmkv getDoubleForKey:@"double"]);

    if (!decodeOnly) {
        [mmkv setObject:@"hello, mmkv" forKey:@"string"];
    }
    NSLog(@"string:%@", [mmkv getObjectOfClass:NSString.class forKey:@"string"]);

    if (!decodeOnly) {
        [mmkv setObject:[NSDate date] forKey:@"date"];
    }
    NSLog(@"date:%@", [mmkv getObjectOfClass:NSDate.class forKey:@"date"]);

    if (!decodeOnly) {
        [mmkv setObject:[@"hello, mmkv again and again" dataUsingEncoding:NSUTF8StringEncoding] forKey:@"data"];
    }
    NSData *data = [mmkv getObjectOfClass:NSData.class forKey:@"data"];
    NSLog(@"data:%@", [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);

    if (!decodeOnly) {
        [mmkv removeValueForKey:@"bool"];
    }
    NSLog(@"bool:%d", [mmkv getBoolForKey:@"bool"]);

    NSLog(@"containsKey[string]: %d", [mmkv containsKey:@"string"]);

    [mmkv removeValuesForKeys:@[ @"int", @"long" ]];
    [mmkv clearMemoryCache];
    NSLog(@"isFileValid[%@]: %d", mmapID, [MMKV isFileValid:mmapID]);
}

- (void)testReKey {
    NSString *mmapID = @"testAES_reKey";
    [self testMMKV:mmapID withCryptKey:nullptr decodeOnly:NO];

    MMKV *kv = [MMKV mmkvWithID:mmapID cryptKey:nullptr];
    NSData *key_1 = [@"Key_seq_1" dataUsingEncoding:NSUTF8StringEncoding];
    [kv reKey:key_1];
    [kv clearMemoryCache];
    [self testMMKV:mmapID withCryptKey:key_1 decodeOnly:YES];

    NSData *key_2 = [@"Key_seq_2" dataUsingEncoding:NSUTF8StringEncoding];
    [kv reKey:key_2];
    [kv clearMemoryCache];
    [self testMMKV:mmapID withCryptKey:key_2 decodeOnly:YES];

    [kv reKey:nullptr];
    [kv clearMemoryCache];
    [self testMMKV:mmapID withCryptKey:nullptr decodeOnly:YES];
}

- (void)testImportFromUserDefault {
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

    auto mmkv = [MMKV mmkvWithID:@"testImportNSUserDefaults"];
    [mmkv migrateFromUserDefaults:userDefault];
    NSLog(@"migrate from NSUserDefault begin");

    NSLog(@"bool = %d", [mmkv getBoolForKey:@"bool"]);
    NSLog(@"NSInteger = %lld", [mmkv getInt64ForKey:@"NSInteger"]);
    NSLog(@"float = %f", [mmkv getFloatForKey:@"float"]);
    NSLog(@"double = %f", [mmkv getDoubleForKey:@"double"]);
    NSLog(@"string = %@", [mmkv getStringForKey:@"string"]);
    NSLog(@"date = %@", [mmkv getDateForKey:@"date"]);
    NSLog(@"data = %@", [[NSString alloc] initWithData:[mmkv getDataForKey:@"data"] encoding:NSUTF8StringEncoding]);
    NSLog(@"url = %@", [NSKeyedUnarchiver unarchiveObjectWithData:[mmkv getDataForKey:@"url"]]);
    NSLog(@"number_bool = %d", [mmkv getBoolForKey:@"number_bool"]);
    NSLog(@"number_char = %d", [mmkv getInt32ForKey:@"number_char"]);
    NSLog(@"number_unsigned_char = %d", [mmkv getInt32ForKey:@"number_unsigned_char"]);
    NSLog(@"number_short = %d", [mmkv getInt32ForKey:@"number_short"]);
    NSLog(@"number_unsigned_short = %d", [mmkv getInt32ForKey:@"number_unsigned_short"]);
    NSLog(@"number_int = %d", [mmkv getInt32ForKey:@"number_int"]);
    NSLog(@"number_unsigned_int = %u", [mmkv getUInt32ForKey:@"number_unsigned_int"]);
    NSLog(@"number_long = %lld", [mmkv getInt64ForKey:@"number_long"]);
    NSLog(@"number_unsigned_long = %llu", [mmkv getUInt64ForKey:@"number_unsigned_long"]);
    NSLog(@"number_long_long = %lld", [mmkv getInt64ForKey:@"number_long_long"]);
    NSLog(@"number_unsigned_long_long = %llu", [mmkv getUInt64ForKey:@"number_unsigned_long_long"]);
    NSLog(@"number_float = %f", [mmkv getFloatForKey:@"number_float"]);
    NSLog(@"number_double = %f", [mmkv getDoubleForKey:@"number_double"]);
    NSLog(@"number_NSInteger = %lld", [mmkv getInt64ForKey:@"number_NSInteger"]);
    NSLog(@"number_NSUInteger = %llu", [mmkv getUInt64ForKey:@"number_NSUInteger"]);

    NSLog(@"migrate from NSUserDefault end");
}

- (IBAction)onBtnClick:(id)sender {
    [self.m_loading startAnimating];
    self.m_btn.enabled = NO;

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        self->m_arrStrings = [NSMutableArray arrayWithCapacity:self->m_loops];
        for (size_t index = 0; index < self->m_loops; index++) {
            NSString *str = [NSString stringWithFormat:@"%s-%d", __FILE__, rand()];
            [self->m_arrStrings addObject:str];

            //TestNSArchive *obj = self->m_arrNSCodingObjs[index];
            //obj.m_username = str;
        }

        [self mmkvBaselineTest:self->m_loops];
        [self userDefaultBaselineTest:self->m_loops];
        //[self brutleTest];

        [self.m_loading stopAnimating];
        self.m_btn.enabled = YES;
    });
}

#pragma mark - mmkv baseline test

- (void)mmkvBaselineTest:(int)loops {
    [self mmkvBatchWriteInt:loops];
    [self mmkvBatchReadInt:loops];
    [self mmkvBatchWriteString:loops];
    [self mmkvBatchReadString:loops];
    //[self mmkvBatchWriteObject:loops];
    //[self mmkvBatchReadObject:loops];

    //[self mmkvBatchDeleteString:loops];
    //[[MMKV defaultMMKV] trim];
}

MMKV *getMMKVForBatchTest() {
    //auto cryptKey = [@"crypt_key" dataUsingEncoding:NSUTF8StringEncoding];
    NSData *cryptKey = nil;
    MMKV *mmkv = [MMKV mmkvWithID:@"batchTest" cryptKey:cryptKey];
    return mmkv;
}

- (void)mmkvBatchWriteInt:(int)loops {
    @autoreleasepool {
        NSDate *startDate = [NSDate date];

        MMKV *mmkv = getMMKVForBatchTest();
        for (int index = 0; index < loops; index++) {
            int32_t tmp = rand();
            NSString *intKey = m_arrIntKeys[index];
            [mmkv setInt32:tmp forKey:intKey];
        }
        NSDate *endDate = [NSDate date];
        int cost = [endDate timeIntervalSinceDate:startDate] * 1000;
        NSLog(@"mmkv write int %d times, cost:%d ms", loops, cost);
    }
}

- (void)mmkvBatchReadInt:(int)loops {
    @autoreleasepool {
        NSDate *startDate = [NSDate date];

        MMKV *mmkv = getMMKVForBatchTest();
        for (int index = 0; index < loops; index++) {
            NSString *intKey = m_arrIntKeys[index];
            [mmkv getInt32ForKey:intKey];
        }
        NSDate *endDate = [NSDate date];
        int cost = [endDate timeIntervalSinceDate:startDate] * 1000;
        NSLog(@"mmkv read int %d times, cost:%d ms", loops, cost);
    }
}

- (void)mmkvBatchWriteString:(int)loops {
    @autoreleasepool {
        NSDate *startDate = [NSDate date];

        MMKV *mmkv = getMMKVForBatchTest();
        for (int index = 0; index < loops; index++) {
            NSString *str = m_arrStrings[index];
            NSString *strKey = m_arrStrKeys[index];
            [mmkv setObject:str forKey:strKey];
        }
        NSDate *endDate = [NSDate date];
        int cost = [endDate timeIntervalSinceDate:startDate] * 1000;
        NSLog(@"mmkv write string %d times, cost:%d ms", loops, cost);
    }
}

- (void)mmkvBatchReadString:(int)loops {
    @autoreleasepool {
        NSDate *startDate = [NSDate date];

        MMKV *mmkv = getMMKVForBatchTest();
        for (int index = 0; index < loops; index++) {
            NSString *strKey = m_arrStrKeys[index];
            [mmkv getObjectOfClass:NSString.class forKey:strKey];
        }
        NSDate *endDate = [NSDate date];
        int cost = [endDate timeIntervalSinceDate:startDate] * 1000;
        NSLog(@"mmkv read string %d times, cost:%d ms", loops, cost);
    }
}

- (void)mmkvBatchDeleteString:(int)loops {
    @autoreleasepool {
        NSDate *startDate = [NSDate date];

        MMKV *mmkv = getMMKVForBatchTest();
        for (int index = 0; index < loops; index++) {
            NSString *strKey = m_arrStrKeys[index];
            [mmkv removeValueForKey:strKey];
        }
        NSDate *endDate = [NSDate date];
        int cost = [endDate timeIntervalSinceDate:startDate] * 1000;
        NSLog(@"mmkv delete string %d times, cost:%d ms", loops, cost);
    }
}

- (void)mmkvBatchWriteObject:(int)loops {
    @autoreleasepool {
        NSDate *startDate = [NSDate date];

        MMKV *mmkv = getMMKVForBatchTest();
        for (int index = 0; index < loops; index++) {
            TestNSArchive *obj = m_arrNSCodingObjs[index];
            NSString *objKey = m_arrObjKeys[index];
            [mmkv setObject:obj forKey:objKey];
        }
        NSDate *endDate = [NSDate date];
        int cost = [endDate timeIntervalSinceDate:startDate] * 1000;
        NSLog(@"mmkv write object %d times, cost:%d ms", loops, cost);
    }
}

- (void)mmkvBatchReadObject:(int)loops {
    @autoreleasepool {
        NSDate *startDate = [NSDate date];

        MMKV *mmkv = getMMKVForBatchTest();
        for (int index = 0; index < loops; index++) {
            NSString *objKey = m_arrObjKeys[index];
            [mmkv getObjectOfClass:TestNSArchive.class forKey:objKey];
        }
        NSDate *endDate = [NSDate date];
        int cost = [endDate timeIntervalSinceDate:startDate] * 1000;
        NSLog(@"mmkv read object %d times, cost:%d ms", loops, cost);
    }
}

#pragma mark - NSUserDefault baseline test

- (void)userDefaultBaselineTest:(int)loops {
    [self userDefaultBatchWriteInt:loops];
    [self userDefaultBatchReadInt:loops];
    [self userDefaultBatchWriteString:loops];
    [self userDefaultBatchReadString:loops];
    //[self userDefaultBatchWriteObject:loops];
    //[self userDefaultBatchReadObject:loops];
}

- (void)userDefaultBatchWriteInt:(int)loops {
    @autoreleasepool {
        NSDate *startDate = [NSDate date];

        NSUserDefaults *userdefault = [NSUserDefaults standardUserDefaults];
        for (int index = 0; index < loops; index++) {
            NSInteger tmp = rand();
            NSString *intKey = m_arrIntKeys[index];
            [userdefault setInteger:tmp forKey:intKey];
        }
        [userdefault synchronize];
        NSDate *endDate = [NSDate date];
        int cost = [endDate timeIntervalSinceDate:startDate] * 1000;
        NSLog(@"NSUserDefaults write int %d times, cost:%d ms", loops, cost);
    }
}

- (void)userDefaultBatchReadInt:(int)loops {
    @autoreleasepool {
        NSDate *startDate = [NSDate date];

        NSUserDefaults *userdefault = [NSUserDefaults standardUserDefaults];
        for (int index = 0; index < loops; index++) {
            NSString *intKey = m_arrIntKeys[index];
            [userdefault integerForKey:intKey];
        }
        NSDate *endDate = [NSDate date];
        int cost = [endDate timeIntervalSinceDate:startDate] * 1000;
        NSLog(@"NSUserDefaults read int %d times, cost:%d ms", loops, cost);
    }
}

- (void)userDefaultBatchWriteString:(int)loops {
    @autoreleasepool {
        NSDate *startDate = [NSDate date];

        NSUserDefaults *userdefault = [NSUserDefaults standardUserDefaults];
        for (int index = 0; index < loops; index++) {
            NSString *str = m_arrStrings[index];
            NSString *strKey = m_arrStrKeys[index];
            [userdefault setObject:str forKey:strKey];
        }
        [userdefault synchronize];
        NSDate *endDate = [NSDate date];
        int cost = [endDate timeIntervalSinceDate:startDate] * 1000;
        NSLog(@"NSUserDefaults write string %d times, cost:%d ms", loops, cost);
    }
}

- (void)userDefaultBatchReadString:(int)loops {
    @autoreleasepool {
        NSDate *startDate = [NSDate date];

        NSUserDefaults *userdefault = [NSUserDefaults standardUserDefaults];
        for (int index = 0; index < loops; index++) {
            NSString *strKey = m_arrStrKeys[index];
            [userdefault objectForKey:strKey];
        }
        NSDate *endDate = [NSDate date];
        int cost = [endDate timeIntervalSinceDate:startDate] * 1000;
        NSLog(@"NSUserDefaults read string %d times, cost:%d ms", loops, cost);
    }
}

- (void)userDefaultBatchWriteObject:(int)loops {
    @autoreleasepool {
        NSDate *startDate = [NSDate date];

        NSUserDefaults *userdefault = [NSUserDefaults standardUserDefaults];
        for (int index = 0; index < loops; index++) {
            TestNSArchive *obj = m_arrNSCodingObjs[index];
            NSString *objKey = m_arrObjKeys[index];
            auto tmp = [NSKeyedArchiver archivedDataWithRootObject:obj];
            [userdefault setObject:tmp forKey:objKey];
        }
        [userdefault synchronize];
        NSDate *endDate = [NSDate date];
        int cost = [endDate timeIntervalSinceDate:startDate] * 1000;
        NSLog(@"NSUserDefaults write object %d times, cost:%d ms", loops, cost);
    }
}

- (void)userDefaultBatchReadObject:(int)loops {
    @autoreleasepool {
        NSDate *startDate = [NSDate date];

        NSUserDefaults *userdefault = [NSUserDefaults standardUserDefaults];
        for (int index = 0; index < loops; index++) {
            NSString *objKey = m_arrObjKeys[index];
            NSData *tmp = [userdefault objectForKey:objKey];
            [NSKeyedUnarchiver unarchiveObjectWithData:tmp];
        }
        NSDate *endDate = [NSDate date];
        int cost = [endDate timeIntervalSinceDate:startDate] * 1000;
        NSLog(@"NSUserDefaults read object %d times, cost:%d ms", loops, cost);
    }
}

#pragma mark - brutle test

- (void)brutleTest {
    auto mmkv = [MMKV mmkvWithID:@"brutleTest"];
    auto ptr = malloc(1024);
    auto data = [NSData dataWithBytes:ptr length:1024];
    free(ptr);
    for (size_t index = 0; index < std::numeric_limits<size_t>::max(); index++) {
        NSString *key = [NSString stringWithFormat:@"key-%zu", index];
        [mmkv setObject:data forKey:key];

        if (index % 1000 == 0) {
            NSLog(@"brutleTest size=%zu", mmkv.totalSize);
        }
    }
}

#pragma mark - multi-process

- (void)testMultiProcess {
    NSData *key_1 = [@"multi_process" dataUsingEncoding:NSUTF8StringEncoding];
    auto mmkv = [MMKV mmkvWithID:@"multi_process" cryptKey:key_1 mode:MMKVMultiProcess];

    [mmkv setBool:YES forKey:@"bool"];
    NSLog(@"bool:%d", [mmkv getBoolForKey:@"bool"]);

    [mmkv setInt32:-1024 forKey:@"int32"];
    NSLog(@"int32:%d", [mmkv getInt32ForKey:@"int32"]);

    [mmkv setUInt32:std::numeric_limits<uint32_t>::max() forKey:@"uint32"];
    NSLog(@"uint32:%u", [mmkv getUInt32ForKey:@"uint32"]);

    [mmkv setInt64:std::numeric_limits<int64_t>::min() forKey:@"int64"];
    NSLog(@"int64:%lld", [mmkv getInt64ForKey:@"int64"]);

    [mmkv setUInt64:std::numeric_limits<uint64_t>::max() forKey:@"uint64"];
    NSLog(@"uint64:%llu", [mmkv getInt64ForKey:@"uint64"]);

    [mmkv setFloat:-3.1415926 forKey:@"float"];
    NSLog(@"float:%f", [mmkv getFloatForKey:@"float"]);

    [mmkv setDouble:std::numeric_limits<double>::max() forKey:@"double"];
    NSLog(@"double:%f", [mmkv getDoubleForKey:@"double"]);

    [mmkv setString:@"hello, mmkv" forKey:@"string"];
    NSLog(@"string:%@", [mmkv getStringForKey:@"string"]);

    [mmkv setObject:nil forKey:@"string"];
    NSLog(@"string after set nil:%@, containsKey:%d",
          [mmkv getObjectOfClass:NSString.class
                          forKey:@"string"],
          [mmkv containsKey:@"string"]);

    [mmkv setDate:[NSDate date] forKey:@"date"];
    NSLog(@"date:%@", [mmkv getDateForKey:@"date"]);

    [mmkv setData:[@"hello, mmkv again and again" dataUsingEncoding:NSUTF8StringEncoding] forKey:@"data"];
    NSData *data = [mmkv getDataForKey:@"data"];
    NSLog(@"data:%@", [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
    NSLog(@"data length:%zu, value size consumption:%zu", data.length, [mmkv getValueSizeForKey:@"data"]);

    [mmkv removeValueForKey:@"bool"];
    NSLog(@"bool:%d", [mmkv getBoolForKey:@"bool"]);
    [mmkv removeValuesForKeys:@[ @"int32", @"uint64" ]];
    NSLog(@"allKeys %@", [mmkv allKeys]);

    [mmkv close];
}

@end
