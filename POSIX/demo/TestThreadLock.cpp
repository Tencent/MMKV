/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2026 THL A29 Limited, a Tencent company.
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
#include "MMKVHandler.h"
#include "MMKVLog.h"

#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <thread>
#include <vector>

using namespace mmkv;

class TestLogHandler final : public MMKVHandler {
public:
    std::atomic<uint32_t> errorCount{0};

    void mmkvLog(MMKVLogLevel level,
                 const char *,
                 int,
                 const char *,
                 MMKVLog_t) override {
        if (level == MMKVLogError) {
            ++errorCount;
        }
    }
};

static void require(bool condition, const char *message) {
    if (!condition) {
        std::fprintf(stderr, "test ThreadLock destruction: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

static void destroyUnlockedLock() {
    auto lock = new ThreadLock();
    delete lock;
}

static void destroyLockedOnce() {
    auto lock = new ThreadLock();
    lock->lock();
    delete lock;
}

static void destroyRecursivelyLocked() {
    auto lock = new ThreadLock();
    lock->lock();
    lock->lock();
    lock->lock();
    delete lock;
}

static void tryLockTracksOwnership() {
    auto lock = new ThreadLock();
#ifndef MMKV_WIN32
    require(lock->try_lock(), "first try_lock failed");
    require(lock->try_lock(), "recursive try_lock failed");
#else
    lock->lock();
    lock->lock();
#endif
    delete lock;
}

static void normalLockStillWorks() {
    ThreadLock lock;
    lock.lock();
    lock.lock();
    lock.unlock();
    lock.unlock();
}

static void concurrentLockingTracksOwnership() {
    constexpr uint32_t threadCount = 8;
    constexpr uint32_t iterationCount = 10000;
    ThreadLock lock;
    uint32_t value = 0;
    std::vector<std::thread> threads;
    threads.reserve(threadCount);

    for (uint32_t thread = 0; thread < threadCount; ++thread) {
        threads.emplace_back([&lock, &value] {
            for (uint32_t iteration = 0; iteration < iterationCount; ++iteration) {
                lock.lock();
                ++value;
                lock.unlock();
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    require(value == threadCount * iterationCount, "concurrent lock lost an update");
}

int main() {
    TestLogHandler logHandler;
    g_handler = &logHandler;

    destroyUnlockedLock();
    destroyLockedOnce();
    destroyRecursivelyLocked();
    tryLockTracksOwnership();
    normalLockStillWorks();
    concurrentLockingTracksOwnership();

    g_handler = nullptr;
    require(logHandler.errorCount == 0, "lock lifecycle logged an error");
    std::puts("test ThreadLock destruction: passed");
    return 0;
}
