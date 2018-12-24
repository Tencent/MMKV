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

@interface ViewController () <MMKVHandler>
@end

@implementation ViewController {
	NSMutableArray *m_arrStrings;
	NSMutableArray *m_arrStrKeys;
	NSMutableArray *m_arrIntKeys;

	int m_loops;
}

- (void)viewDidLoad {
	[super viewDidLoad];

	[MMKV registerHandler:self];

	[self funcionalTest];
	//[self testReKey];

	DemoSwiftUsage *swiftUsageDemo = [[DemoSwiftUsage alloc] init];
	[swiftUsageDemo testSwiftFunctionality];

	m_loops = 10000;
	m_arrStrings = [NSMutableArray arrayWithCapacity:m_loops];
	m_arrStrKeys = [NSMutableArray arrayWithCapacity:m_loops];
	m_arrIntKeys = [NSMutableArray arrayWithCapacity:m_loops];
	for (size_t index = 0; index < m_loops; index++) {
		NSString *str = [NSString stringWithFormat:@"%s-%d", __FILE__, rand()];
		[m_arrStrings addObject:str];

		NSString *strKey = [NSString stringWithFormat:@"str-%zu", index];
		[m_arrStrKeys addObject:strKey];

		NSString *intKey = [NSString stringWithFormat:@"int-%zu", index];
		[m_arrIntKeys addObject:intKey];
	}
}

- (void)funcionalTest {
	MMKV *mmkv = [MMKV mmkvWithID:@"test/case1"];

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

	[mmkv removeValueForKey:@"bool"];
	NSLog(@"bool:%d", [mmkv getBoolForKey:@"bool"]);

	[mmkv close];
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

- (IBAction)onBtnClick:(id)sender {
	[self.m_loading startAnimating];
	self.m_btn.enabled = NO;

	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
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

	//[self mmkvBatchDeleteString:loops];
	//[[MMKV defaultMMKV] trim];
}

- (void)mmkvBatchWriteInt:(int)loops {
	@autoreleasepool {
		NSDate *startDate = [NSDate date];

		MMKV *mmkv = [MMKV defaultMMKV];
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

		MMKV *mmkv = [MMKV defaultMMKV];
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

		MMKV *mmkv = [MMKV defaultMMKV];
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

		MMKV *mmkv = [MMKV defaultMMKV];
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

		MMKV *mmkv = [MMKV defaultMMKV];
		for (int index = 0; index < loops; index++) {
			NSString *strKey = m_arrStrKeys[index];
			[mmkv removeValueForKey:strKey];
		}
		NSDate *endDate = [NSDate date];
		int cost = [endDate timeIntervalSinceDate:startDate] * 1000;
		NSLog(@"mmkv delete string %d times, cost:%d ms", loops, cost);
	}
}

#pragma mark - NSUserDefault baseline test

- (void)userDefaultBaselineTest:(int)loops {
	[self userDefaultBatchWriteInt:loops];
	[self userDefaultBatchReadInt:loops];
	[self userDefaultBatchWriteString:loops];
	[self userDefaultBatchReadString:loops];
}

- (void)userDefaultBatchWriteInt:(int)loops {
	@autoreleasepool {
		NSDate *startDate = [NSDate date];

		NSUserDefaults *userdefault = [NSUserDefaults standardUserDefaults];
		for (int index = 0; index < loops; index++) {
			NSInteger tmp = rand();
			NSString *intKey = m_arrIntKeys[index];
			[userdefault setInteger:tmp forKey:intKey];
			[userdefault synchronize];
		}
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
			[userdefault synchronize];
		}
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

#pragma mark - MMKVHandler

- (MMKVRecoverStrategic)onMMKVCRCCheckFail:(NSString *)mmapID {
	return MMKVOnErrorRecover;
}

- (MMKVRecoverStrategic)onMMKVFileLengthError:(NSString *)mmapID {
	return MMKVOnErrorRecover;
}

@end
