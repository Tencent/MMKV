/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2019 THL A29 Limited, a Tencent company.
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

#include "InterProcessLock.h"
#include "MMKV.h"
#include <chrono>
#include <cstdio>
#include <iostream>
#include <limits>
#include <pthread.h>
#include <semaphore.h>
#include <string>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <cassert>

using namespace std;
using namespace mmkv;

string to_string(vector<string> &&arr, const char* sp = ", ") {
    string str;
    for (const auto &element : arr) {
        str += element;
        str += sp;
    }
    if (!str.empty()) {
        str.erase(str.length() - strlen(sp));
    }
    return str;
}

void functionalTest(MMKV *mmkv, bool decodeOnly) {
    if (!decodeOnly) {
        mmkv->set(true, "bool");
    }
    cout << "bool = " << mmkv->getBool("bool") << endl;

    if (!decodeOnly) {
        mmkv->set(1024, "int32");
    }
    cout << "int32 = " << mmkv->getInt32("int32") << endl;

    if (!decodeOnly) {
        mmkv->set(numeric_limits<uint32_t>::max(), "uint32");
    }
    cout << "uint32 = " << mmkv->getUInt32("uint32") << endl;

    if (!decodeOnly) {
        mmkv->set(numeric_limits<int64_t>::min(), "int64");
    }
    cout << "int64 = " << mmkv->getInt64("int64") << endl;

    if (!decodeOnly) {
        mmkv->set(numeric_limits<uint64_t>::max(), "uint64");
    }
    cout << "uint64 = " << mmkv->getUInt64("uint64") << endl;

    if (!decodeOnly) {
        mmkv->set(3.14f, "float");
    }
    cout << "float = " << mmkv->getFloat("float") << endl;

    if (!decodeOnly) {
        mmkv->set(numeric_limits<double>::max(), "double");
    }
    cout << "double = " << mmkv->getDouble("double") << endl;

    if (!decodeOnly) {
        mmkv->set("Hello, MMKV-示例 for POSIX", "string");
    }
    string result;
    mmkv->getString("string", result);
    cout << "string = " << result << endl;

    cout << "allKeys: " << ::to_string(mmkv->allKeys()) << endl;
    cout << "count = " << mmkv->count() << ", totalSize = " << mmkv->totalSize() << endl;
    cout << "containsKey[string]: " << mmkv->containsKey("string") << endl;

    mmkv->removeValueForKey("bool");
    cout << "bool: " << mmkv->getBool("bool") << endl;
    mmkv->removeValuesForKeys({"int", "long"});

    mmkv->set("some string", "null string");
    result.erase();
    mmkv->getString("null string", result);
    cout << "string before set null: " << result << endl;
    mmkv->set((const char *) nullptr, "null string");
    //mmkv->set("", "null string");
    result.erase();
    mmkv->getString("null string", result);
    cout << "string after set null: " << result << ", containsKey:" << mmkv->containsKey("null string") << endl;

    //kv.sync();
    //kv.async();
    //kv.clearAll();
    mmkv->clearMemoryCache();
    cout << "allKeys: " << ::to_string(mmkv->allKeys()) << endl;
    cout << "isFileValid[" << mmkv->mmapID() + "]: " << MMKV::isFileValid(mmkv->mmapID()) << endl;
}

constexpr int32_t keyCount = 10000;
constexpr int32_t threadCount = 10;
static const string MMKV_ID = "thread_test1";
vector<string> arrIntKeys;
vector<string> arrStringKeys;

void *threadFunction(void *lpParam) {
    auto threadIndex = (size_t) lpParam;
    auto mmkv = MMKV::mmkvWithID(MMKV_ID);
    mmkv->lock();
    cout << "thread " << threadIndex << " starts" << endl;
    mmkv->unlock();

    auto segmentCount = keyCount / threadCount;
    auto startIndex = segmentCount * threadIndex;
    for (int32_t index = startIndex; index < startIndex + segmentCount; index++) {
        mmkv->set(index, arrIntKeys[index]);
        mmkv->set("str-" + to_string(index), arrStringKeys[index]);
    }

    mmkv->lock();
    cout << "thread " << threadIndex << " ends" << endl;
    mmkv->unlock();
    return nullptr;
}

void threadTest() {
    pthread_t threadHandles[threadCount] = {0};
    for (size_t index = 0; index < threadCount; index++) {
        pthread_create(&threadHandles[index], nullptr, threadFunction, (void *) index);
    }
    for (auto threadHandle : threadHandles) {
        pthread_join(threadHandle, nullptr);
    }

    auto mmkv = MMKV::mmkvWithID(MMKV_ID);
    cout << "total count " << mmkv->count() << endl;
}

void brutleTest() {
    using hclock = chrono::high_resolution_clock;
    auto start = hclock::now();

    auto mmkv = MMKV::mmkvWithID("brutleTest");
    for (int32_t i = 0; i < keyCount; i++) {
        mmkv->set(i, arrIntKeys[i]);
        mmkv->set("str-" + to_string(i), arrStringKeys[i]);
    }

    auto finish = hclock::now();
    long long used = chrono::duration_cast<chrono::milliseconds>(finish - start).count();
    printf("encode int & string %d times, cost: %lld ms\n", keyCount, used);
}

