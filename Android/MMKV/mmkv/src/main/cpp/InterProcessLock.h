//
// Created by Ling Guo on 2018/5/25.
//

#ifndef MMKV_INTERPROCESSLOCK_H
#define MMKV_INTERPROCESSLOCK_H

#include <assert.h>
#include <fcntl.h>

enum LockType {
    SharedLockType,
    ExclusiveLockType,
};

// a recursive POSIX file-lock wrapper
// handles lock upgrade & downgrade correctly
class FileLock {
    int m_fd;
    flock m_lockInfo;
    size_t m_sharedLockCount;
    size_t m_exclusiveLockCount;

    bool doLock(LockType lockType, int cmd);

    // just forbid it for possibly misuse
    FileLock(const FileLock &other) = delete;

    FileLock &operator=(const FileLock &other) = delete;

public:
    FileLock(int fd);

    bool lock(LockType lockType);

    bool try_lock(LockType lockType);

    bool unlock(LockType lockType);
};

class InterProcessLock {
    FileLock *m_fileLock;
    LockType m_lockType;

public:
    InterProcessLock(FileLock *fileLock, LockType lockType)
        : m_fileLock(fileLock), m_lockType(lockType), m_enable(true) {
        assert(m_fileLock);
    }

    bool m_enable;

    void lock() {
        if (m_enable) {
            m_fileLock->lock(m_lockType);
        }
    }

    bool try_lock() {
        if (m_enable) {
            return m_fileLock->try_lock(m_lockType);
        }
        return false;
    }

    void unlock() {
        if (m_enable) {
            m_fileLock->unlock(m_lockType);
        }
    }
};

#endif //MMKV_INTERPROCESSLOCK_H
