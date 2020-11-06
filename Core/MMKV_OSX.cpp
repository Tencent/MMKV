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

#ifdef MMKV_APPLE

#    include "CodedInputData.h"
#    include "CodedOutputData.h"
#    include "InterProcessLock.h"
#    include "MMKV.h"
#    include "MemoryFile.h"
#    include "MiniPBCoder.h"
#    include "PBUtility.h"
#    include "ScopedLock.hpp"
#    include "ThreadLock.h"
#    include "aes/AESCrypt.h"
#    include <sys/utsname.h>

#    ifdef MMKV_IOS
#        include "MMKV_OSX.h"
#        include <sys/mman.h>
#    endif

#    ifdef __aarch64__
#        include "Checksum.h"
#    endif

#    if __has_feature(objc_arc)
#        error This file must be compiled with MRC. Use -fno-objc-arc flag.
#    endif

using namespace std;
using namespace mmkv;

extern ThreadLock *g_instanceLock;
extern MMKVPath_t g_rootDir;

enum { UnKnown = 0, PowerMac = 1, Mac, iPhone, iPod, iPad, AppleTV, AppleWatch };
static void GetAppleMachineInfo(int &device, int &version);

MMKV_NAMESPACE_BEGIN

#    ifdef MMKV_IOS
MLockPtr::MLockPtr(void *ptr, size_t size) : m_lockDownSize(0), m_lockedPtr(nullptr) {
    if (!ptr || size == 0) {
        return;
    }
    // calc ptr to mlock()
    auto writePtr = (size_t) ptr;
    auto lockPtr = (writePtr / DEFAULT_MMAP_SIZE) * DEFAULT_MMAP_SIZE;
    auto lockDownSize = writePtr - lockPtr + size;
    if (mlock((void *) lockPtr, lockDownSize) == 0) {
        m_lockedPtr = (uint8_t *) lockPtr;
        m_lockDownSize = lockDownSize;
    } else {
        MMKVError("fail to mlock [%p], %s", m_lockedPtr, strerror(errno));
        // just fail on this condition, otherwise app will crash anyway
    }
}

MLockPtr::MLockPtr(MLockPtr &&other) : m_lockDownSize(other.m_lockDownSize), m_lockedPtr(other.m_lockedPtr) {
    other.m_lockedPtr = nullptr;
}

MLockPtr::~MLockPtr() {
    if (m_lockedPtr) {
        munlock(m_lockedPtr, m_lockDownSize);
    }
}

#    endif

extern ThreadOnceToken_t once_control;
extern void initialize();