void processTest() {
    constexpr auto processCount = 2;
    pid_t processHandles[processCount] = {0};
    for (int &processHandle : processHandles) {
        auto pid = fork();
        // this is child
        if (pid <= 0) {
            execl("process", "process", nullptr);
            perror("execl"); // execl doesn't return unless there is a problem
            exit(1);
        } else {
            processHandle = pid;
        }
    }

    for (int &processHandle : processHandles) {
        printf("Waiting for child %d ...\n", processHandle);
        auto pid = waitpid(processHandle, nullptr, 0);
        printf("Child quit pid: %d\n", pid);
    }

    auto mmkv = MMKV::mmkvWithID("process_test", MMKV_MULTI_PROCESS);
    cout << "total count of process_test: " << mmkv->count() << endl;
}

void testInterProcessLock() {
    //auto mmkv = MMKV::mmkvWithID("TestInterProcessLock", MMKV_MULTI_PROCESS);
    //mmkv->set(true, "bool");

    auto fd = open("/tmp/mmkv/TestInterProcessLock.file", O_RDWR | O_CREAT | O_CLOEXEC, S_IRWXU);
    FileLock flock(fd);

    auto pid = fork();
    // this is child
    if (pid <= 0) {
        execl("TestInterProcessLock", "TestInterProcessLock", nullptr);
        perror("execl"); // execl doesn't return unless there is a problem
        exit(1);
    }
    printf("Waiting for child %d to start ...\n", pid);
    sem_t *sem = sem_open("/mmkv_main", O_CREAT, 0644, 0);
    if (!sem) {
        printf("fail to create semaphore: %d(%s)\n", errno, strerror(errno));
        exit(1);
    }
    sem_wait(sem);
    printf("Child %d to started\n", pid);

    //mmkv->clearAll();
    //mmkv->lock();
    flock.lock(ExclusiveLockType);

    sem_post(sem);
    sem_close(sem);

    printf("Waiting for child %d to finish...\n", pid);
    waitpid(pid, nullptr, 0);
    printf("Child %d to finished\n", pid);

    //mmkv->unlock();
    flock.unlock(ExclusiveLockType);
    close(fd);
}

void cornetSizeTest() {
    string aesKey = "aes";
    auto mmkv = MMKV::mmkvWithID("cornerSize", MMKV_MULTI_PROCESS, &aesKey);
    mmkv->clearAll();
    auto size = getpagesize() - 2;
    size -= 4;
    string key = "key";
    auto keySize = 3 + 1;
    size -= keySize;
    auto valueSize = 3;
    size -= valueSize;
    mmkv::MMBuffer value(size);
    mmkv->set(value, key);
    mmkv->trim();
}

void fastRemoveCornetSizeTest() {
    string aesKey = "aes";
    auto mmkv = MMKV::mmkvWithID("fastRemoveCornerSize", MMKV_MULTI_PROCESS, &aesKey);
    mmkv->clearAll();
    auto size = getpagesize() - 4;
    size -= 4;
    string key = "key";
    auto keySize = 3 + 1;
    size -= keySize;
    auto valueSize = 3;
    size -= valueSize;
    size -= (keySize + 1); // total size of fast remove
    size /= 16;
    mmkv::MMBuffer value(size);
    auto ptr = (char *) value.getPtr();
    for (size_t i = 0; i < value.length(); i++) {
        ptr[i] = 'A';
    }
    for (int i = 0; i < 16; i++) {
        mmkv->set(value, key); // when a full write back is occur, here's corruption happens
        mmkv->removeValueForKey(key);
    }
}

void testClearEmptyMMKV() {
    auto mmkv = MMKV::mmkvWithID("emptyMMKV");
    mmkv->set(true, "bool");
    mmkv->clearAll();
    mmkv->clearAll();
}

void testBackup() {
    string rootDir = "/tmp/mmkv_backup";
    string mmapID = "test/Encrypt";
    string aesKey = "cryptKey";
    auto ret = MMKV::backupOneToDirectory(mmapID, rootDir);
    printf("backup one return %d\n", ret);
    if (ret) {
        auto mmkv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, &aesKey, &rootDir);
        cout << "after backup allKeys: " << ::to_string(mmkv->allKeys()) << endl;
    }

    auto count = MMKV::backupAllToDirectory(rootDir);
    printf("backup all count: %zu\n", count);
    if (count > 0) {
        auto backupMMKV = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, &aesKey, &rootDir);
        cout << "check on backup [" << backupMMKV->mmapID() << "] allKeys: " << ::to_string(backupMMKV->allKeys(), ",\n") << endl;

        backupMMKV = MMKV::mmkvWithID("brutleTest", MMKV_SINGLE_PROCESS, nullptr, &rootDir);
        cout << "check on backup [" << backupMMKV->mmapID() << "] allKeys count: " << backupMMKV->count() << endl;

        backupMMKV = MMKV::mmkvWithID("process_test", MMKV_MULTI_PROCESS, nullptr, &rootDir);
        cout << "check on backup [" << backupMMKV->mmapID() << "] allKeys count: " << backupMMKV->count() << endl;

        backupMMKV = MMKV::mmkvWithID("thread_test1", MMKV_SINGLE_PROCESS, nullptr, &rootDir);
        cout << "check on backup [" << backupMMKV->mmapID() << "] allKeys count: " << backupMMKV->count() << endl;
    }
}

