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
#include <cassert>

#if MMKV_USING_PTHREAD

using namespace std;

MyMutex::MyMutex()
: m_RecursionCount(0)
, m_LockCount(0)
, m_Owner()
{}

void MyMutex::lock()
{
    auto current_thread = std::this_thread::get_id();
    std::hash<std::thread::id> thread_hasher;
    size_t thread_hash = thread_hasher(current_thread);
    size_t old_hash;
    
    // Attempt to acquire the mutex.
    while (true)
    {
        size_t old_count = m_LockCount.exchange(1, std::memory_order::memory_order_acquire);
        
        // Freshly acquired the lock, so set the owner
        if (old_count == 0)
        {
            assert(m_RecursionCount == 0);
            m_Owner.store(thread_hash, std::memory_order::memory_order_relaxed);
            break;
        }
        
        // Lock is already acquired, must be calling it recursively to be acquiring it
        if (old_count == 1 && m_Owner.load(std::memory_order::memory_order_relaxed) == thread_hash)
        {
            assert(m_RecursionCount > 0);
            break;
        }
    }
    
    // Mutex acquired, increment the recursion counter
    m_RecursionCount += 1;
}


void MyMutex::unlock()
{
    auto current_thread = std::this_thread::get_id();
    std::hash<std::thread::id> thread_hasher;
    assert(m_Owner.load() == thread_hasher(current_thread));

    // decrement the recursion count and if we reach 0, actually release the lock
    m_RecursionCount -= 1;
    if (m_RecursionCount == 0)
    {
        m_Owner.store(thread_hasher(std::thread::id()), std::memory_order::memory_order_relaxed);
        m_LockCount.exchange(0, std::memory_order::memory_order_release);
    }
}

bool MyMutex::try_lock() {
    lock();
    return true;
}

namespace mmkv {
#if MMKV_USING_STD_MUTEX

ThreadLock::ThreadLock() {
}

ThreadLock::~ThreadLock() {
}

void ThreadLock::lock() {
    m_lock.lock();
}
void ThreadLock::unlock() {
    m_lock.unlock();
}

bool ThreadLock::try_lock() {
    return m_lock.try_lock();
}

#else // MMKV_USING_STD_MUTEX

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

#endif // !MMKV_USING_STD_MUTEX

void ThreadLock::initialize() {
    return;
}

void ThreadLock::ThreadOnce(ThreadOnceToken_t *onceToken, void (*callback)()) { pthread_once(onceToken, callback); }

} // namespace mmkv

#endif // MMKV_USING_PTHREAD
