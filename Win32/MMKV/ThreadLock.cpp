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

#include "pch.h"

#include "MMKVLog.h"
#include "ThreadLock.h"

namespace mmkv {

ThreadLock::ThreadLock() : m_lock{0} {}

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

void ThreadLock::ThreadOnce(ThreadOnceToken volatile &onceToken, void (*callback)(void)) {
    if (!callback) {
        return;
    }
    while (true) {
        auto pre = (ThreadOnceToken) InterlockedCompareExchange(
            (volatile LONG *) &onceToken, ThreadOnceInitializing, ThreadOnceUninitialized);
        switch (pre) {
            case ThreadOnceUninitialized:
                callback();
                InterlockedExchange((volatile LONG *) &onceToken, ThreadOnceInitialized);
                return;
            case ThreadOnceInitializing:
                // another thread is initializing, let's wait for 1ms.
                Sleep(1);
                break;
            case ThreadOnceInitialized:
                return;
            default:
                MMKVError("never happen:%ld", pre);
                return;
        }
    }
}

} // namespace mmkv
