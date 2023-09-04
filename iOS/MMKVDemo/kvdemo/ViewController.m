//
//  ViewController.m
//  kvdemo
//
//  Created by lingol on 2023/6/28.
//  Copyright © 2023 Lingol. All rights reserved.
//

#import "ViewController.h"
#import <MMKV/MMKV.h>

@interface ViewController ()
@property(weak, nonatomic) IBOutlet UILabel *m_label;
@end

@implementation ViewController {
    MMKV *m_kv;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.

    NSData *encryptionKey = [@"multi_process" dataUsingEncoding:NSUTF8StringEncoding];
    m_kv = [MMKV mmkvWithID:@"multi_process" cryptKey:encryptionKey mode:MMKVMultiProcess];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(updateTodayContent)
                                                 name:UIApplicationDidBecomeActiveNotification
                                               object:nil];
}

- (void)updateTodayContent {
    NSString *newContent = [m_kv getStringForKey:@"content"];
    _m_label.text = newContent;
}

@end
