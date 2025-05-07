/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2020 THL A29 Limited, a Tencent company.
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

#include <MMKV/MMKV.h>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <sys/mman.h>

// it's not a must-have for most app so do it the handy way
#include "../../Core/InterProcessLock.h"

using namespace std;
using namespace mmkv;

#define MMKV_ID "TestInterProcessLock"

sem_t *semEnded = nullptr;
int threadIndex = 0;
FileLock *flockPtr = nullptr;

void *threadFunction(void *lpParam) {
    auto sem = (sem_t *) lpParam;
    sem_post(sem);
    auto index = threadIndex;
    cout << "Thread " << index << " started" << endl;

    //auto mmkv = MMKV::mmkvWithID(MMKV_ID, MMKV_MULTI_PROCESS);
    // this should hold forever
    //mmkv->count();
    flockPtr->lock(SharedLockType);
    //flockPtr->unlock(SharedLockType);

    // something is wrong with inter-process lock
    sem_post(semEnded);
    cout << "Thread " << index << " ended" << endl;
    return nullptr;
}

bool threadTest() {
    sem_t *semParent = sem_open("/mmkv_main", O_CREAT, 0644, 0);

    //auto mmkv = MMKV::mmkvWithID(MMKV_ID, MMKV_MULTI_PROCESS);
    auto fd = open("/tmp/mmkv/TestInterProcessLock.file", O_RDWR | O_CREAT | O_CLOEXEC, S_IRWXU);
    flockPtr = new FileLock(fd);

    sem_post(semParent);
    cout << "Waiting for parent..." << endl;
    usleep(1000);
    sem_wait(semParent);
    sem_close(semParent);
    cout << "Parent locked" << endl;

    semEnded = sem_open(MMKV_ID, O_CREAT, 0644, 0);

    sem_t *semStarted = sem_open("/mmkv_sem_started", O_CREAT, 0644, 0);
    if (!semStarted) {
        printf("fail to create semphare: %d(%s)\n", errno, strerror(errno));
        exit(1);
    }
    for (int index = 0; index < 2; ++index) {
        pthread_t threadHandle;
        pthread_create(&threadHandle, nullptr, threadFunction, semStarted);
        sem_wait(semStarted);
        threadIndex++;
    }
    sem_close(semStarted);

    cout << "Waiting for any child exit..." << endl;
    usleep(1000);
    auto ret = sem_trywait(semEnded);
    sem_close(semEnded);
    cout << "Any child exit: " << (ret == 0) << endl;

    delete flockPtr;
    flockPtr = nullptr;
    //close(fd);

    return (ret != 0);
}

int main() {
    locale::global(locale(""));
    wcout.imbue(locale(""));

    string rootDir = "/tmp/mmkv";
    MMKV::initializeMMKV(rootDir);

    auto processID = getpid();
    cout << "TestInterProcessLock: " << processID << " started\n";

//    auto fd3 = open("/tmp/mmkv/TestInterProcessLock2.file", O_RDWR | O_CREAT | O_CLOEXEC, S_IRWXU);
//    FileLock flock3(fd3);
//
//    auto ptr = ::mmap(nullptr, DEFAULT_MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd3, 0);
//    printf("mmap fd %d to %p\n", fd3, ptr);
//
//    printf("trying flock.try_lock %d\n", fd3);
//    flock3.try_lock(ExclusiveLockType, nullptr);
//    printf("trying flock.lock %d\n", fd3);
//    flock3.lock(ExclusiveLockType);
//    printf("finish flock.lock %d\n", fd3);

    auto ret = threadTest();
    cout << "TestInterProcessLock: " << (ret ? "pass" : "failed") << endl;
    cout << "TestInterProcessLock: " << processID << " ended\n";

    return 0;
}
