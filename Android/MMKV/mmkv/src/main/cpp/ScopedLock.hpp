//
// Created by Ling Guo on 2018/4/25.
//

#ifndef MMKV_SCOPEDLOCK_HPP
#define MMKV_SCOPEDLOCK_HPP

#include <pthread.h>

class ScopedLock {
    pthread_mutex_t *m_oLock;

    // just forbid it for possibly misuse
    ScopedLock(const ScopedLock& other) = delete;
    ScopedLock& operator =(const ScopedLock& other) = delete;

public:
    ScopedLock(pthread_mutex_t *oLock)
            : m_oLock(oLock)
    {
        if (m_oLock) {
            pthread_mutex_lock(m_oLock);
        }
    }

    ~ScopedLock() {
        if (m_oLock) {
            pthread_mutex_unlock(m_oLock);
            m_oLock = nullptr;
        }
    }
};


#endif //MMKV_SCOPEDLOCK_HPP
