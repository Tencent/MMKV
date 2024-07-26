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
    pthread_mutex_unlock(&m_lock);

    pthread_mutex_destroy(&m_lock);
}

void ThreadLock::lock() {
    auto ret = pthread_mutex_lock(&m_lock);
    if (ret != 0) {
        MMKVError("fail to lock %p, ret=%d, errno=%s", &m_lock, ret, strerror(errno));
    }
}

void ThreadLock::unlock() {
    auto ret = pthread_mutex_unlock(&m_lock);
    if (ret != 0) {
        MMKVError("fail to unlock %p, ret=%d, errno=%s", &m_lock, ret, strerror(errno));
    }
}

bool ThreadLock::try_lock() {
    auto ret = pthread_mutex_trylock(&m_lock);
    return (ret == 0);
}

void ThreadLock::initialize() {
    return;
}

void ThreadLock::ThreadOnce(ThreadOnceToken_t *onceToken, void (*callback)()) {
    pthread_once(onceToken, callback);
}

} // namespace mmkv

#endif // MMKV_USING_PTHREAD
