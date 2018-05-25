//
// Created by Ling Guo on 2018/5/25.
//

#ifndef MMKV_INTERPROCESSLOCK_H
#define MMKV_INTERPROCESSLOCK_H

#include <atomic>
#include <pthread.h>
#include <string>

class MmapedFile;

class InterProcessLock {
    enum {
        UninitializedSegment = 0,
        InitializingSegment,
        InitializedSegment,
    };
#pragma pack(push, 8)
    struct {
        std::atomic<int32_t > m_initStatus;
        pthread_mutex_t m_lock;
    } m_mmaped;
#pragma pack(pop)

    MmapedFile* m_mmapFile;

    void initLock();

    // just forbid it for possibly misuse
    InterProcessLock(const InterProcessLock& other) = delete;
    InterProcessLock& operator =(const InterProcessLock& other) = delete;

public:
    InterProcessLock(const std::string& path);
    ~InterProcessLock();
};


#endif //MMKV_INTERPROCESSLOCK_H
