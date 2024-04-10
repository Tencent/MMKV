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

void dumpMutex(MPthread_mutex_t *mutex, const char *msg) {
    MPthread *pSelf = __pthread_self();
    auto &wrap = mutex->__u.wrap;
    MMKVInfo("pthread_mutex: %p [%s], m_type %d, m_prev %p, m_next %p, robust_list: {%p, 0x%llx, %p}",
        mutex, msg, wrap.m_type, wrap.m_prev, wrap.m_next,
        pSelf->robust_list.head, pSelf->robust_list.off, pSelf->robust_list.pending);
}

ThreadLock::ThreadLock() : m_lock({}), m_lockPtr((MPthread_mutex_t *)&m_lock) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    dumpMutex(m_lockPtr, "before init");
    pthread_mutex_init(&m_lock, &attr);
    dumpMutex(m_lockPtr, "after init");

    pthread_mutexattr_destroy(&attr);
}

ThreadLock::~ThreadLock() {
#ifdef MMKV_OHOS
    dumpMutex(m_lockPtr, "before unlock");
    pthread_mutex_unlock(&m_lock);
    dumpMutex(m_lockPtr, "after unlock");
#endif
    dumpMutex(m_lockPtr, "before destroy");
    pthread_mutex_destroy(&m_lock);
    dumpMutex(m_lockPtr, "after destroy");
}

void ThreadLock::lock() {
    dumpMutex(m_lockPtr, "before lock");
    auto ret = pthread_mutex_lock(&m_lock);
    dumpMutex(m_lockPtr, "after lock");
    if (ret != 0) {
            MMKVError("fail to lock %p, ret=%d, errno=%s", &m_lock, ret, strerror(errno));
    }
}

void ThreadLock::unlock() {
    dumpMutex(m_lockPtr, "before unlock");
    auto ret = pthread_mutex_unlock(&m_lock);
    dumpMutex(m_lockPtr, "after unlock");
    if (ret != 0) {
            MMKVError("fail to unlock %p, ret=%d, errno=%s", &m_lock, ret, strerror(errno));
    }
}

bool ThreadLock::try_lock() {
    dumpMutex(m_lockPtr, "before try_lock");
    auto ret = pthread_mutex_trylock(&m_lock);
    dumpMutex(m_lockPtr, "after try_lock");
    return (ret == 0);
}

#endif // !MMKV_USING_STD_MUTEX

void ThreadLock::initialize() {
    return;
}

void ThreadLock::ThreadOnce(ThreadOnceToken_t *onceToken, void (*callback)()) { pthread_once(onceToken, callback); }

void dumpMutex(pthread_mutex_t *m, const char *msg) {
    MPthread_mutex_t *mutex = (MPthread_mutex_t *)m;
    MPthread *pSelf = __pthread_self();
    auto &wrap = mutex->__u.wrap;
    MMKVInfo("pthread_mutex: %p [%s], m_type %d, m_prev %p, m_next %p, robust_list: {%p, 0x%llx, %p}", mutex, msg,
        wrap.m_type, wrap.m_prev, wrap.m_next, pSelf->robust_list.head, pSelf->robust_list.off,
        pSelf->robust_list.pending);
}

static void pthreadMutexMemoryCorruptPOC() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    pthread_mutex_t stackMutex = {};
    pthread_mutex_init(&stackMutex, &attr);

    size_t size = sizeof(pthread_mutex_t);
    pthread_mutex_t *heapMutex = (pthread_mutex_t *)malloc(size);
    pthread_mutex_init(heapMutex, &attr);
    pthread_mutexattr_destroy(&attr);

    pthread_mutex_lock(&stackMutex);
    dumpMutex(&stackMutex, "lock");

    pthread_mutex_lock(heapMutex);
    dumpMutex(heapMutex, "lock");
    // pthread_mutex_lock(heapMutex);
    // dumpMutex(heapMutex, "lock");
    pthread_mutex_unlock(heapMutex);
    dumpMutex(heapMutex, "unlock");
    pthread_mutex_unlock(heapMutex);
    dumpMutex(heapMutex, "unlock");

    pthread_mutex_destroy(heapMutex);
    dumpMutex(heapMutex, "destroy");
    free(heapMutex);

    pthread_mutex_unlock(&stackMutex);
    dumpMutex(&stackMutex, "unlock");

    size_t count = size / sizeof(char);
    char *buffer = (char *)calloc(count, sizeof(char));
    MMKVInfo("heapMutex %p, buffer %p", heapMutex, buffer);

    pthread_mutex_lock(&stackMutex);
    dumpMutex(&stackMutex, "lock");

    for (size_t index = 0; index < count; index++) {
        if (buffer[index] != 0) {
            abort();
        }
    }
}

} // namespace mmkv

#endif // MMKV_USING_PTHREAD
