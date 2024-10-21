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
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <cstring>
#include <cassert>
#include <ctime>
#include <cmath>
#include <cinttypes> // For PRId64 & PRIu64

using namespace std;
using namespace mmkv;

string to_string(const std::string& str) {  return str; }

template <class T>
string to_string(const vector<T> &arr, const char* sp = ", ") {
    string str;
    for (const auto &element : arr) {
        str += to_string(element);
        str += sp;
    }
    if (!str.empty()) {
        str.erase(str.length() - strlen(sp));
    }
    return str;
}

void containerTest(MMKV *mmkv, bool decodeOnly);

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
        mmkv->set("Hello, MMKV-示例 for POSIX", "raw_string");
        string str = "Hello, MMKV-示例 for POSIX string";
        mmkv->set(str, "string");
        mmkv->set(string_view(str).substr(7, 21), "string_view");
    }
    string result;
    mmkv->getString("raw_string", result);
    cout << "raw_string = " << result << endl;
    mmkv->getString("string", result);
    cout << "string = " << result << endl;
    mmkv->getString("string_view", result);
    cout << "string_view = " << result << endl;

    containerTest(mmkv, decodeOnly);

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

void containerTest(MMKV *mmkv, bool decodeOnly) {
    {
        if (!decodeOnly) {
            vector<string> vec = {"Hello", "MMKV-示例", "for", "POSIX"};
            mmkv->set(vec, "string-set");
        }
        vector<string> vecResult;
        mmkv->getVector("string-set", vecResult);
        cout << "string-set = " << to_string(vecResult) << endl;
    }
#if __cplusplus>=202002L
    {
        if (!decodeOnly) {
            vector<bool> vec = {true, false, std::numeric_limits<bool>::min(), std::numeric_limits<bool>::max()};
            mmkv->set(vec, "bool-set");
        }
        vector<bool> vecResult;
        mmkv->getVector("bool-set", vecResult);
        cout << "bool-set = " << to_string(vecResult) << endl;
    }

    {
        if (!decodeOnly) {
            vector<int32_t> vec = {1024, 0, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()};
            mmkv->set(vec, "int32-set");
        }
        vector<int32_t> vecResult;
        mmkv->getVector("int32-set", vecResult);
        cout << "int32-set = " << to_string(vecResult) << endl;
    }

    {
        if (!decodeOnly) {
            constexpr uint32_t arr[] = {2048, 0, std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max()};
            std::span vec = arr;
            mmkv->set(vec, "uint32-set");
        }
        vector<uint32_t> vecResult;
        mmkv->getVector("uint32-set", vecResult);
        cout << "uint32-set = " << to_string(vecResult) << endl;
    }

    {
        if (!decodeOnly) {
            constexpr int64_t vec[] = {1024, 0, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max()};
            mmkv->set(std::span(vec), "int64-set");
        }
        vector<int64_t> vecResult;
        mmkv->getVector("int64-set", vecResult);
        cout << "int64-set = " << to_string(vecResult) << endl;
    }

    {
        if (!decodeOnly) {
            vector<uint64_t> vec = {1024, 0, std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max()};
            mmkv->set(vec, "uint64-set");
        }
        vector<uint64_t> vecResult;
        mmkv->getVector("uint64-set", vecResult);
        cout << "uint64-set = " << to_string(vecResult) << endl;
    }

    {
        if (!decodeOnly) {
            vector<float> vec = {1024.0, 0.0, std::numeric_limits<float>::min(), std::numeric_limits<float>::max()};
            mmkv->set(vec, "float-set");
        }
        vector<float> vecResult;
        mmkv->getVector("float-set", vecResult);
        cout << "float-set = " << to_string(vecResult) << endl;
    }

    {
        if (!decodeOnly) {
            vector<double> vec = {1024.0, 0.0, std::numeric_limits<double>::min(), std::numeric_limits<double>::max()};
            mmkv->set(vec, "double-set");
        }
        vector<double> vecResult;
        mmkv->getVector("double-set", vecResult);
        cout << "double-set = " << to_string(vecResult) << endl;

        // un-comment to test the functionality of set<!MMKV_SUPPORTED_VALUE_TYPE<T>>(const T& value, key)
        // mmkv->set(&vecResult, "unsupported-type");
    }
#endif // __cplusplus>=202002L
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

void testClearAllKeepSpace() {
    {
        auto mmkv = MMKV::mmkvWithID("testClearAllKeepSpace");
        string big(10, '0');
        for (int i = 0; i < 100; i++) {
            mmkv->set(big, to_string(i));
        }
        size_t size0 = mmkv->totalSize();

        mmkv->clearAll(true);
        size_t size1 = mmkv->totalSize();
        assert(size0 == size1);
        assert(mmkv->count() == 0);
        string key1 = "key1";
        string key2 = "key2";
        mmkv->set(123, key1);
        assert(mmkv->getInt32(key1, 0) == 123);

        mmkv->set(456, key2);
        assert(mmkv->getInt32(key2, 0) == 456);

        // test normal clearAll
        assert(mmkv->count() == 2);
        mmkv->clearAll();
        assert(mmkv->count() == 0);

        // test reopen
        mmkv->set(1234, key1);
        mmkv->set(12345, key2);
        mmkv->clearAll(true);
        mmkv->close();
        mmkv = MMKV::mmkvWithID("testClearAllKeepSpace");
        assert(mmkv->count() == 0);

        mmkv->set(456, key2);
        assert(mmkv->getInt32(key2, 0) == 456);
    }

    {
        string aesKey = "cryptKey111";
        auto mmkv = MMKV::mmkvWithID("testClearAllKeepSpaceWithCrypt", MMKV_SINGLE_PROCESS, &aesKey);
        string big(10, '0');
        for (int i = 0; i < 100; i++) {
            mmkv->set(big, to_string(i));
        }
        size_t size0 = mmkv->totalSize();

        mmkv->clearAll(true);
        size_t size1 = mmkv->totalSize();
        assert(size0 == size1);
        assert(mmkv->count() == 0);
        string key1 = "key1";
        string key2 = "key2";
        mmkv->set(123, key1);
        assert(mmkv->getInt32(key1, 0) == 123);

        mmkv->set(456, key2);
        assert(mmkv->getInt32(key2, 0) == 456);

        // test normal clearAll
        assert(mmkv->count() == 2);
        mmkv->clearAll();
        assert(mmkv->count() == 0);

        // test reopen
        mmkv->set(1234, key1);
        mmkv->set(12345, key2);
        mmkv->clearAll(true);
        mmkv->close();
        mmkv = MMKV::mmkvWithID("testClearAllKeepSpaceWithCrypt", MMKV_SINGLE_PROCESS, &aesKey);
        assert(mmkv->count() == 0);

        mmkv->set(456, key2);
        assert(mmkv->getInt32(key2, 0) == 456);
    }
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
    mmkv->set("never_expire_value_1", "never_expire_key_1", MMKV::ExpireNever);
    mmkv->set("", "never_expire_key_2", MMKV::ExpireNever);
    mmkv->set(MMBuffer(), "never_expire_key_3", MMKV::ExpireNever);

    sleep(2);
    assert(mmkv->containsKey("auto_expire_key_1") == false);
    assert(mmkv->containsKey("never_expire_key_1") == true);
    assert(mmkv->containsKey("never_expire_key_2") == true);
    assert(mmkv->containsKey("never_expire_key_3") == true);
    {
        string result;
        auto ret = mmkv->getString("never_expire_key_2", result);
        assert(ret && result.empty());

        MMBuffer buffer;
        ret = mmkv->getBytes("never_expire_key_3", buffer);
        assert(ret && buffer.length() == 0);
    }

    mmkv->removeValueForKey("never_expire_key_1");
    mmkv->enableAutoKeyExpire(MMKV::ExpireNever);
    mmkv->set("never_expire_value_1", "never_expire_key_1");
    mmkv->set(true, "auto_expire_key_1", 1);
    sleep(2);
    assert(mmkv->containsKey("never_expire_key_1") == true);
    assert(mmkv->containsKey("auto_expire_key_1") == false);

    auto count = mmkv->count(true);
    cout << "count all non expire keys: " << count << endl;
    auto allKeys = mmkv->allKeys(true);
    cout << "all non expire keys: " << ::to_string(allKeys) << endl;
}

void testExpectedCapacity() {
    auto mmkv0 = MMKV::mmkvWithID("testExpectedCapacity0", MMKV_SINGLE_PROCESS, nullptr, nullptr, 0);
    assert(mmkv0->totalSize() == DEFAULT_MMAP_SIZE);

    auto mmkv1 = MMKV::mmkvWithID("testExpectedCapacity1", MMKV_SINGLE_PROCESS, nullptr, nullptr, DEFAULT_MMAP_SIZE + 1);
    assert(mmkv1->totalSize() == DEFAULT_MMAP_SIZE << 1);

    auto mmkv2 = MMKV::mmkvWithID("testExpectedCapacity2", MMKV_SINGLE_PROCESS, nullptr, nullptr, DEFAULT_MMAP_SIZE >> 1);
    assert(mmkv2->totalSize() == DEFAULT_MMAP_SIZE);

    auto mmkv3 = MMKV::mmkvWithID("testExpectedCapacity3");
    mmkv3->clearAll();
    assert(mmkv3->totalSize() == DEFAULT_MMAP_SIZE);
    mmkv3->close();
    // expand it
    mmkv3 = MMKV::mmkvWithID("testExpectedCapacity3", MMKV_SINGLE_PROCESS, nullptr, nullptr, 100 * DEFAULT_MMAP_SIZE + 100);
    assert(mmkv3->totalSize() == DEFAULT_MMAP_SIZE * 101);

    // if new size is smaller than file size, keep file its origin size
    mmkv3->close();
    mmkv3 = MMKV::mmkvWithID("testExpectedCapacity3", MMKV_SINGLE_PROCESS, nullptr, nullptr, 0);
    assert(mmkv3->totalSize() == DEFAULT_MMAP_SIZE * 101);

    int len = 10000;
    std::string value(len, '0');
    value = "🏊🏻®4️⃣🐅_" + value;
    cout << "value length = " << value.size() << endl;
    std::string key = "key";
    // if you know exactly the sizes of key and value, set expectedCapacity for performance improvement
    size_t expectedSize = key.size() + value.size();
    auto mmkv4 = MMKV::mmkvWithID("testExpectedCapacity4", MMKV_SINGLE_PROCESS, nullptr, nullptr, expectedSize);
    // 0 times expand
    mmkv4->set(value, key);

    int count = 10;
    expectedSize = (key.size() + value.size()) * count;
    auto mmkv5 = MMKV::mmkvWithID("testExpectedCapacity5", MMKV_SINGLE_PROCESS, nullptr, nullptr, expectedSize);
    for (int i = 0; i < count; i++) {
        key[0] = static_cast<char>('a' + i);
        // 0 times expand
        mmkv5->set(value, key);
    }
}

void testOnlyOneKey() {
    string key = "name";
    string s;
    {
        s = "";
        auto mmkv = MMKV::mmkvWithID("testOneKey");

        mmkv->getString(key, s);
        printf("testOneKey: value = %s\n", s.c_str());

        mmkv->set("world", key);
        mmkv->getString(key, s);
        printf("testOneKey: value = %s\n", s.c_str());

        for (int i = 0; i < 10; i++) {
            string value = "world_";
            value += to_string(i);
            mmkv->set(value, key);
            mmkv->getString(key, s);
            printf("testOneKey: value = %s\n", s.c_str());
        }

        // test file expanding
        string bigValue(100000, '0');
        mmkv->set(bigValue, key);
        mmkv->getString(key, s);
        assert(s == bigValue);

        mmkv->set("OK", key);
        mmkv->getString(key, s);
        printf("testOneKey: value = %s\n", s.c_str());

        // close it and reopen it
        mmkv->close();
        mmkv = MMKV::mmkvWithID("testOneKey");
        mmkv->getString(key, s);
        printf("testOneKey: after reopen value = %s\n", s.c_str());
        assert(s == "OK");
    }

    {
        s = "";
        string cryptKey = "fastest";
        auto mmkv = MMKV::mmkvWithID("testOneKeyCrypt", MMKV_SINGLE_PROCESS, &cryptKey);

        mmkv->getString(key, s);
        printf("testOneKeyCrypt: value = %s\n", s.c_str());

        mmkv->set("cryptworld", key);
        mmkv->getString(key, s);
        printf("testOneKeyCrypt: value = %s\n", s.c_str());

        for (int i = 0; i < 10; i++) {
            string value = "cryptworld_";
            value += to_string(i);
            mmkv->set(value, key);
            mmkv->getString(key, s);
            printf("testOneKeyCrypt: value = %s\n", s.c_str());
        }

        // close it and reopen it
        mmkv->close();
        mmkv = MMKV::mmkvWithID("testOneKeyCrypt", MMKV_SINGLE_PROCESS, &cryptKey);
        mmkv->getString(key, s);
        printf("testOneKeyCrypt: after reopen value = %s\n", s.c_str());
        assert(s == "cryptworld_9");

        mmkv->set("cryptworld_good", key);
        mmkv->getString(key, s);
        printf("testOneKeyCrypt: value = %s\n", s.c_str());
        assert(s == "cryptworld_good");
    }

    {
        string cryptKey = "expiretest";
        auto mmkv = MMKV::mmkvWithID("testOneKeyCryptExpire", MMKV_SINGLE_PROCESS, &cryptKey);
        mmkv->enableAutoKeyExpire(24 * 60 * 60);

        s = "";
        mmkv->getString(key, s);
        printf("testOneKeyCryptExpire: value = %s\n", s.c_str());

        mmkv->set("expire", key, 1000);
        mmkv->getString(key, s);
        printf("testOneKeyCryptExpire: value = %s\n", s.c_str());

        for (int i = 0; i < 10; i++) {
            string value = "expire_";
            value += to_string(i);
            mmkv->set(value, key);
            mmkv->getString(key, s);
            printf("testOneKeyCryptExpire: value = %s\n", s.c_str());
        }

        // close it and reopen it
        mmkv->close();
        mmkv = MMKV::mmkvWithID("testOneKeyCryptExpire", MMKV_SINGLE_PROCESS, &cryptKey);
        mmkv->getString(key, s);
        printf("testOneKeyCryptExpire: after reopen value = %s\n", s.c_str());
        assert(s == "expire_9");
    }
}

void testOverride() {
    string key1 = "key1";
    string key2 = "key2";
    string key3 = "key3";
    string s;
    {
        s = "";
        auto mmkv = MMKV::mmkvWithID("testOverride");

        mmkv->set("world1", key1);
        mmkv->getString(key1, s);
        printf("testOverride: key1 = %s\n", s.c_str());

        mmkv->set("world2", key2);
        mmkv->getString(key2, s);
        printf("testOverride: key2 = %s\n", s.c_str());

        mmkv->removeValueForKey(key1);
        mmkv->removeValueForKey(key2);

        printf("testOverride: actualSize = %lu\n", mmkv->actualSize());

        mmkv->set("world3", key3);
        mmkv->getString(key3, s);
        printf("testOverride: key3 = %s\n", s.c_str());
        printf("testOverride: actualSize = %lu\n", mmkv->actualSize());

        mmkv->removeValueForKey(key3);

        // test file expanding
        string bigValue(100000, '0');
        mmkv->set(bigValue, key1);
        mmkv->getString(key1, s);
        assert(s == bigValue);

        mmkv->removeValueForKey(key1);

        mmkv->set("OK", key2);
        mmkv->getString(key2, s);
        printf("testOverride: value = %s\n", s.c_str());

        // close it and reopen it
        mmkv->close();
        mmkv = MMKV::mmkvWithID("testOverride");
        mmkv->getString(key2, s);
        printf("testOverride: after reopen key2 = %s\n", s.c_str());
        assert(s == "OK");

        mmkv->removeValueForKey(key2);
        mmkv->trim();
    }

    {
        s = "";
        string cryptKey = "fastest2";
        auto mmkv = MMKV::mmkvWithID("testOverrideCrypt", MMKV_SINGLE_PROCESS, &cryptKey);
        mmkv->enableAutoKeyExpire(24 * 60 * 60);

        mmkv->set("world1", key1);
        mmkv->getString(key1, s);
        printf("testOverrideCrypt: key1 = %s\n", s.c_str());

        mmkv->set("world2", key2);
        mmkv->getString(key2, s);
        printf("testOverrideCrypt: key2 = %s\n", s.c_str());

        mmkv->removeValueForKey(key1);
        mmkv->removeValueForKey(key2);

        printf("testOverrideCrypt: actualSize = %lu\n", mmkv->actualSize());

        mmkv->set("world3", key3);
        mmkv->getString(key3, s);
        printf("testOverrideCrypt: key3 = %s\n", s.c_str());
        printf("testOverrideCrypt: actualSize = %lu\n", mmkv->actualSize());

        mmkv->removeValueForKey(key3);

        // test file expanding
        string bigValue(100000, '0');
        mmkv->set(bigValue, key1);
        mmkv->getString(key1, s);
        assert(s == bigValue);

        mmkv->removeValueForKey(key1);

        mmkv->set("OK", key2);
        mmkv->getString(key2, s);
        printf("testOverrideCrypt: value = %s\n", s.c_str());

        // close it and reopen it
        mmkv->close();
        mmkv = MMKV::mmkvWithID("testOverrideCrypt", MMKV_SINGLE_PROCESS, &cryptKey);
        mmkv->getString(key2, s);
        printf("testOverrideCrypt: after reopen key2 = %s\n", s.c_str());
        assert(s == "OK");

        mmkv->removeValueForKey(key2);
        mmkv->trim();
    }

}

uint64_t getTimeInMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void testGetStringSpeed() {
    string bigValue(100000, '0');
    string key = "key1";
    auto mmkv = MMKV::mmkvWithID("testGetStringSpeed");
    mmkv->set(bigValue, key);

    string result;
    uint64_t start1, end1, start2, end2;

    start2 = getTimeInMs();
    for (int i = 0; i < 2000000; i++) {
        mmkv->getString("key1", result, true);
    }
    end2 = getTimeInMs();

    start1 = getTimeInMs();
    for (int i = 0; i < 2000000; i++) {
        mmkv->getString("key1", result, false);
    }
    end1 = getTimeInMs();

    printf("old_method = %" PRId64 ", new_method = %" PRId64 "\n", end1 - start1, end2 - start2);

    start1 = getTimeInMs();
    for (int i = 0; i < 2000000; i++) {
        mmkv->getString("key1", result, false);
    }
    end1 = getTimeInMs();

    start2 = getTimeInMs();
    for (int i = 0; i < 2000000; i++) {
        mmkv->getString("key1", result, true);
    }
    end2 = getTimeInMs();
    printf("old_method = %" PRId64 ", new_method = %" PRId64 "\n", end1 - start1, end2 - start2);
}

void printVector(vector<string> &v) {
    printf("testCompareBeforeSet: string<vector>: ");
    if (v.empty()) {
        return;
    }
    for (int i = 0; i < v.size() - 1; i++) {
        printf("%s, ", v[i].c_str());
    }
    if (!v.empty()) {
        printf("%s\n", v[v.size() - 1].c_str());
    } else {
        printf("\n");
    }
}

void testCompareBeforeSet() {
    auto mmkv = MMKV::mmkvWithID("testCompareBeforeSet", MMKV_SINGLE_PROCESS);
    mmkv->enableCompareBeforeSet();
    mmkv->set("extraValue", "extraKey");

    size_t actualSize1 = -1;
    size_t actualSize2 = -1;

    string key;
    {
        key = "bool";
        mmkv->set(true, key);
        printf("testCompareBeforeSet: bool value = %d\n", mmkv->getBool(key));
        actualSize1 = mmkv->actualSize();
        printf("testCompareBeforeSet: actualSize = %lu\n", actualSize1);
        printf("testCompareBeforeSet: bool value = %d\n", mmkv->getBool(key));
        mmkv->set(true, key);
        actualSize2 = mmkv->actualSize();
        printf("testCompareBeforeSet: actualSize2 = %lu\n", actualSize2);
        assert(actualSize1 == actualSize2);
        mmkv->set(false, key);
        printf("testCompareBeforeSet: bool value = %d\n", mmkv->getBool(key));
        assert(mmkv->getBool(key) == false);
    }
    {
        key = "int32_t";
        int32_t v = 12345;
        mmkv->set(v, key);
        printf("testCompareBeforeSet: int32 value = %d\n", mmkv->getInt32(key));
        actualSize1 = mmkv->actualSize();
        printf("testCompareBeforeSet: actualSize = %lu\n", actualSize1);
        printf("testCompareBeforeSet: int32 value = %d\n", mmkv->getInt32(key));
        mmkv->set(v, key);
        actualSize2 = mmkv->actualSize();
        assert(actualSize1 == actualSize2);
        mmkv->set(v << 1, key);
        printf("testCompareBeforeSet: int32 value = %d\n", mmkv->getInt32(key));
        assert(mmkv->getInt32(key)  == v << 1);
    }

    {
        key = "uint32_t";
        uint32_t v32u = 6379;
        mmkv->set(v32u, key);
        printf("testCompareBeforeSet: uint32_t value = %d\n", mmkv->getUInt32(key));
        actualSize1 = mmkv->actualSize();
        printf("testCompareBeforeSet: actualSize = %lu\n", actualSize1);
        printf("testCompareBeforeSet: uint32_t value = %d\n", mmkv->getUInt32(key));
        mmkv->set(v32u, key);
        actualSize2 = mmkv->actualSize();
        assert(actualSize1 == actualSize2);
        mmkv->set(v32u >> 1, key);
        printf("testCompareBeforeSet: uint32_t value = %d\n", mmkv->getUInt32(key));
        assert(mmkv->getUInt32(key) == v32u >> 1);
    }
    {
        key = "int64_t";
        int64_t v64 = 8080;
        mmkv->set(v64, key);
        printf("testCompareBeforeSet: int64_t value = %" PRId64 "\n", mmkv->getInt64(key));
        actualSize1 = mmkv->actualSize();
        printf("testCompareBeforeSet: actualSize = %lu\n", actualSize1);
        printf("testCompareBeforeSet: int64_t value = %" PRId64 "\n", mmkv->getInt64(key));
        mmkv->set(v64, key);
        actualSize2 = mmkv->actualSize();
        assert(actualSize1 == actualSize2);
        mmkv->set(v64 >> 1, key);
        printf("testCompareBeforeSet: int64_t value = %" PRId64 "\n", mmkv->getInt64(key));
        assert(mmkv->getInt64(key) == v64 >> 1);
    }

    {
        key = "uint64_t";
        uint64_t v64u = 8848;
        mmkv->set(v64u, key);
        printf("testCompareBeforeSet: uint64_t value = %" PRIu64 "\n", mmkv->getUInt64(key));
        actualSize1 = mmkv->actualSize();
        printf("testCompareBeforeSet: actualSize = %lu\n", actualSize1);
        printf("testCompareBeforeSet: uint64_t value = %" PRIu64 "\n", mmkv->getUInt64(key));
        mmkv->set(v64u, key);
        actualSize2 = mmkv->actualSize();
        assert(actualSize1 == actualSize2);
        mmkv->set(v64u >> 1, key);
        printf("testCompareBeforeSet: uint64_t value = %" PRIu64 "\n", mmkv->getUInt64(key));
        assert(mmkv->getUInt64(key) == v64u >> 1);
    }

    {
        key = "float";
        float flt = -987.012f;
        mmkv->set(flt, key);
        printf("testCompareBeforeSet: float value = %lf\n", mmkv->getFloat(key));
        actualSize1 = mmkv->actualSize();
        printf("testCompareBeforeSet: actualSize = %lu\n", actualSize1);
        printf("testCompareBeforeSet: float value = %lf\n", mmkv->getFloat(key));
        mmkv->set(flt, key);
        actualSize2 = mmkv->actualSize();
        assert(actualSize1 == actualSize2);
        mmkv->set(flt * 12.56f, key);
        printf("testCompareBeforeSet: float value = %lf\n", mmkv->getFloat(key));
        assert(fabs(mmkv->getFloat(key) - flt * 12.56f) <= 1e-6);
    }

    {
        key = "double";
        double db = 8888987.012f;
        mmkv->set(db, key);
        printf("testCompareBeforeSet: double value = %lf\n", mmkv->getDouble(key));
        actualSize1 = mmkv->actualSize();
        printf("testCompareBeforeSet: actualSize = %lu\n", actualSize1);
        printf("testCompareBeforeSet: double value = %lf\n", mmkv->getDouble(key));
        mmkv->set(db, key);
        actualSize2 = mmkv->actualSize();
        assert(actualSize1 == actualSize2);
        mmkv->set(db * -12.56f, key);
        printf("testCompareBeforeSet: double value = %lf\n", mmkv->getDouble(key));
        assert(fabs(mmkv->getDouble(key) - db * -12.56f) <= 1e-6);
    }

    bool ret = false;
    string resultString;
    const char* raws = "🏊🏻®4️⃣🐅_";
    const char* raws2 = "12🏊🏻e®4️⃣🐅_34)(*()";
    {
        key = "char*";
        mmkv->set(raws, key);
        ret = mmkv->getString(key, resultString);
        printf("testCompareBeforeSet: char* = %s\n", resultString.c_str());
        actualSize1 = mmkv->actualSize();
        printf("testCompareBeforeSet: actualSize = %lu\n", actualSize1);
        ret = mmkv->getString(key, resultString);
        printf("testCompareBeforeSet: char* = %s\n", resultString.c_str());
        mmkv->set(raws, key);
        actualSize2 = mmkv->actualSize();
        assert(actualSize1 == actualSize2);
        mmkv->set(raws2, key);
        ret = mmkv->getString(key, resultString);
        printf("testCompareBeforeSet: char* = %s\n", resultString.c_str());
        assert(resultString == raws2);
    }

    string s1 = "🏊🏻®hhh4️⃣🐅_yyy";
    string s2 = "0aA🏊🏻®hhh4️⃣🐅_zzz";

    {
        key = "string";
        mmkv->set(s1, key);
        ret = mmkv->getString(key, resultString);
        printf("testCompareBeforeSet: string = %s\n", resultString.c_str());
        actualSize1 = mmkv->actualSize();
        printf("testCompareBeforeSet: actualSize = %lu\n", actualSize1);
        ret = mmkv->getString(key, resultString);
        printf("testCompareBeforeSet: string = %s\n", resultString.c_str());
        mmkv->set(s1, key);
        actualSize2 = mmkv->actualSize();
        assert(actualSize1 == actualSize2);
        mmkv->set(s2, key);
        ret = mmkv->getString(key, resultString);
        printf("testCompareBeforeSet: string = %s\n", resultString.c_str());
        assert(resultString == s2);
    }

    {
        s1 = "buffer_🏊🏻®hhh4️⃣🐅_yyy";
        s2 = "buffer_0aA🏊🏻®hhh4️⃣🐅_zzz";
        MMBuffer buffer1((void *) s1.c_str(), s1.size());
        MMBuffer buffer2((void *) s2.c_str(), s2.size());
        key = "mmbuffer";
        mmkv->set(buffer1, key);
        ret = mmkv->getString(key, resultString);
        printf("testCompareBeforeSet: MMBuffer = %s\n", resultString.c_str());
        actualSize1 = mmkv->actualSize();
        printf("testCompareBeforeSet: MMBuffer = %lu\n", actualSize1);
        ret = mmkv->getString(key, resultString);
        printf("testCompareBeforeSet: MMBuffer = %s\n", resultString.c_str());
        mmkv->set(buffer1, key);
        actualSize2 = mmkv->actualSize();
        assert(actualSize1 == actualSize2);
        mmkv->set(buffer2, key);
        ret = mmkv->getString(key, resultString);
        printf("testCompareBeforeSet: MMBuffer = %s\n", resultString.c_str());
        assert(resultString == s2);
    }

    {
        key = "string<vector>";
        vector<string> v1 = {"1", s1, s2};
        vector<string> v2 = {"2", s1, s2};
        vector<string> vectorResult;
        mmkv->set(v1, key);
        ret = mmkv->getVector(key, vectorResult);
        printVector(vectorResult);
        actualSize1 = mmkv->actualSize();
        printf("testCompareBeforeSet: string<vector> size = %lu\n", actualSize1);
        ret = mmkv->getVector(key, vectorResult);
        printVector(vectorResult);
        mmkv->set(v1, key);
        actualSize2 = mmkv->actualSize();
        assert(actualSize1 == actualSize2);
        mmkv->set(v2, key);
        ret = mmkv->getVector(key, vectorResult);
        printVector(vectorResult);
        assert(vectorResult == v2);
    }

//    {
//        string key = "keyyy";
//        auto mmkv1 = MMKV::mmkvWithID("testCompareBeforeSet1", MMKV_SINGLE_PROCESS, &key);
//        mmkv1->enableCompareBeforeSet();
//    }

//    {
//        auto mmkv2 = MMKV::mmkvWithID("testCompareBeforeSet2", MMKV_SINGLE_PROCESS);
//        mmkv2->enableAutoKeyExpire();
//        mmkv2->enableCompareBeforeSet();
//    }
//
//    {
//        auto mmkv3 = MMKV::mmkvWithID("testCompareBeforeSet3", MMKV_SINGLE_PROCESS);
//        mmkv3->enableCompareBeforeSet();
//        mmkv3->enableAutoKeyExpire();
//    }

    {
        actualSize1 = -1;
        auto mmkv1 = MMKV::mmkvWithID("differentType2");
        mmkv1->clearAll();
        mmkv1->enableCompareBeforeSet();
        mmkv1->set("xxx", "yyy");
        actualSize1 = mmkv1->actualSize();

        mmkv1->set("value1", "key1");
        actualSize2 = mmkv1->actualSize();
        assert(actualSize2 > actualSize1);
        actualSize1 = actualSize2;
        printf("%d\n", mmkv1->getBool("key1", false));
        printf("actualSize = %lu\n", mmkv1->actualSize());
        assert( mmkv1->getBool("key1", false));
        mmkv1->set(true, "key1");
        actualSize2 = mmkv1->actualSize();
        assert(actualSize2 > actualSize1);
        actualSize1 = actualSize2;

        mmkv1->set(false, "key1");
        actualSize2 = mmkv1->actualSize();
        assert(actualSize2 > actualSize1);
        actualSize1 = actualSize2;

        printf("%d\n", mmkv1->getBool("key1", false)); // print 1
        printf("actualSize = %lu\n", mmkv1->actualSize());
        mmkv1->set("value1", "key1");
        actualSize2 = mmkv1->actualSize();
        assert(actualSize2 > actualSize1);
        actualSize1 = actualSize2;

        string v;
        mmkv1->getString("key1", v);
        printf("%s\n", v.c_str()); // print value1

        vector<string> v1 = {"1", s1, s2};
        vector<string> v2 = {"2", s1, s2};
        mmkv1->set(v1, "key1");
        actualSize2 = mmkv1->actualSize();
        assert(actualSize2 > actualSize1);
        actualSize1 = actualSize2;
        vector<string> vectorResult;
        ret = mmkv1->getVector("key1", vectorResult);
        printVector(vectorResult);

        // test exception
        mmkv1->set(12345, "key2");
        actualSize1 = mmkv1->actualSize();
        // "<MMKV_IO.cpp::setDataForKey> compareBeforeSet exception: InvalidProtocolBuffer truncatedMessage" from log
        string vv = "abcdefg";
        mmkv1->set(vv, "key2");
        actualSize2 = mmkv1->actualSize();
        string vv2(vv.size(), 0);
        mmkv1->getString("key2", vv2, true);
        assert(vv == vv2);
        assert(actualSize2 > actualSize1);
    }

    {
        key = "key";
        auto mmkv1 = MMKV::mmkvWithID("compareEmpty");
        mmkv1->enableCompareBeforeSet();
        mmkv1->set("", key);
        actualSize1 = mmkv1->actualSize();
        mmkv1->set("", key);
        actualSize2 = mmkv1->actualSize();
        assert(actualSize1 == actualSize2);

        string vv = "abcdefG";
        actualSize1 = mmkv1->actualSize();
        mmkv1->set(vv, key);
        actualSize2 = mmkv1->actualSize();
        assert(actualSize2 > actualSize1);
        string result;
        mmkv1->getString(key, result, true);
        assert(result == vv);
    }
}

void testFtruncateFail() {
    auto mmkv = MMKV::mmkvWithID("testFtruncateFail");
    signal(SIGXFSZ, SIG_IGN);
    struct rlimit rlim_new,rlim;
    string bigValue(1000, '0');
    if (getrlimit(RLIMIT_FSIZE, &rlim) == 0) {
        rlim_new.rlim_cur = rlim_new.rlim_max = 5000 * 1024;
        int ret = setrlimit(RLIMIT_FSIZE, &rlim_new);
        printf("setrlimit ret = %d\n", ret);

        for (int i = 0; i < 1000000; i++) {
            string key = "qwerttt" + to_string(i);
//            fail to truncate [/tmp/mmkv/testFtruncateFail] to size 8388608, File too large
            bool ret = mmkv->set(bigValue, key);
            if (!ret) {
                break;
            }
        }
    }
}

void testRemoveStorage() {
    string mmapID = "test_remove";
    {
        auto mmkv = MMKV::mmkvWithID(mmapID, MMKV_MULTI_PROCESS);
        mmkv->set(true, "bool");
    }
    MMKV::removeStorage(mmapID);
    {
        auto mmkv = MMKV::mmkvWithID(mmapID, MMKV_MULTI_PROCESS);
        if (mmkv->count() != 0) {
            abort();
        }
    }

    mmapID = "test_remove/sg";
    string rootDir = "/tmp/mmkv_1";
    auto mmkv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, nullptr, &rootDir);
    mmkv->set(true, "bool");
    MMKV::removeStorage(mmapID, &rootDir);
    mmkv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, nullptr, &rootDir);
    if (mmkv->count() != 0) {
        abort();
    }
}

