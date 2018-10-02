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

#ifdef __cplusplus

#import "KeyValueHolder.h"
#import "MemoryFile.h"
#import <Foundation/Foundation.h>

@interface MiniPBCoder : NSObject

+ (MMBuffer)encodeDataWithObject:(id)obj;

+ (id)decodeObjectOfClass:(Class)cls fromData:(MMBuffer &)oData;

// for NSDictionary
// note: NSDictionary's key must be NSString
+ (id)decodeContainerOfClass:(Class)cls withValueClass:(Class)valueClass fromData:(MMBuffer &)oData;

+ (void)decodeContainer:(KVItemsWrap &)kvItemsWrap fromMemoryFile:(MemoryFile *)memoryFile fromOffset:(size_t)offset withSize:(size_t)size;

@end

#endif
