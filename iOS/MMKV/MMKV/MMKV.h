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

#import "MMKVHandler.h"

@interface MMKV : NSObject

NS_ASSUME_NONNULL_BEGIN

// a generic purpose instance
+ (instancetype)defaultMMKV;

// mmapID: any unique ID (com.tencent.xin.pay, etc)
// if you want a per-user mmkv, you could merge user-id within mmapID
+ (nullable instancetype)mmkvWithID:(NSString *)mmapID NS_SWIFT_NAME(init(mmapID:));

// mmapID: any unique ID (com.tencent.xin.pay, etc)
// if you want a per-user mmkv, you could merge user-id within mmapID
// cryptKey: 16 byte at most
+ (nullable instancetype)mmkvWithID:(NSString *)mmapID cryptKey:(nullable NSData *)cryptKey NS_SWIFT_NAME(init(mmapID:cryptKey:));

// mmapID: any unique ID (com.tencent.xin.pay, etc)
// if you want a per-user mmkv, you could merge user-id within mmapID
// relativePath: custom path of the file, `NSDocumentDirectory/mmkv` by default
+ (nullable instancetype)mmkvWithID:(NSString *)mmapID relativePath:(nullable NSString *)path NS_SWIFT_NAME(init(mmapID:relativePath:));

// clang-format off

// mmapID: any unique ID (com.tencent.xin.pay, etc)
// if you want a per-user mmkv, you could merge user-id within mmapID
// cryptKey: 16 byte at most
// relativePath: custom path of the file, `NSDocumentDirectory/mmkv` by default
+ (nullable instancetype)mmkvWithID:(NSString *)mmapID cryptKey:(nullable NSData *)cryptKey relativePath:(nullable NSString *)path NS_SWIFT_NAME(init(mmapID:cryptKey:relativePath:));

// clang-format on

// default to `NSDocumentDirectory/mmkv`
+ (NSString *)mmkvBasePath;

// if you want to change the base path, do it BEFORE getting any MMKV instance
// otherwise the behavior is undefined
+ (void)setMMKVBasePath:(NSString *)basePath;

- (BOOL)reKey:(nullable NSData *)newKey NS_SWIFT_NAME(reset(cryptKey:));
- (nullable NSData *)cryptKey;

- (BOOL)setObject:(nullable NSObject<NSCoding> *)object forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setBool:(BOOL)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setInt32:(int32_t)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setUInt32:(uint32_t)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setInt64:(int64_t)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setUInt64:(uint64_t)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setFloat:(float)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setDouble:(double)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setString:(NSString *)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setDate:(NSDate *)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setData:(NSData *)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (nullable id)getObjectOfClass:(Class)cls forKey:(NSString *)key NS_SWIFT_NAME(object(of:forKey:));

- (BOOL)getBoolForKey:(NSString *)key __attribute__((swift_name("bool(forKey:)")));
- (BOOL)getBoolForKey:(NSString *)key defaultValue:(BOOL)defaultValue __attribute__((swift_name("bool(forKey:defaultValue:)")));

- (int32_t)getInt32ForKey:(NSString *)key NS_SWIFT_NAME(int32(forKey:));
- (int32_t)getInt32ForKey:(NSString *)key defaultValue:(int32_t)defaultValue NS_SWIFT_NAME(int32(forKey:defaultValue:));

- (uint32_t)getUInt32ForKey:(NSString *)key NS_SWIFT_NAME(uint32(forKey:));
- (uint32_t)getUInt32ForKey:(NSString *)key defaultValue:(uint32_t)defaultValue NS_SWIFT_NAME(uint32(forKey:defaultValue:));

- (int64_t)getInt64ForKey:(NSString *)key NS_SWIFT_NAME(int64(forKey:));
- (int64_t)getInt64ForKey:(NSString *)key defaultValue:(int64_t)defaultValue NS_SWIFT_NAME(int64(forKey:defaultValue:));

- (uint64_t)getUInt64ForKey:(NSString *)key NS_SWIFT_NAME(uint64(forKey:));
- (uint64_t)getUInt64ForKey:(NSString *)key defaultValue:(uint64_t)defaultValue NS_SWIFT_NAME(uint64(forKey:defaultValue:));

- (float)getFloatForKey:(NSString *)key NS_SWIFT_NAME(float(forKey:));
- (float)getFloatForKey:(NSString *)key defaultValue:(float)defaultValue NS_SWIFT_NAME(float(forKey:defaultValue:));

- (double)getDoubleForKey:(NSString *)key NS_SWIFT_NAME(double(forKey:));
- (double)getDoubleForKey:(NSString *)key defaultValue:(double)defaultValue NS_SWIFT_NAME(double(forKey:defaultValue:));

- (nullable NSString *)getStringForKey:(NSString *)key NS_SWIFT_NAME(string(forKey:));
- (nullable NSString *)getStringForKey:(NSString *)key defaultValue:(nullable NSString *)defaultValue NS_SWIFT_NAME(string(forKey:defaultValue:));

- (nullable NSDate *)getDateForKey:(NSString *)key NS_SWIFT_NAME(date(forKey:));
- (nullable NSDate *)getDateForKey:(NSString *)key defaultValue:(nullable NSDate *)defaultValue NS_SWIFT_NAME(date(forKey:defaultValue:));

- (nullable NSData *)getDataForKey:(NSString *)key NS_SWIFT_NAME(data(forKey:));
- (nullable NSData *)getDataForKey:(NSString *)key defaultValue:(nullable NSData *)defaultValue NS_SWIFT_NAME(data(forKey:defaultValue:));

// return the actual size consumption of the key's value
// Note: might be a little bigger than value's length
- (size_t)getValueSizeForKey:(NSString *)key NS_SWIFT_NAME(valueSize(forKey:));

- (BOOL)containsKey:(NSString *)key NS_SWIFT_NAME(contains(key:));

- (size_t)count;

- (size_t)totalSize;

- (size_t)actualSize;

- (void)enumerateKeys:(void (^)(NSString *key, BOOL *stop))block;

- (void)removeValueForKey:(NSString *)key NS_SWIFT_NAME(removeValue(forKey:));

- (void)removeValuesForKeys:(NSArray<NSString *> *)arrKeys NS_SWIFT_NAME(removeValues(forKeys:));

- (void)clearAll;

// MMKV's size won't reduce after deleting key-values
// call this method after lots of deleting f you care about disk usage
// note that `clearAll` has the similar effect of `trim`
- (void)trim;

// call this method if the instance is no longer needed in the near future
// any subsequent call to the instance is undefined behavior
- (void)close;

// call this method if you are facing memory-warning
// any subsequent call to the instance will load all key-values from file again
- (void)clearMemoryCache;

// you don't need to call this, really, I mean it
// unless you care about out of battery
- (void)sync;

// for CrashProtected Only!!
+ (BOOL)isFileValid:(NSString *)mmapID NS_SWIFT_NAME(isFileValid(for:));
+ (BOOL)isFileValid:(NSString *)mmapID relativePath:(nullable NSString *)path NS_SWIFT_NAME(isFileValid(for:relativePath:));

+ (void)registerHandler:(id<MMKVHandler>)handler;
+ (void)unregiserHandler;

// MMKVLogInfo by default
// MMKVLogNone to disable all logging
+ (void)setLogLevel:(MMKVLogLevel)logLevel;

// Migrate NSUserDefault data to MMKV
// return imported count of key-values
- (uint32_t)migrateFromUserDefaults:(NSUserDefaults *)userDaults NS_SWIFT_NAME(migrateFrom(userDefaults:));

NS_ASSUME_NONNULL_END

@end
