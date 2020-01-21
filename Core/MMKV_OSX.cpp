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

#include "MMKVPredef.h"

#ifdef MMKV_IOS_OR_MAC

#    include "CodedInputData.h"
#    include "CodedOutputData.h"
#    include "InterProcessLock.h"
#    include "MMKV.h"
#    include "MemoryFile.h"
#    include "MiniPBCoder.h"
#    include "ScopedLock.hpp"
#    include "ThreadLock.h"

#    ifdef MMKV_IOS
#        include <sys/mman.h>
#    endif

#    if __has_feature(objc_arc)
#        error This file must be compiled with MRC. Use -fno-objc-arc flag.
#    endif

using namespace std;
using namespace mmkv;

extern ThreadLock *g_instanceLock;
extern MMKVPath_t g_rootDir;

MMKV_NAMESPACE_BEGIN

extern ThreadOnceToken_t once_control;
extern void initialize();

void MMKV::minimalInit(MMKVPath_t defaultRootDir) {
    ThreadLock::ThreadOnce(&once_control, initialize);

    g_rootDir = defaultRootDir;
    mkPath(g_rootDir);

    MMKVInfo("default root dir: " MMKV_PATH_FORMAT, g_rootDir.c_str());
}

#    ifdef MMKV_IOS

static bool g_isInBackground = false;

void MMKV::setIsInBackground(bool isInBackground) {
    SCOPED_LOCK(g_instanceLock);

    g_isInBackground = isInBackground;
    MMKVInfo("g_isInBackground:%d", g_isInBackground);
}

// @finally in C++ stype
template <typename F>
struct AtExit {
    AtExit(F f) : m_func{f} {}
    ~AtExit() { m_func(); }

private:
    F m_func;
};

bool MMKV::protectFromBackgroundWriting(size_t size, WriteBlock block) {
    if (g_isInBackground) {
        // calc ptr to be mlock()
        auto writePtr = (size_t) m_output->curWritePointer();
        auto ptr = (writePtr / DEFAULT_MMAP_SIZE) * DEFAULT_MMAP_SIZE;
        size_t lockDownSize = writePtr - ptr + size;
        if (mlock((void *) ptr, lockDownSize) != 0) {
            MMKVError("fail to mlock [%s], %s", m_mmapID.c_str(), strerror(errno));
            // just fail on this condition, otherwise app will crash anyway
            //block(m_output);
            return false;
        } else {
            AtExit cleanup([=] { munlock((void *) ptr, lockDownSize); });
            try {
                block(m_output);
            } catch (std::exception &exception) {
                MMKVError("%s", exception.what());
                return false;
            }
        }
    } else {
        block(m_output);
    }

    return true;
}

#    endif // MMKV_IOS

bool MMKV::set(NSObject<NSCoding> *__unsafe_unretained obj, MMKVKey_t key) {
    if (isKeyEmpty(key)) {
        return false;
    }
    if (!obj) {
        removeValueForKey(key);
        return true;
    }
    MMBuffer data;
    if (MiniPBCoder::isCompatibleObject(obj)) {
        data = MiniPBCoder::encodeDataWithObject(obj);
    } else {
        /*if ([object conformsToProtocol:@protocol(NSCoding)])*/ {
            auto tmp = [NSKeyedArchiver archivedDataWithRootObject:obj];
            if (tmp.length > 0) {
                data = MMBuffer(tmp);
            }
        }
    }

    return setDataForKey(std::move(data), key);
}

NSObject *MMKV::getObject(MMKVKey_t key, Class cls) {
    if (isKeyEmpty(key) || !cls) {
        return nil;
    }
    SCOPED_LOCK(m_lock);
    auto &data = getDataForKey(key);
    if (data.length() > 0) {
        if (MiniPBCoder::isCompatibleClass(cls)) {
            try {
                auto result = MiniPBCoder::decodeObject(data, cls);
                return result;
            } catch (std::exception &exception) {
                MMKVError("%s", exception.what());
            }
        } else {
            if ([cls conformsToProtocol:@protocol(NSCoding)]) {
                auto tmp = [NSData dataWithBytesNoCopy:data.getPtr() length:data.length() freeWhenDone:NO];
                return [NSKeyedUnarchiver unarchiveObjectWithData:tmp];
            }
        }
    }
    return nil;
}

NSArray *MMKV::allKeys() {
    SCOPED_LOCK(m_lock);
    checkLoadData();

    NSMutableArray *keys = [NSMutableArray array];
    for (const auto &pair : m_dic) {
        [keys addObject:pair.first];
    }
    return keys;
}

void MMKV::removeValuesForKeys(NSArray *arrKeys) {
    if (arrKeys.count == 0) {
        return;
    }
    if (arrKeys.count == 1) {
        return removeValueForKey(arrKeys[0]);
    }

    SCOPED_LOCK(m_lock);
    SCOPED_LOCK(m_exclusiveProcessLock);
    checkLoadData();

    size_t deleteCount = 0;
    for (NSString *key in arrKeys) {
        auto itr = m_dic.find(key);
        if (itr != m_dic.end()) {
            [itr->first release];
            m_dic.erase(itr);
            deleteCount++;
        }
    }
    if (deleteCount > 0) {
        m_hasFullWriteback = false;

        fullWriteback();
    }
}

void MMKV::enumerateKeys(EnumerateBlock block) {
    if (block == nil) {
        return;
    }
    SCOPED_LOCK(m_lock);
    checkLoadData();

    MMKVInfo("enumerate [%s] begin", m_mmapID.c_str());
    for (const auto &pair : m_dic) {
        BOOL stop = NO;
        block(pair.first, &stop);
        if (stop) {
            break;
        }
    }
    MMKVInfo("enumerate [%s] finish", m_mmapID.c_str());
}

MMKV_NAMESPACE_END

#endif // MMKV_IOS_OR_MAC
