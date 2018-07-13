//
//  MMKVPerformanceTest.m
//  MMKVDemoTests
//
//  Created by Ling Guo on 2018/7/13.
//  Copyright © 2018 Lingol. All rights reserved.
//

#import <MMKV/MMKV.h>
#import <XCTest/XCTest.h>

@interface MMKVPerformanceTest : XCTestCase

@end

@implementation MMKVPerformanceTest {
	MMKV *mmkv;
}

- (void)setUp {
	[super setUp];

	mmkv = [MMKV mmkvWithID:@"performance_test"];
}

- (void)tearDown {
	mmkv = nil;

	[super tearDown];
}

- (void)testWriteIntPerformance {
	int loops = 10000;
	NSMutableArray *arrIntKeys = [NSMutableArray arrayWithCapacity:loops];
	for (size_t index = 0; index < loops; index++) {
		NSString *intKey = [NSString stringWithFormat:@"int-%zu", index];
		[arrIntKeys addObject:intKey];
	}

	[self measureBlock:^{
	  for (int index = 0; index < loops; index++) {
		  int32_t tmp = rand();
		  NSString *intKey = arrIntKeys[index];
		  [mmkv setInt32:tmp forKey:intKey];
	  }
	}];
}

- (void)testReadIntPerformance {
	int loops = 10000;
	NSMutableArray *arrIntKeys = [NSMutableArray arrayWithCapacity:loops];
	for (size_t index = 0; index < loops; index++) {
		NSString *intKey = [NSString stringWithFormat:@"int-%zu", index];
		[arrIntKeys addObject:intKey];
	}

	[self measureBlock:^{
	  for (int index = 0; index < loops; index++) {
		  NSString *intKey = arrIntKeys[index];
		  int32_t tmp = [mmkv getInt32ForKey:intKey];
	  }
	}];
}

- (void)testWriteStringPerformance {
	int loops = 10000;
	NSMutableArray *arrStrings = [NSMutableArray arrayWithCapacity:loops];
	NSMutableArray *arrStrKeys = [NSMutableArray arrayWithCapacity:loops];
	for (size_t index = 0; index < loops; index++) {
		NSString *str = [NSString stringWithFormat:@"%s-%d", __FILE__, rand()];
		[arrStrings addObject:str];

		NSString *strKey = [NSString stringWithFormat:@"str-%zu", index];
		[arrStrKeys addObject:strKey];
	}

	[self measureBlock:^{
	  for (int index = 0; index < loops; index++) {
		  NSString *str = arrStrings[index];
		  NSString *strKey = arrStrKeys[index];
		  [mmkv setObject:str forKey:strKey];
	  }
	}];
}

- (void)testReadStringPerformance {
	int loops = 10000;
	NSMutableArray *arrStrings = [NSMutableArray arrayWithCapacity:loops];
	NSMutableArray *arrStrKeys = [NSMutableArray arrayWithCapacity:loops];
	for (size_t index = 0; index < loops; index++) {
		NSString *str = [NSString stringWithFormat:@"%s-%d", __FILE__, rand()];
		[arrStrings addObject:str];

		NSString *strKey = [NSString stringWithFormat:@"str-%zu", index];
		[arrStrKeys addObject:strKey];
	}

	[self measureBlock:^{
	  for (int index = 0; index < loops; index++) {
		  NSString *strKey = arrStrKeys[index];
		  NSString *str = [mmkv getObjectOfClass:NSString.class forKey:strKey];
	  }
	}];
}

@end
