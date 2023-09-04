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

#import "ViewController+TestCaseBad.h"
#import <MMKV/MMKV.h>
#import <string>
#import <vector>

@implementation ViewController (TestCaseBad)

- (void)testCornerSize {
    // auto mmkv = [MMKV mmkvWithID:@"test/cornerSize" cryptKey:[@"crypt" dataUsingEncoding:NSUTF8StringEncoding]];
    auto mmkv = [MMKV mmkvWithID:@"test/cornerSize1"];
    [mmkv clearAll];
    auto size = getpagesize() - 2;
    size -= 4;
    NSString *key = @"key";
    auto keySize = 3 + 1;
    size -= keySize;
    auto valueSize = 3;
    size -= valueSize;
    NSData *value = [NSMutableData dataWithLength:size];
    [mmkv setObject:value forKey:key];
}

- (void)testFastRemoveCornerSize {
    // auto mmkv = [MMKV mmkvWithID:@"test/FastRemoveCornerSize" cryptKey:[@"crypt" dataUsingEncoding:NSUTF8StringEncoding]];
    auto mmkv = [MMKV mmkvWithID:@"test/FastRemoveCornerSize1"];
    [mmkv clearAll];
    auto size = getpagesize() - 4;
    size -= 4;
    NSString *key = @"key";
    auto keySize = 3 + 1;
    size -= keySize;
    auto valueSize = 3;
    size -= valueSize;
    size -= (keySize + 1); // total size of fast remove
    size /= 16;
    NSMutableData *value = [NSMutableData dataWithLength:size];
    auto ptr = (char *) value.mutableBytes;
    for (int i = 0; i < value.length; i++) {
        ptr[i] = 'A';
    }
    for (int i = 0; i < 16; i++) {
        [mmkv setObject:value forKey:key]; // when a full write back is occur, here's corruption happens
        [mmkv removeValueForKey:key];
    }
}

- (void)testChineseCharKey {
    std::vector<std::tuple<std::string, size_t, size_t>> fakeKeyValues = {
        {"UUID", 36, 37},
        {"webViewShake", 1, 2},
        {"install_now", 1, 1},
        {"HomeContainerViewControllerGuide", 32, 33},
        {"AdvertisingKey", 397, 397},
        {"PrivacyPolicy", 1, 1},
        {"appVersion", 6, 7},
        {"IS_RECEIVE_PUSH", 1, 1},
        {"invite_url", 0, 1},
        {"kHaveOneDayOrLongTime", 8, 8},
        {"kHaveOneHourOrLongTime", 8, 8},
        {"PO_TYPE", 139, 139},
        {"invite_url", 41, 42},
        {"watermark", 95, 96},
        {"BrowserWhitelist", 253, 253},
        {"open_log", 0, 1},
        {"xz_district_key", 1390, 1390},
        {"open_position", 1, 1},
        {"xz_auth_wechat", 1, 1},
        {"xz_auth_weibo", 0, 1},
        {"xz_auth_qq", 0, 1},
        {"cache_expire", 0, 8},
        {"PopupWindowNotice", 461, 461},
        {"推送通知提示间隔", 1, 1},
        {"版本更新提示间隔", 0, 1},
        {"定位提醒显示间隔", 0, 8},
        {"QFH5_JAVASCRIPT", 2157, 2159},
        {"上次定位_city", 9, 10},
        {"PopupWindowNotice", 0, 0},
    };

    auto mmkv = [MMKV mmkvWithID:@"testChineseCharKey"];
    for (auto &kv : fakeKeyValues) {
        auto key = [NSString stringWithUTF8String:std::get<0>(kv).c_str()];
        auto size = std::get<1>(kv);
        const auto orgSize = std::get<2>(kv);
        if (orgSize == 8) {
            [mmkv setDouble:0.0 forKey:key];
        } else if (size < orgSize) {
            auto buf = [NSMutableData dataWithLength:size];
            [mmkv setObject:buf forKey:key];
        } else if (size == 1) {
            [mmkv setBool:YES forKey:key];
        } else if (size == 0) {
            [mmkv removeValueForKey:key];
        } else if (size > 8) {
            if (orgSize <= 127) {
                size -= 1;
            } else {
                size -= 2;
            }
            auto buf = [NSMutableData dataWithLength:size];
            [mmkv setObject:buf forKey:key];
        } else {
            assert(0);
        }
        auto resultSize = [mmkv getValueSizeForKey:key actualSize:NO];
        assert(resultSize == orgSize);
        NSLog(@"%@, %zu", key, [mmkv actualSize]);
    }
}

- (void)testItemSizeHolderOverride {
    auto mmapID = @"testItemSizeHolderOverride";
    auto cryptKey = nil;
    // auto mmapID = @"testItemSizeHolderOverride_crypt";
    // auto cryptKey = [@"Key_seq_1" dataUsingEncoding:NSUTF8StringEncoding];

    /* do these on v1.1.2
    {
        auto mmkv = [MMKV mmkvWithID:mmapID cryptKey:cryptKey];

        // turn on to check crypt MMKV
        // [mmkv setBool:YES forKey:@"b"];

        // turn on (mutex with the above setBool:forKey testcase) to check crypt MMKV
        // [mmkv setInt32:0xff forKey:@"i"];

        auto value = [NSMutableData dataWithLength:512];
        [mmkv setData:value forKey:@"data"];
        NSLog(@"%@", [mmkv allKeys]);
    }*/

    // do these on v1.2.0
    {
        auto mmkv = [MMKV mmkvWithID:mmapID cryptKey:cryptKey];
        auto actualSize = [mmkv actualSize];
        auto fileSize = [mmkv totalSize];
        // force a fullwrieback()
        auto valueSize = fileSize - actualSize;
        auto value = [NSMutableData dataWithLength:valueSize];
        [mmkv setObject:value forKey:@"bigData"];

        [mmkv clearMemoryCache];

        // it might throw exception
        // you won't find the key "data"
        NSLog(@"%@", [mmkv allKeys]);
    }
}

- (void)testAutoExpireWildPtr {
    NSString *mmapID = @"testAutoExpire";
    auto mmkv = [MMKV mmkvWithID:mmapID];
    [mmkv clearAll];
    [mmkv trim];
    [mmkv enableAutoKeyExpire:1];

    auto size = getpagesize() - 4;
    size -= 4;
    NSString *key = @"key";
    auto keySize = 3 + 1;
    size -= keySize;
    auto valueSize = 3;
    size -= valueSize;
    // size -= (keySize + 1); // total size of fast remove
    size /= 16;
    NSMutableData *value = [NSMutableData dataWithLength:size];
    auto ptr = (char *) value.mutableBytes;
    for (int i = 0; i < value.length; i++) {
        ptr[i] = 'A';
    }
    for (int i = 0; i < 15; i++) {
        [mmkv setObject:value forKey:key]; // when a full write back is occur, here's corruption happens
    }

    sleep(2);
    [mmkv setObject:value forKey:key];
    assert([mmkv containsKey:key]);
}

@end
