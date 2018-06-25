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
	int m_loops;
}

- (void)viewDidLoad {
	[super viewDidLoad];

	[self funcionalTest];

	m_loops = 10000;
	m_arrStrings = [NSMutableArray arrayWithCapacity:m_loops];
	for (size_t index = 0; index < m_loops; index++) {
		[m_arrStrings addObject:[NSString stringWithFormat:@"%s-%d", __FILE__, rand()]];
	}
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

-(IBAction)onBtnClick:(id)sender {
	[self.m_loading startAnimating];
	self.m_btn.enabled = NO;
	
	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
		[self mmkvBaselineTest:m_loops];
		[self userDefaultBaselineTest:m_loops];
		
		[self.m_loading stopAnimating];
		self.m_btn.enabled = YES;
	});
}

#pragma mark - mmkv baseline test

-(void)mmkvBaselineTest:(int)loops {
	[self mmkvBatchWriteInt:loops];
	[self mmkvBatchReadInt:loops];
	[self mmkvBatchWriteString:loops];
	[self mmkvBatchReadString:loops];
}

-(void)mmkvBatchWriteInt:(int)loops {
	@autoreleasepool {
		NSDate *startDate = [NSDate date];
		
		MMKV* mmkv = [MMKV defaultMMKV];
		for (int index = 0; index < loops; index++) {
			int32_t tmp = rand();
			[mmkv setInt32:tmp forKey:@"testInt"];
		}
		NSDate *endDate = [NSDate date];
		NSLog(@"mmkv write int %d times, cost:%f", loops, [endDate timeIntervalSinceDate:startDate] * 1000);
	}
}

-(void)mmkvBatchReadInt:(int)loops {
	@autoreleasepool {
		NSDate *startDate = [NSDate date];
		
		MMKV* mmkv = [MMKV defaultMMKV];
		for (int index = 0; index < loops; index++) {
			int32_t tmp = [mmkv getInt32ForKey:@"testInt"];
		}
		NSDate *endDate = [NSDate date];
		NSLog(@"mmkv read int %d times, cost:%f", loops, [endDate timeIntervalSinceDate:startDate] * 1000);
	}
}

-(void)mmkvBatchWriteString:(int)loops {
	@autoreleasepool {
		NSDate *startDate = [NSDate date];
		
		MMKV* mmkv = [MMKV defaultMMKV];
		for (int index = 0; index < loops; index++) {
			NSString* str = m_arrStrings[index];
			[mmkv setObject:str forKey:@"testStr"];
		}
		NSDate *endDate = [NSDate date];
		NSLog(@"mmkv write string %d times, cost:%f", loops, [endDate timeIntervalSinceDate:startDate] * 1000);
	}
}

-(void)mmkvBatchReadString:(int)loops {
	@autoreleasepool {
		NSDate *startDate = [NSDate date];
		
		MMKV* mmkv = [MMKV defaultMMKV];
		for (int index = 0; index < loops; index++) {
			NSString* str = [mmkv getObjectOfClass:NSString.class forKey:@"testStr"];
		}
		NSDate *endDate = [NSDate date];
		NSLog(@"mmkv read string %d times, cost:%f", loops, [endDate timeIntervalSinceDate:startDate] * 1000);
	}
}

#pragma mark - NSUserDefault baseline test

-(void)userDefaultBaselineTest:(int)loops {
	[self userDefaultBatchWriteInt:loops];
	[self userDefaultBatchReadInt:loops];
	[self userDefaultBatchWriteString:loops];
	[self userDefaultBatchReadString:loops];
}

-(void)userDefaultBatchWriteInt:(int)loops {
	@autoreleasepool {
		NSDate *startDate = [NSDate date];
		
		NSUserDefaults* userdefault = [NSUserDefaults standardUserDefaults];
		for (int index = 0; index < loops; index++) {
			NSInteger tmp = rand();
			[userdefault setInteger:tmp forKey:@"testInt"];
			[userdefault synchronize];
		}
		NSDate *endDate = [NSDate date];
		NSLog(@"NSUserDefaults write int %d times, cost:%f", loops, [endDate timeIntervalSinceDate:startDate] * 1000);
	}
}

-(void)userDefaultBatchReadInt:(int)loops {
	@autoreleasepool {
		NSDate *startDate = [NSDate date];
		
		NSUserDefaults* userdefault = [NSUserDefaults standardUserDefaults];
		for (int index = 0; index < loops; index++) {
			NSInteger tmp = [userdefault integerForKey:@"testInt"];
		}
		NSDate *endDate = [NSDate date];
		NSLog(@"NSUserDefaults read int %d times, cost:%f", loops, [endDate timeIntervalSinceDate:startDate] * 1000);
	}
}

-(void)userDefaultBatchWriteString:(int)loops {
	@autoreleasepool {
		NSDate *startDate = [NSDate date];
		
		NSUserDefaults* userdefault = [NSUserDefaults standardUserDefaults];
		for (int index = 0; index < loops; index++) {
			NSString* str = m_arrStrings[index];
			[userdefault setObject:str forKey:@"testStr"];
			[userdefault synchronize];
		}
		NSDate *endDate = [NSDate date];
		NSLog(@"NSUserDefaults write string %d times, cost:%f", loops, [endDate timeIntervalSinceDate:startDate] * 1000);
	}
}

-(void)userDefaultBatchReadString:(int)loops {
	@autoreleasepool {
		NSDate *startDate = [NSDate date];
		
		NSUserDefaults* userdefault = [NSUserDefaults standardUserDefaults];
		for (int index = 0; index < loops; index++) {
			NSString* str = [userdefault objectForKey:@"testStr"];
		}
		NSDate *endDate = [NSDate date];
		NSLog(@"NSUserDefaults read string %d times, cost:%f", loops, [endDate timeIntervalSinceDate:startDate] * 1000);
	}
}

@end