void setReadOnly(const MMKVPath_t& path, bool readOnly) {
    int mode = 0;
    // alter the read-only attribute
    if (readOnly) {
        mode = 0444;
    } else {
        mode = 0666;
    }
    // Set the file attributes to the new value
    if (chmod(path.c_str(), mode) != 0) {
        // If the function fails, print an error message
        auto err = errno;
        printf("Failed to set file attributes. Error code: %d, %s\n", err, strerror(err));
    }
}

void testReadOnly() {
    string mmapID = "testReadOnly";
    string aesKey = "ReadOnly+Key";
    {
        auto mmkv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, &aesKey);
        functionalTest(mmkv, false);
        mmkv->close();
    }

    auto path = MMKV::getRootDir() + MMKV_PATH_SLASH + mmapID;
    setReadOnly(path, true);
    auto crcPath = path + ".crc";
    setReadOnly(crcPath, true);
    {
        auto mmkv = MMKV::mmkvWithID(mmapID, (MMKV_SINGLE_PROCESS | MMKV_READ_ONLY), &aesKey);
        functionalTest(mmkv, true);

        // also check if it tolerate update operations without crash
        functionalTest(mmkv, false);

        mmkv->close();
    }
    setReadOnly(path, false);
    setReadOnly(crcPath, false);
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
    testExpectedCapacity();
    testOnlyOneKey();
    testOverride();
    testClearAllKeepSpace();
//    testGetStringSpeed();
    testCompareBeforeSet();
    testBackup();
    testRestore();
    testAutoExpiration();
//    testFtruncateFail();
    testRemoveStorage();
    testReadOnly();
}
