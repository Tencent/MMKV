//
//  ViewController.m
//  MMKVDemo
//
//  Created by Ling Guo on 27/02/2018.
//  Copyright © 2018 Lingol. All rights reserved.
//

#import "ViewController.h"
#import <MMKV/MMKV.h>

@interface ViewController ()

@end

@implementation ViewController

- (void)viewDidLoad {
	[super viewDidLoad];

	MMKV* mmkv = [MMKV defaultMMKV];
	[mmkv setBool:YES forKey:@"bool"];
	NSLog(@"%d", [mmkv getBoolForKey:@"bool"]);
}


- (void)didReceiveMemoryWarning {
	[super didReceiveMemoryWarning];
	// Dispose of any resources that can be recreated.
}


@end
