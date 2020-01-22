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

#include "MMKV.h"
#include <chrono>
#include <cstdio>
#include <iostream>
#include <limits>
#include <pthread.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;
using namespace mmkv;

string to_string(vector<string> &&arr) {
    string str;
    for (const auto &element : arr) {
        str += element + ", ";
    }
    if (!str.empty()) {
        str.erase(str.length() - 2);
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
    MMKV::initializeMMKV(rootDir);
    //MMKV::setLogLevel(MMKVLogNone);
    MMKV::registerLogHandler(MyLogHandler);

    //auto mmkv = MMKV::defaultMMKV();
    string aesKey = "cryptKey";
    auto mmkv = MMKV::mmkvWithID("testEncrypt", MMKV_SINGLE_PROCESS, &aesKey);
    functionalTest(mmkv, false);

    for (size_t index = 0; index < keyCount; index++) {
        arrIntKeys.push_back("int-" + to_string(index));
        arrStringKeys.push_back("string-" + to_string(index));
    }

    //fastRemoveCornetSizeTest();
    //cornetSizeTest();
    brutleTest();
    threadTest();
    processTest();
}
