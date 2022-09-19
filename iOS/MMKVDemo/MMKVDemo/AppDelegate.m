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

#import "AppDelegate.h"
#import <MMKV/MMKV.h>

@interface AppDelegate () <MMKVHandler>

@end

@implementation AppDelegate

#pragma mark - init MMKV in the main thread

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {

    // usally you can just init MMKV with the default root dir like this
    //[MMKV initializeMMKV:nil];

    // or you can customize MMKV's root dir & group dir
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
    NSString *libraryPath = (NSString *) [paths firstObject];
    if ([libraryPath length] > 0) {
        NSString *rootDir = [libraryPath stringByAppendingPathComponent:@"mmkv"];
        NSString *groupDir = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:@"group.tencent.mmkv"].path;

        // you can turn off logging by passing MMKVLogNone
        // register handler on init
        [MMKV initializeMMKV:rootDir groupDir:groupDir logLevel:MMKVLogInfo handler:self];

        NSLog(@"MMKV version: %@", [MMKV version]);
    }

    // enable auto clean up
    uint32_t maxIdleMinutes = 1;
    [MMKV enableAutoCleanUp:maxIdleMinutes];

    return YES;
}

#pragma mark - MMKVHandler

- (MMKVRecoverStrategic)onMMKVCRCCheckFail:(NSString *)mmapID {
    return MMKVOnErrorRecover;
}

- (MMKVRecoverStrategic)onMMKVFileLengthError:(NSString *)mmapID {
    return MMKVOnErrorRecover;
}

- (void)onMMKVContentChange:(NSString *)mmapID {
    NSLog(@"onMMKVContentChange: %@", mmapID);
}

- (void)mmkvLogWithLevel:(MMKVLogLevel)level file:(const char *)file line:(int)line func:(const char *)funcname message:(NSString *)message {
    const char *levelDesc = NULL;
    switch (level) {
        case MMKVLogDebug:
            levelDesc = "D";
            break;
        case MMKVLogInfo:
            levelDesc = "I";
            break;
        case MMKVLogWarning:
            levelDesc = "W";
            break;
        case MMKVLogError:
            levelDesc = "E";
            break;
        default:
            levelDesc = "N";
            break;
    }

    NSLog(@"redirect logging [%s] <%s:%d::%s> %@", levelDesc, file, line, funcname, message);
}

- (void)applicationWillResignActive:(UIApplication *)application {
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and invalidate graphics rendering callbacks. Games should use this method to pause the game.
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
    // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
    // Called as part of the transition from the background to the active state; here you can undo many of the changes made on entering the background.
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
}

- (void)applicationWillTerminate:(UIApplication *)application {
    // it's totally fine no calling this method
    [MMKV onAppTerminate];
}

@end
