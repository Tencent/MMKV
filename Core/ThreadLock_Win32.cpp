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

#include "ThreadLock.h"

#if !(MMKV_USING_PTHREAD)

#    include "MMKVLog.h"
#    include <atomic>
#    include <cassert>

namespace mmkv {

ThreadLock::ThreadLock() : m_lock{0} {
}

ThreadLock::~ThreadLock() {
    DeleteCriticalSection(&m_lock);
}

void ThreadLock::initialize() {
    // TODO: a better spin count?
    if (!InitializeCriticalSectionAndSpinCount(&m_lock, 1024)) {
        MMKVError("fail to init critical section:%d", GetLastError());
    }
}

void ThreadLock::lock() {
    EnterCriticalSection(&m_lock);
}

void ThreadLock::unlock() {
    LeaveCriticalSection(&m_lock);
}

void ThreadLock::ThreadOnce(ThreadOnceToken_t *onceToken, void (*callback)()) {
    if (!onceToken || !callback) {
        assert(onceToken);
        assert(callback);
        return;
    }
    while (true) {
        auto expected = ThreadOnceUninitialized;
        atomic_compare_exchange_weak(onceToken, &expected, ThreadOnceInitializing);
        switch (expected) {
            case ThreadOnceInitialized:
                return;
            case ThreadOnceUninitialized:
                callback();
                onceToken->store(ThreadOnceInitialized);
                return;
            case ThreadOnceInitializing: {
                // another thread is initializing, let's wait for 1ms
                ThreadLock::Sleep(1);
                break;
            }
            default: {
                MMKVError("should never happen:%d", expected);
                assert(0);
                return;
            }
        }
    }
}

void ThreadLock::Sleep(int ms) {
    ::Sleep(ms);
}

} // namespace mmkv

#endif // MMKV_USING_PTHREAD
