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

#if MMKV_USING_PTHREAD

using namespace std;

namespace mmkv {

ThreadLock::ThreadLock() : m_lock({}) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    pthread_mutex_init(&m_lock, &attr);

    pthread_mutexattr_destroy(&attr);
}

ThreadLock::~ThreadLock() {
    while (m_lockCount > 0) {
        auto ret = pthread_mutex_unlock(&m_lock);
        if (ret != 0) {
            MMKVError("fail to unlock %p while destroying, ret=%d, error=%s",
                      &m_lock, ret, strerror(ret));
            break;
        }
        --m_lockCount;
    }

    auto ret = pthread_mutex_destroy(&m_lock);
    if (ret != 0) {
        MMKVError("fail to destroy %p, ret=%d, error=%s", &m_lock, ret, strerror(ret));
    }
}

void ThreadLock::lock() {
    auto ret = pthread_mutex_lock(&m_lock);
    if (ret != 0) {
        MMKVError("fail to lock %p, ret=%d, error=%s", &m_lock, ret, strerror(ret));
        return;
    }
    ++m_lockCount;
}

void ThreadLock::unlock() {
    if (m_lockCount == 0) {
        MMKVError("attempt to unlock unowned lock %p", &m_lock);
        return;
    }
    --m_lockCount;
    auto ret = pthread_mutex_unlock(&m_lock);
    if (ret != 0) {
        ++m_lockCount;
        MMKVError("fail to unlock %p, ret=%d, error=%s", &m_lock, ret, strerror(ret));
        return;
    }
}

bool ThreadLock::try_lock() {
    auto ret = pthread_mutex_trylock(&m_lock);
    if (ret == 0) {
        ++m_lockCount;
        return true;
    }
    return false;
}

void ThreadLock::initialize() {
    return;
}

void ThreadLock::ThreadOnce(ThreadOnceToken_t *onceToken, void (*callback)()) {
    pthread_once(onceToken, callback);
}

} // namespace mmkv

#endif // MMKV_USING_PTHREAD
