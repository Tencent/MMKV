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

#ifndef MMKV_THREADLOCK_H
#define MMKV_THREADLOCK_H
#include <linux/futex.h>
#ifdef  __cplusplus

#include "MMKVPredef.h"

#ifndef MMKV_WIN32
#    include <pthread.h>
#    define MMKV_USING_PTHREAD 1
#    ifdef MMKV_OHOS
// #        define MMKV_USING_STD_MUTEX 1
#    endif
#endif

#if MMKV_USING_PTHREAD
#    include <atomic>
#endif

#include <thread>

class MyMutex {
public:
    MyMutex();

    MyMutex(MyMutex const &) = delete;

    MyMutex &operator=(MyMutex) = delete;

    /**
     * Acquires exclusive access to this mutex.
     * This is recursion-safe, i.e. from one thread it can be called multiple
     * times.
     */
    void lock();

    bool try_lock();

    /**
     * Releases a previously acquired lock.
     * Number of calls should match acquire() to fully release access.
     */
    void unlock();
    
protected:
    size_t m_RecursionCount;
    std::atomic<size_t> m_LockCount;
    std::atomic<size_t> m_Owner;
};

static inline uintptr_t __get_tp() {
    uintptr_t tp;
#if defined(__aarch64__)
    __asm__("mrs %0,tpidr_el0" : "=r"(tp));
#endif
    return tp;
}

#define TLS_ABOVE_TP
#define GAP_ABOVE_TP 16

#define MC_PC pc

struct MPthread {
	/* Part 1 -- these fields may be external or
	 * internal (accessed via asm) ABI. Do not change. */
	struct MPthread *self;
#ifndef TLS_ABOVE_TP
	uintptr_t *dtv;
#endif
	struct MPthread *prev, *next; /* non-ABI */
	uintptr_t sysinfo;
#ifndef TLS_ABOVE_TP
#ifdef CANARY_PAD
	uintptr_t canary_pad;
#endif
	uintptr_t canary;
#endif

	/* Part 2 -- implementation details, non-ABI. */
	int tid;
	int errno_val;
	volatile int detach_state;
	volatile int cancel;
	volatile unsigned char canceldisable, cancelasync;
	unsigned char tsd_used:1;
	unsigned char dlerror_flag:1;
	unsigned char *map_base;
	size_t map_size;
	void *stack;
	size_t stack_size;
	size_t guard_size;
	void *result;
	struct __ptcb *cancelbuf;
	void **tsd;
	struct {
		volatile void *volatile head;
		long off;
		volatile void *volatile pending;
	} robust_list;
	int h_errno_val;
	volatile int timer_id;
	locale_t locale;
	volatile int killlock[1];
	char *dlerror_buf;
	void *stdio_locks;

	/* Part 3 -- the positions of these fields relative to
	 * the end of the structure is external and internal ABI. */
#ifdef TLS_ABOVE_TP
	uintptr_t canary;
	uintptr_t *dtv;
#endif
};

static_assert(offsetof(MPthread, robust_list) == 0x78, "size error");

#ifndef TP_OFFSET
// #define TP_OFFSET 0
#define TP_OFFSET 128
#endif

#ifdef TLS_ABOVE_TP
#define TP_ADJ(p) ((char *)(p) + sizeof(struct MPthread) + TP_OFFSET)
#define __pthread_self() ((MPthread *)(__get_tp() - sizeof(struct MPthread) - TP_OFFSET))
#else
#define TP_ADJ(p) (p)
#define __pthread_self() ((pthread_t)__get_tp())
#endif

static_assert(sizeof(struct MPthread) + TP_OFFSET == 0x148, "size error");
// char (*__kaboom)[sizeof(struct MPthread) + TP_OFFSET] = 1;

struct MPthread_mutex_t {
    union {
        int __i[10];
        volatile int __vi[10];
        volatile void *volatile __p[5];

        struct {
            int32_t m_type;             // #define _m_type __u.__i[0]
            volatile int32_t m_lock;    // #define _m_lock __u.__vi[1]
            volatile int32_t m_waiters; // #define _m_waiters __u.__vi[2]
            int32_t __holder_i3;
            int32_t __holder_i4;
            int32_t m_count;            // #define _m_count __u.__i[5]
            volatile void *m_prev;      // #define _m_prev __u.__p[3]
            volatile void *m_next;      // #define _m_next __u.__p[4]
        } wrap;
    } __u;
};

namespace mmkv {

#if MMKV_USING_PTHREAD
#    define ThreadOnceToken_t pthread_once_t
#    define ThreadOnceUninitialized PTHREAD_ONCE_INIT
#else
enum ThreadOnceTokenEnum : int32_t { ThreadOnceUninitialized = 0, ThreadOnceInitializing, ThreadOnceInitialized };
using ThreadOnceToken_t = std::atomic<ThreadOnceTokenEnum>;
#endif

class ThreadLock {
public:
#if MMKV_USING_PTHREAD
#   if MMKV_USING_STD_MUTEX
    // looks like there's bug on pthread_mutex_lock() on OHOS, use custom mutex instead
    MyMutex m_lock;
#   else
    pthread_mutex_t m_lock;
    MPthread_mutex_t *m_lockPtr;
#   endif
#else
    CRITICAL_SECTION m_lock;
#endif

public:
    ThreadLock();
    ~ThreadLock();

    void initialize();

    void lock();
    void unlock();

#ifndef MMKV_WIN32
    bool try_lock();
#endif

    static void ThreadOnce(ThreadOnceToken_t *onceToken, void (*callback)(void));

#ifdef MMKV_WIN32
    static void Sleep(int ms);
#endif

    // just forbid it for possibly misuse
    explicit ThreadLock(const ThreadLock &other) = delete;
    ThreadLock &operator=(const ThreadLock &other) = delete;
};

} // namespace mmkv

#endif
#endif //MMKV_THREADLOCK_H