void MMKV::minimalInit(MMKVPath_t defaultRootDir) {
    ThreadLock::ThreadOnce(&once_control, initialize);

    // crc32 instruction requires A10 chip, aka iPhone 7 or iPad 6th generation
    int device = 0, version = 0;
    GetAppleMachineInfo(device, version);
    MMKVInfo("Apple Device:%d, version:%d", device, version);

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

bool MMKV::isInBackground() {
    SCOPED_LOCK(g_instanceLock);

    return g_isInBackground;
}

pair<bool, MLockPtr> guardForBackgroundWriting(void *ptr, size_t size) {
    if (g_isInBackground) {
        MLockPtr mlockPtr(ptr, size);
        return make_pair(mlockPtr.isLocked(), move(mlockPtr));
    } else {
        return make_pair(true, MLockPtr(nullptr, 0));
    }
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

    NSData *tmpData = nil;
    if ([obj isKindOfClass:NSString.class]) {
        auto str = (NSString *) obj;
        tmpData = [str dataUsingEncoding:NSUTF8StringEncoding];
    } else if ([obj isKindOfClass:NSData.class]) {
        tmpData = (NSData *) obj;
    }
    if (tmpData) {
        // delay write the size needed for encoding tmpData
        // avoid memory copying
        return setDataForKey(MMBuffer(tmpData, MMBufferNoCopy), key, true);
    } else if ([obj isKindOfClass:NSDate.class]) {
        NSDate *oDate = (NSDate *) obj;
        double time = oDate.timeIntervalSince1970;
        return set(time, key);
    } else {
        /*if ([object conformsToProtocol:@protocol(NSCoding)])*/ {
            auto tmp = [NSKeyedArchiver archivedDataWithRootObject:obj];
            if (tmp.length > 0) {
                return setDataForKey(MMBuffer(tmp, MMBufferNoCopy), key);
            }
        }
    }
    return false;
}

NSObject *MMKV::getObject(MMKVKey_t key, Class cls) {
    if (isKeyEmpty(key) || !cls) {
        return nil;
    }
    SCOPED_LOCK(m_lock);
    auto data = getDataForKey(key);
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

#    ifndef MMKV_DISABLE_CRYPT

constexpr uint32_t Fixed32Size = pbFixed32Size();

pair<bool, KeyValueHolder>
MMKV::appendDataWithKey(const MMBuffer &data, MMKVKey_t key, const KeyValueHolderCrypt &kvHolder, bool isDataHolder) {
    if (kvHolder.type != KeyValueHolderType_Offset) {
        return appendDataWithKey(data, key, isDataHolder);
    }
    SCOPED_LOCK(m_exclusiveProcessLock);

    uint32_t keyLength = kvHolder.keySize;
    // size needed to encode the key
    size_t rawKeySize = keyLength + pbRawVarint32Size(keyLength);

    auto basePtr = (uint8_t *) m_file->getMemory() + Fixed32Size;
    MMBuffer keyData(rawKeySize);
    AESCrypt decrypter = m_crypter->cloneWithStatus(kvHolder.cryptStatus);
    decrypter.decrypt(basePtr + kvHolder.offset, keyData.getPtr(), rawKeySize);

    return doAppendDataWithKey(data, keyData, isDataHolder, keyLength);
}
#    endif

NSArray *MMKV::allKeys() {
    SCOPED_LOCK(m_lock);
    checkLoadData();

    NSMutableArray *keys = [NSMutableArray array];
    if (m_crypter) {
        for (const auto &pair : *m_dicCrypt) {
            [keys addObject:pair.first];
        }
    } else {
        for (const auto &pair : *m_dic) {
            [keys addObject:pair.first];
        }
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
    if (m_crypter) {
        for (NSString *key in arrKeys) {
            auto itr = m_dicCrypt->find(key);
            if (itr != m_dicCrypt->end()) {
                auto oldKey = itr->first;
                m_dicCrypt->erase(itr);
                [oldKey release];
                deleteCount++;
            }
        }
    } else {
        for (NSString *key in arrKeys) {
            auto itr = m_dic->find(key);
            if (itr != m_dic->end()) {
                auto oldKey = itr->first;
                m_dic->erase(itr);
                [oldKey release];
                deleteCount++;
            }
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
    if (m_crypter) {
        for (const auto &pair : *m_dicCrypt) {
            BOOL stop = NO;
            block(pair.first, &stop);
            if (stop) {
                break;
            }
        }
    } else {
        for (const auto &pair : *m_dic) {
            BOOL stop = NO;
            block(pair.first, &stop);
            if (stop) {
                break;
            }
        }
    }
    MMKVInfo("enumerate [%s] finish", m_mmapID.c_str());
}

MMKV_NAMESPACE_END

#    include <sys/sysctl.h>

static void GetAppleMachineInfo(int &device, int &version) {
    device = UnKnown;
    version = 0;

#    if 0
    struct utsname systemInfo = {};
    uname(&systemInfo);
    std::string machine(systemInfo.machine);
#    else
    size_t size;
    sysctlbyname("hw.machine", nullptr, &size, nullptr, 0);
    char *answer = (char *) malloc(size);
    sysctlbyname("hw.machine", answer, &size, nullptr, 0);
    std::string machine(answer);
    free(answer);
#    endif

    if (machine.find("PowerMac") != std::string::npos || machine.find("Power Macintosh") != std::string::npos) {
        device = PowerMac;
    } else if (machine.find("Mac") != std::string::npos || machine.find("Macintosh") != std::string::npos) {
        device = Mac;
    } else if (machine.find("iPhone") != std::string::npos) {
        device = iPhone;
    } else if (machine.find("iPod") != std::string::npos) {
        device = iPod;
    } else if (machine.find("iPad") != std::string::npos) {
        device = iPad;
    } else if (machine.find("AppleTV") != std::string::npos) {
        device = AppleTV;
    } else if (machine.find("AppleWatch") != std::string::npos) {
        device = AppleWatch;
    }
    auto pos = machine.find_first_of("0123456789");
    if (pos != std::string::npos) {
        version = std::atoi(machine.substr(pos).c_str());
    }
}

#endif // MMKV_APPLE