void testRestore() {
    string rootDir = "/tmp/mmkv_backup";
    string mmapID = "test/Encrypt";
    string aesKey = "cryptKey";
    auto mmkv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, &aesKey);
    mmkv->set(__LINE__, "test_restore_key");
    cout << "before restore [" << mmkv->mmapID() << "] allKeys: " << ::to_string(mmkv->allKeys(), ",\n") << endl;

    auto ret = MMKV::restoreOneFromDirectory(mmapID, rootDir);
    printf("restore one return %d\n", ret);
    if (ret) {
        cout << "after restore [" << mmkv->mmapID() << "] allKeys: " << ::to_string(mmkv->allKeys(), ",\n") << endl;
    }

    auto count = MMKV::restoreAllFromDirectory(rootDir);
    printf("restore all count: %zu\n", count);
    if (count > 0) {
        auto backupMMKV = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, &aesKey);
        cout << "check on restore [" << backupMMKV->mmapID() << "] allKeys: " << ::to_string(backupMMKV->allKeys(), ",\n") << endl;

        backupMMKV = MMKV::mmkvWithID("brutleTest");
        cout << "check on restore [" << backupMMKV->mmapID() << "] allKeys count: " << backupMMKV->count() << endl;

        backupMMKV = MMKV::mmkvWithID("process_test", MMKV_MULTI_PROCESS);
        cout << "check on restore [" << backupMMKV->mmapID() << "] allKeys count: " << backupMMKV->count() << endl;

        backupMMKV = MMKV::mmkvWithID("thread_test1", MMKV_SINGLE_PROCESS);
        cout << "check on restore [" << backupMMKV->mmapID() << "] allKeys count: " << backupMMKV->count() << endl;
    }
}

void testAutoExpiration() {
    string mmapID = "testAutoExpire";
    auto mmkv = MMKV::mmkvWithID(mmapID);
    mmkv->clearAll();
    mmkv->trim();
    mmkv->disableAutoKeyExpire();

    mmkv->set(true, "auto_expire_key_1");
    mmkv->enableAutoKeyExpire(1);
    mmkv->set("never_expire_key_1", "never_expire_key_1", 0);

    sleep(2);
    assert(mmkv->containsKey("auto_expire_key_1") == false);
    assert(mmkv->containsKey("never_expire_key_1") == true);

    mmkv->removeValueForKey("never_expire_key_1");
    mmkv->enableAutoKeyExpire(0);
    mmkv->set("never_expire_key_1", "never_expire_key_1");
    mmkv->set(true, "auto_expire_key_1", 1);
    sleep(2);
    assert(mmkv->containsKey("never_expire_key_1") == true);
    assert(mmkv->containsKey("auto_expire_key_1") == false);
}

void MyLogHandler(MMKVLogLevel level, const char *file, int line, const char *function, const string &message) {

    auto desc = [level] {
        switch (level) {
            case MMKVLogDebug:
                return "D";
            case MMKVLogInfo:
                return "I";
            case MMKVLogWarning:
                return "W";
            case MMKVLogError:
                return "E";
            default:
                return "N";
        }
    }();
    printf("redirecting-[%s] <%s:%d::%s> %s\n", desc, file, line, function, message.c_str());
}

int main() {
    locale::global(locale(""));
    wcout.imbue(locale(""));
    char c;
    srand((uint64_t) &c);

    string rootDir = "/tmp/mmkv";
    MMKV::initializeMMKV(rootDir, MMKVLogInfo, MyLogHandler);
    // MMKV::setLogLevel(MMKVLogNone);
    // MMKV::registerLogHandler(MyLogHandler);

    //auto mmkv = MMKV::defaultMMKV();
    string aesKey = "cryptKey";
    auto mmkv = MMKV::mmkvWithID("test/Encrypt", MMKV_SINGLE_PROCESS, &aesKey);
    functionalTest(mmkv, false);

    for (size_t index = 0; index < keyCount; index++) {
        arrIntKeys.push_back("int-" + to_string(index));
        arrStringKeys.push_back("string-" + to_string(index));
    }

    //fastRemoveCornetSizeTest();
    //cornetSizeTest();
    //testClearEmptyMMKV();
    brutleTest();
    threadTest();
    processTest();
    testInterProcessLock();
    testBackup();
    testRestore();
    testAutoExpiration();
}
