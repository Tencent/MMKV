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

#import <Foundation/Foundation.h>

@interface MMKV : NSObject

// a generic purpose instance
+ (instancetype)defaultMMKV;

// mmapID: any unique ID (com.tencent.xin.pay, etc)
// if you want a per-user mmkv, you could merge user-id within mmapID
+ (instancetype)mmkvWithID:(NSString *)mmapID;

// mmapID: any unique ID (com.tencent.xin.pay, etc)
// if you want a per-user mmkv, you could merge user-id within mmapID
// cryptKey: 16 byte at most
+ (instancetype)mmkvWithID:(NSString *)mmapID cryptKey:(NSData *)cryptKey;

- (BOOL)reKey:(NSData *)newKey;
- (NSData *)cryptKey;

// object: NSString/NSData/NSDate
- (BOOL)setObject:(id)obj forKey:(NSString *)key;

- (BOOL)setBool:(BOOL)value forKey:(NSString *)key;

- (BOOL)setInt32:(int32_t)value forKey:(NSString *)key;

- (BOOL)setUInt32:(uint32_t)value forKey:(NSString *)key;

- (BOOL)setInt64:(int64_t)value forKey:(NSString *)key;

- (BOOL)setUInt64:(uint64_t)value forKey:(NSString *)key;

- (BOOL)setFloat:(float)value forKey:(NSString *)key;

- (BOOL)setDouble:(double)value forKey:(NSString *)key;

- (id)getObjectOfClass:(Class)cls forKey:(NSString *)key;

- (bool)getBoolForKey:(NSString *)key;
- (bool)getBoolForKey:(NSString *)key defaultValue:(bool)defaultValue;

- (int32_t)getInt32ForKey:(NSString *)key;
- (int32_t)getInt32ForKey:(NSString *)key defaultValue:(int32_t)defaultValue;

- (uint32_t)getUInt32ForKey:(NSString *)key;
- (uint32_t)getUInt32ForKey:(NSString *)key defaultValue:(uint32_t)defaultValue;

- (int64_t)getInt64ForKey:(NSString *)key;
- (int64_t)getInt64ForKey:(NSString *)key defaultValue:(int64_t)defaultValue;

- (uint64_t)getUInt64ForKey:(NSString *)key;
- (uint64_t)getUInt64ForKey:(NSString *)key defaultValue:(uint64_t)defaultValue;

- (float)getFloatForKey:(NSString *)key;
- (float)getFloatForKey:(NSString *)key defaultValue:(float)defaultValue;

- (double)getDoubleForKey:(NSString *)key;
- (double)getDoubleForKey:(NSString *)key defaultValue:(double)defaultValue;

- (BOOL)containsKey:(NSString *)key;

- (size_t)count;

- (size_t)totalSize;

- (void)enumerateKeys:(void (^)(NSString *key, BOOL *stop))block;

- (void)removeValueForKey:(NSString *)key;

- (void)removeValuesForKeys:(NSArray *)arrKeys;

- (void)clearMemoryCache;

- (void)clearAll;

// you don't need to call this, really, I mean it
// unless you care about out of battery
- (void)sync;

// for CrashProtected Only!!
+ (BOOL)isFileValid:(NSString *)mmapID;

@end
