/*
* Tencent is pleased to support the open source community by making
* MMKV available.
*
* Copyright (C) 2019 THL A29 Limited, a Tencent company.
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

#import "TodayViewController.h"
#import <MMKVAppExtension/MMKV.h>
#import <NotificationCenter/NotificationCenter.h>

@interface TodayViewController () <NCWidgetProviding>
@property (nonatomic) IBOutlet UILabel *m_label;
@end

@implementation TodayViewController {
    NSData *m_encryptionKey;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.

    NSString *groupDir = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:@"group.tencent.mmkv"].path;
    [MMKV initializeMMKV:nil groupDir:groupDir logLevel:MMKVLogInfo];
    m_encryptionKey = [@"multi_process" dataUsingEncoding:NSUTF8StringEncoding];

    // [self testMultiProcess];
}

- (void)widgetPerformUpdateWithCompletionHandler:(void (^)(NCUpdateResult))completionHandler {
    // Perform any setup necessary in order to update the view.

    // If an error is encountered, use NCUpdateResultFailed
    // If there's no update required, use NCUpdateResultNoData
    // If there's an update, use NCUpdateResultNewData
    MMKV *mmkv = [MMKV mmkvWithID:@"multi_process" cryptKey:m_encryptionKey mode:MMKVMultiProcess];
    NSString *newContent = [mmkv getStringForKey:@"content"];
    _m_label.text = newContent;

    completionHandler(NCUpdateResultNewData);
}

- (void)testMultiProcess {
    MMKV *mmkv = [MMKV mmkvWithID:@"multi_process" cryptKey:m_encryptionKey mode:MMKVMultiProcess];

    [mmkv setBool:YES forKey:@"bool"];
    NSLog(@"bool:%d", [mmkv getBoolForKey:@"bool"]);

    [mmkv setInt32:-1024 forKey:@"int32"];
    NSLog(@"int32:%d", [mmkv getInt32ForKey:@"int32"]);

    [mmkv setUInt32:UINT32_MAX forKey:@"uint32"];
    NSLog(@"uint32:%u", [mmkv getUInt32ForKey:@"uint32"]);

    [mmkv setInt64:INT64_MIN forKey:@"int64"];
    NSLog(@"int64:%lld", [mmkv getInt64ForKey:@"int64"]);

    [mmkv setUInt64:UINT64_MAX forKey:@"uint64"];
    NSLog(@"uint64:%llu", [mmkv getInt64ForKey:@"uint64"]);

    [mmkv setFloat:-3.1415926 forKey:@"float"];
    NSLog(@"float:%f", [mmkv getFloatForKey:@"float"]);

    [mmkv setDouble:DBL_MAX forKey:@"double"];
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

    [mmkv removeValueForKey:@"bool"];
    NSLog(@"bool:%d", [mmkv getBoolForKey:@"bool"]);
    [mmkv removeValuesForKeys:@[ @"int32", @"uint64" ]];
    NSLog(@"allKeys %@", [mmkv allKeys]);

    [mmkv close];
}

@end
