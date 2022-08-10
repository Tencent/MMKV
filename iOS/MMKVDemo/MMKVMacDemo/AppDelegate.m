//
//  AppDelegate.m
//  MMKVMacDemo
//
//  Created by Ling Guo on 2018/9/27.
//  Copyright © 2018 Lingol. All rights reserved.
//

#import "AppDelegate.h"
#import <MMKV/MMKV.h>

@interface AppDelegate ()

@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    // Insert code here to initialize your application
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    // Insert code here to tear down your application
    [MMKV onAppTerminate];
}

@end
