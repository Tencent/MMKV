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

#ifndef MMKVHandler_h
#define MMKVHandler_h
#import <Foundation/Foundation.h>

typedef enum : NSUInteger {
	MMKVOnErrorDiscard = 0,
	MMKVOnErrorRecover,
} MMKVRecoverStrategic;

// callback is called on the operating thread of the MMKV instance
@protocol MMKVHandler <NSObject>
@optional

// by default MMKV will discard all datas on crc32-check failure
// return `MMKVOnErrorRecover` to recover any data on the file
- (MMKVRecoverStrategic)onMMKVCRCCheckFail:(NSString *)mmapID;

// by default MMKV will discard all datas on file length mismatch
// return `MMKVOnErrorRecover` to recover any data on the file
- (MMKVRecoverStrategic)onMMKVFileLengthError:(NSString *)mmapID;

@end

#endif /* MMKVHandler_h */
