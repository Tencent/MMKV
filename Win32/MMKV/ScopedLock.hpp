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

#ifndef MMKV_SCOPEDLOCK_HPP
#define MMKV_SCOPEDLOCK_HPP

#include "MMKVLog.h"

namespace mmkv {

template <typename T>
class ScopedLock {
    T *m_lock;

    // just forbid it for possibly misuse
    ScopedLock(const ScopedLock<T> &other) = delete;

    ScopedLock &operator=(const ScopedLock<T> &other) = delete;

public:
    ScopedLock(T *oLock) : m_lock(oLock) {
        assert(m_lock);
        lock();
    }

    ~ScopedLock() {
        unlock();
        m_lock = nullptr;
    }

    void lock() {
        if (m_lock) {
            m_lock->lock();
        }
    }

    void unlock() {
        if (m_lock) {
            m_lock->unlock();
        }
    }
};

#define SCOPEDLOCK(lock) _SCOPEDLOCK(lock, __COUNTER__)
#define _SCOPEDLOCK(lock, counter) __SCOPEDLOCK(lock, counter)
#define __SCOPEDLOCK(lock, counter) ScopedLock<decltype(lock)> __scopedLock##counter(&lock)

//#include <type_traits>
//#define __SCOPEDLOCK(lock, counter) ScopedLock<std::remove_pointer<decltype(lock)>::type> __scopedLock##counter(lock)

} // namespace mmkv

#endif //MMKV_SCOPEDLOCK_HPP
