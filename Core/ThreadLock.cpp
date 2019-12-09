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

#include "ThreadLock.h"
#include "MMKVLog.h"
#include <atomic>

using namespace std;

namespace mmkv {

ThreadLock::ThreadLock() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    pthread_mutex_init(&m_lock, &attr);

    pthread_mutexattr_destroy(&attr);
}

ThreadLock::~ThreadLock() {
    pthread_mutex_destroy(&m_lock);
}

void ThreadLock::lock() {
    auto ret = pthread_mutex_lock(&m_lock);
    if (ret != 0) {
        MMKVError("fail to lock %p, ret=%d, errno=%s", &m_lock, ret, strerror(errno));
    }
}

bool ThreadLock::try_lock() {
    auto ret = pthread_mutex_trylock(&m_lock);
    if (ret != 0) {
        MMKVError("fail to try lock %p, ret=%d, errno=%s", &m_lock, ret, strerror(errno));
    }
    return (ret == 0);
}

void ThreadLock::unlock() {
    auto ret = pthread_mutex_unlock(&m_lock);
    if (ret != 0) {
        MMKVError("fail to unlock %p, ret=%d, errno=%s", &m_lock, ret, strerror(errno));
    }
}

#if MMKV_USING_PTHREAD
void ThreadLock::ThreadOnce(ThreadOnceToken *onceToken, void (*callback)()) {
    pthread_once(onceToken, callback);
}
#    ifndef NDEBUG
static uint64_t gettid() {
#        if MMKV_MAC
    uint64_t tid = 0;
    pthread_threadid_np(nullptr, &tid);
    return tid;
#        else
    return gettid();
#        endif
}
#    endif
#else
void ThreadLock::ThreadOnce(ThreadOnceToken *onceToken, void (*callback)()) {
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
                Sleep(1);
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
    constexpr auto MILLI_SECOND_MULTIPLIER = 1000;
    constexpr auto NANO_SECOND_MULTIPLIER = MILLI_SECOND_MULTIPLIER * MILLI_SECOND_MULTIPLIER;
    timespec duration = {0};
    if (ms > 999) {
        duration.tv_sec = ms / MILLI_SECOND_MULTIPLIER;
        duration.tv_nsec = (ms % MILLI_SECOND_MULTIPLIER) * NANO_SECOND_MULTIPLIER;
    } else {
        duration.tv_nsec = ms * NANO_SECOND_MULTIPLIER;
    }
    nanosleep(&duration, nullptr);
}

#endif

} // namespace mmkv
