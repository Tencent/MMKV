//
// Created by Ling Guo on 2018/4/25.
//

#ifndef MMKV_SCOPEDLOCK_HPP
#define MMKV_SCOPEDLOCK_HPP

#include "MMKVLog.h"

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

    bool try_lock() {
        if (m_lock) {
            return m_lock->try_lock();
        }
        return false;
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

#endif //MMKV_SCOPEDLOCK_HPP
