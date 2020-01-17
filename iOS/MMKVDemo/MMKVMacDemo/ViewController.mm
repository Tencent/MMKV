//
//  ViewController.m
//  MMKVMacDemo
//
//  Created by Ling Guo on 2018/9/27.
//  Copyright © 2018 Lingol. All rights reserved.
//

#import "ViewController.h"
#import <MMKV/MMKV.h>

@implementation ViewController {
	NSMutableArray *m_arrStrings;
	NSMutableArray *m_arrStrKeys;
	NSMutableArray *m_arrIntKeys;

	int m_loops;
}

- (void)viewDidLoad {
	[super viewDidLoad];

	[self funcionalTest];
	[self testNeedLoadFromFile];

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
	MMKV *mmkv = [MMKV defaultMMKV];

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

	//    [mmkv setObject:@"hello, mmkv" forKey:@"string"];
	//    NSLog(@"string:%@", [mmkv getObjectOfClass:NSString.class forKey:@"string"]);

	[mmkv setDate:[NSDate date] forKey:@"date"];
	NSLog(@"date:%@", [mmkv getDateForKey:@"date"]);

	//    [mmkv setObject:[NSDate date] forKey:@"date"];
	//    NSLog(@"date:%@", [mmkv getObjectOfClass:NSDate.class forKey:@"date"]);

	[mmkv setObject:[@"hello, mmkv again and again" dataUsingEncoding:NSUTF8StringEncoding] forKey:@"data"];
	NSData *data = [mmkv getObjectOfClass:NSData.class forKey:@"data"];
	NSLog(@"data:%@", [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);

	[mmkv removeValueForKey:@"bool"];
	NSLog(@"bool:%d", [mmkv getBoolForKey:@"bool"]);
}

- (void)setRepresentedObject:(id)representedObject {
	[super setRepresentedObject:representedObject];

	// Update the view, if already loaded.
}

- (void)testNeedLoadFromFile {
	auto mmkv = [MMKV mmkvWithID:@"testNeedLoadFromFile"];
	[mmkv clearMemoryCache]; // or may be triggered by Memory Warning
	[mmkv clearAll];
	NSAssert([mmkv setString:@"value" forKey:@"key"], @"Fail to save");
}

@end
