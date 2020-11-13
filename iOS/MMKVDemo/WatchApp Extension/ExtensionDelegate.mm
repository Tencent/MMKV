/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2020 THL A29 Limited, a Tencent company.
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

#import "ExtensionDelegate.h"
#import <MMKVWatchExtension/MMKV.h>

@implementation ExtensionDelegate

- (void)applicationDidFinishLaunching {
    // Perform any final initialization of your application.
    [MMKV initializeMMKV:nil];

    [self funcionalTest];
}

- (void)funcionalTest {
    auto path = [MMKV mmkvBasePath];
    path = [path stringByDeletingLastPathComponent];
    path = [path stringByAppendingPathComponent:@"mmkv_2"];
    auto mmkv = [MMKV mmkvWithID:@"test/case1" rootPath:path];

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
    NSLog(@"data length:%zu, value size consumption:%zu", data.length, [mmkv getValueSizeForKey:@"data" actualSize:NO]);

    [mmkv setObject:[NSData data] forKey:@"data"];
    NSLog(@"data after set empty data:%@, containsKey:%d",
          [mmkv getObjectOfClass:NSData.class
                          forKey:@"data"],
          [mmkv containsKey:@"data"]);

    [mmkv removeValueForKey:@"bool"];
    NSLog(@"bool:%d", [mmkv getBoolForKey:@"bool"]);
    [mmkv removeValuesForKeys:@[ @"int32", @"uint64" ]];
    NSLog(@"allKeys %@", [mmkv allKeys]);

    [mmkv close];
}

- (void)applicationDidBecomeActive {
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
}

- (void)applicationWillResignActive {
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, etc.
}

- (void)handleBackgroundTasks:(NSSet<WKRefreshBackgroundTask *> *)backgroundTasks {
    // Sent when the system needs to launch the application in the background to process tasks. Tasks arrive in a set, so loop through and process each one.
    for (WKRefreshBackgroundTask * task in backgroundTasks) {
        // Check the Class of each task to decide how to process it
        if ([task isKindOfClass:[WKApplicationRefreshBackgroundTask class]]) {
            // Be sure to complete the background task once you’re done.
            WKApplicationRefreshBackgroundTask *backgroundTask = (WKApplicationRefreshBackgroundTask*)task;
            [backgroundTask setTaskCompletedWithSnapshot:NO];
        } else if ([task isKindOfClass:[WKSnapshotRefreshBackgroundTask class]]) {
            // Snapshot tasks have a unique completion call, make sure to set your expiration date
            WKSnapshotRefreshBackgroundTask *snapshotTask = (WKSnapshotRefreshBackgroundTask*)task;
            [snapshotTask setTaskCompletedWithDefaultStateRestored:YES estimatedSnapshotExpiration:[NSDate distantFuture] userInfo:nil];
        } else if ([task isKindOfClass:[WKWatchConnectivityRefreshBackgroundTask class]]) {
            // Be sure to complete the background task once you’re done.
            WKWatchConnectivityRefreshBackgroundTask *backgroundTask = (WKWatchConnectivityRefreshBackgroundTask*)task;
            [backgroundTask setTaskCompletedWithSnapshot:NO];
        } else if ([task isKindOfClass:[WKURLSessionRefreshBackgroundTask class]]) {
            // Be sure to complete the background task once you’re done.
            WKURLSessionRefreshBackgroundTask *backgroundTask = (WKURLSessionRefreshBackgroundTask*)task;
            [backgroundTask setTaskCompletedWithSnapshot:NO];
        } else if ([task isKindOfClass:[WKRelevantShortcutRefreshBackgroundTask class]]) {
            // Be sure to complete the relevant-shortcut task once you’re done.
            WKRelevantShortcutRefreshBackgroundTask *relevantShortcutTask = (WKRelevantShortcutRefreshBackgroundTask*)task;
            [relevantShortcutTask setTaskCompletedWithSnapshot:NO];
        } else if ([task isKindOfClass:[WKIntentDidRunRefreshBackgroundTask class]]) {
            // Be sure to complete the intent-did-run task once you’re done.
            WKIntentDidRunRefreshBackgroundTask *intentDidRunTask = (WKIntentDidRunRefreshBackgroundTask*)task;
            [intentDidRunTask setTaskCompletedWithSnapshot:NO];
        } else {
            // make sure to complete unhandled task types
            [task setTaskCompletedWithSnapshot:NO];
        }
    }
}

@end
