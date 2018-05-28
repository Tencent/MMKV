//
// Created by Ling Guo on 2018/5/25.
//

#ifndef MMKV_INTERPROCESSLOCK_H
#define MMKV_INTERPROCESSLOCK_H

#include <pthread.h>
#include <atomic>
#include <string>

class MmapedFile;

class InterProcessLock {
#pragma pack(push, 8)
    struct {
        std::atomic<int32_t > m_initStatus;
        pthread_mutex_t m_lock;
    } *m_mmaped;
#pragma pack(pop)

    MmapedFile* m_mmapFile;

    void initLock();

    // just forbid it for possibly misuse
    InterProcessLock(const InterProcessLock& other) = delete;
    InterProcessLock& operator =(const InterProcessLock& other) = delete;

public:
    InterProcessLock(const std::string& path);
    ~InterProcessLock();

    pthread_mutex_t* getLock();
};


#endif //MMKV_INTERPROCESSLOCK_H
