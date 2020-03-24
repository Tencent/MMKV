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

#import <MMKV/MMKV.h>
#import <XCTest/XCTest.h>

@interface MMKVPerformanceTest : XCTestCase

@end

@implementation MMKVPerformanceTest {
    MMKV *mmkv;
}

- (void)setUp {
    [super setUp];

    mmkv = [MMKV mmkvWithID:@"performance_test"];
}

- (void)tearDown {
    mmkv = nil;

    [super tearDown];
}

- (void)testWriteIntPerformance {
    int loops = 10000;
    NSMutableArray *arrIntKeys = [NSMutableArray arrayWithCapacity:loops];
    for (size_t index = 0; index < loops; index++) {
        NSString *intKey = [NSString stringWithFormat:@"int-%zu", index];
        [arrIntKeys addObject:intKey];
    }

    [self measureBlock:^{
        for (int index = 0; index < loops; index++) {
            int32_t tmp = rand();
            NSString *intKey = arrIntKeys[index];
            [self->mmkv setInt32:tmp forKey:intKey];
        }
    }];
}

- (void)testReadIntPerformance {
    int loops = 10000;
    NSMutableArray *arrIntKeys = [NSMutableArray arrayWithCapacity:loops];
    for (size_t index = 0; index < loops; index++) {
        NSString *intKey = [NSString stringWithFormat:@"int-%zu", index];
        [arrIntKeys addObject:intKey];
    }

    [self measureBlock:^{
        for (int index = 0; index < loops; index++) {
            NSString *intKey = arrIntKeys[index];
            [self->mmkv getInt32ForKey:intKey];
        }
    }];
}

- (void)testWriteStringPerformance {
    int loops = 10000;
    NSMutableArray *arrStrings = [NSMutableArray arrayWithCapacity:loops];
    NSMutableArray *arrStrKeys = [NSMutableArray arrayWithCapacity:loops];
    for (size_t index = 0; index < loops; index++) {
        NSString *str = [NSString stringWithFormat:@"%s-%d", __FILE__, rand()];
        [arrStrings addObject:str];

        NSString *strKey = [NSString stringWithFormat:@"str-%zu", index];
        [arrStrKeys addObject:strKey];
    }

    [self measureBlock:^{
        for (int index = 0; index < loops; index++) {
            NSString *str = arrStrings[index];
            NSString *strKey = arrStrKeys[index];
            [self->mmkv setObject:str forKey:strKey];
        }
    }];
}

- (void)testReadStringPerformance {
    int loops = 10000;
    NSMutableArray *arrStrings = [NSMutableArray arrayWithCapacity:loops];
    NSMutableArray *arrStrKeys = [NSMutableArray arrayWithCapacity:loops];
    for (size_t index = 0; index < loops; index++) {
        NSString *str = [NSString stringWithFormat:@"%s-%d", __FILE__, rand()];
        [arrStrings addObject:str];

        NSString *strKey = [NSString stringWithFormat:@"str-%zu", index];
        [arrStrKeys addObject:strKey];
    }

    [self measureBlock:^{
        for (int index = 0; index < loops; index++) {
            NSString *strKey = arrStrKeys[index];
            [self->mmkv getObjectOfClass:NSString.class forKey:strKey];
        }
    }];
}

@end
