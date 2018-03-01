//
//  ViewController.m
//  MMKVDemo
//
//  Created by Ling Guo on 27/02/2018.
//  Copyright © 2018 Lingol. All rights reserved.
//

#import "ViewController.h"
#import <MMKV/MMKV.h>

@implementation ViewController {
	NSMutableArray* m_arrStrings;
}

- (void)viewDidLoad {
	[super viewDidLoad];

//	[self funcionalTest];

	int loops = 10000;
	m_arrStrings = [NSMutableArray arrayWithCapacity:loops];
	for (size_t index = 0; index < loops; index++) {
		[m_arrStrings addObject:[NSString stringWithFormat:@"%s-%d", __FILE__, rand()]];
	}
	[self mmkvBaselineTest:loops];
	[self userDefaultBaselineTest:loops];
}

-(void)funcionalTest {
	MMKV* mmkv = [MMKV defaultMMKV];
	
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

	[mmkv setObject:@"hello, mmkv" forKey:@"string"];
	NSLog(@"string:%@", [mmkv getObjectOfClass:NSString.class forKey:@"string"]);
	
	[mmkv setObject:[NSDate date] forKey:@"date"];
	NSLog(@"date:%@", [mmkv getObjectOfClass:NSDate.class forKey:@"date"]);
	
	[mmkv setObject:[@"hello, mmkv again and again" dataUsingEncoding:NSUTF8StringEncoding] forKey:@"data"];
	NSData* data = [mmkv getObjectOfClass:NSData.class forKey:@"data"];
	NSLog(@"data:%@", [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
}

-(void)mmkvBaselineTest:(int)loops {
	@autoreleasepool {
		NSDate *startDate = [NSDate date];

		MMKV* mmkv = [MMKV defaultMMKV];
		for (int index = 0; index < loops; index++) {
			int32_t tmp = rand();
			[mmkv setInt32:tmp forKey:@"testInt"];
			tmp = [mmkv getInt32ForKey:@"testInt"];

			NSString* str = m_arrStrings[index];
			[mmkv setObject:str forKey:@"testStr"];
			str = [mmkv getObjectOfClass:NSString.class forKey:@"testStr"];
		}
		NSDate *endDate = [NSDate date];
		NSLog(@"mmkv %d times, cost:%f", loops, [endDate timeIntervalSinceDate:startDate]);
	}
}

-(void)userDefaultBaselineTest:(int)loops {
	@autoreleasepool {
		NSDate *startDate = [NSDate date];

		NSUserDefaults* userdefault = [NSUserDefaults standardUserDefaults];
		for (int index = 0; index < loops; index++) {
			NSInteger tmp = rand();
			[userdefault setInteger:tmp forKey:@"testInt"];
			tmp = [userdefault integerForKey:@"testInt"];
			
			NSString* str = m_arrStrings[index];
			[userdefault setObject:str forKey:@"testStr"];
			str = [userdefault objectForKey:@"testStr"];
		}
		NSDate *endDate = [NSDate date];
		NSLog(@"NSUserDefaults %d times, cost:%f", loops, [endDate timeIntervalSinceDate:startDate]);
	}
}

@end
