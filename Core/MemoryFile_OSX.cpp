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

#include "MemoryFile.h"

#ifdef MMKV_IOS

#    include "MMKVLog.h"

using namespace std;

namespace mmkv {

void tryResetFileProtection(const string &path) {
    @autoreleasepool {
        NSString *nsPath = [NSString stringWithUTF8String:path.c_str()];
        NSDictionary *attr = [[NSFileManager defaultManager] attributesOfItemAtPath:nsPath error:nullptr];
        NSString *protection = [attr valueForKey:NSFileProtectionKey];
        MMKVInfo("protection on [%@] is %@", nsPath, protection);
        if ([protection isEqualToString:NSFileProtectionCompleteUntilFirstUserAuthentication] == NO) {
            NSMutableDictionary *newAttr = [NSMutableDictionary dictionaryWithDictionary:attr];
            [newAttr setObject:NSFileProtectionCompleteUntilFirstUserAuthentication forKey:NSFileProtectionKey];
            NSError *err = nil;
            [[NSFileManager defaultManager] setAttributes:newAttr ofItemAtPath:nsPath error:&err];
            if (err != nil) {
                MMKVError("fail to set attribute %@ on [%@]: %@", NSFileProtectionCompleteUntilFirstUserAuthentication,
                          nsPath, err);
            }
        }
    }
}

} // namespace mmkv

#endif // MMKV_IOS
