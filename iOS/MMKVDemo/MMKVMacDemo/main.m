//
//  main.m
//  MMKVMacDemo
//
//  Created by Ling Guo on 2018/9/27.
//  Copyright © 2018 Lingol. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <MMKV/MMKV.h>

int main(int argc, const char *argv[]) {
    [MMKV initializeMMKV:nil];
    return NSApplicationMain(argc, argv);
}
