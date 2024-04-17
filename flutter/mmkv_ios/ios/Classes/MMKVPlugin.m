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

#import "MMKVPlugin.h"
#import <MMKV/MMKV.h>

@implementation MMKVPlugin

+ (void)registerWithRegistrar:(NSObject<FlutterPluginRegistrar> *)registrar {
    FlutterMethodChannel *channel = [FlutterMethodChannel
        methodChannelWithName:@"mmkv"
              binaryMessenger:[registrar messenger]];
    MMKVPlugin *instance = [[MMKVPlugin alloc] init];
    [registrar addMethodCallDelegate:instance channel:channel];
}

- (void)handleMethodCall:(FlutterMethodCall *)call result:(FlutterResult)result {
    if ([@"initializeMMKV" isEqualToString:call.method]) {
        NSString *rootDir = [call.arguments objectForKey:@"rootDir"];
        NSNumber *logLevel = [call.arguments objectForKey:@"logLevel"];
        NSString *groupDir = [call.arguments objectForKey:@"groupDir"];
        NSString *ret = nil;
        if (groupDir.length > 0) {
            ret = [MMKV initializeMMKV:rootDir groupDir:groupDir logLevel:logLevel.intValue];
        } else {
            ret = [MMKV initializeMMKV:rootDir logLevel:logLevel.intValue];
        }
        result(ret);
    } else {
        result(FlutterMethodNotImplemented);
    }
}

@end
