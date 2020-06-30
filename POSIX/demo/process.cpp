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
#include <string>
#include <unistd.h>

using namespace std;
using namespace mmkv;

constexpr auto keyCount = 1000;
static const string MMKV_ID = "process_test";
vector<string> arrIntKeys;
vector<string> arrStringKeys;

void brutleTest(int processID) {
    using hclock = chrono::high_resolution_clock;
    auto start = hclock::now();

    auto mmkv = MMKV::mmkvWithID(MMKV_ID, MMKV_MULTI_PROCESS);
    for (int32_t i = 0; i < keyCount; i++) {
        mmkv->set(i, arrIntKeys[i]);
        mmkv->set("str-" + to_string(i), arrStringKeys[i]);
        mmkv->getInt32(arrIntKeys[i]);
        string result;
        mmkv->getString(arrStringKeys[i], result);
    }

    auto finish = hclock::now();
    auto used = chrono::duration_cast<chrono::milliseconds>(finish - start).count();
    mmkv->lock();
    cout << endl << processID << ": " << used << " ms\n";
    mmkv->unlock();
}

int main() {
    locale::global(locale(""));
    wcout.imbue(locale(""));
    char c;
    srand((uint64_t) &c);

    string rootDir = "/tmp/mmkv";
    MMKV::initializeMMKV(rootDir);

    auto processID = getpid();
    cout << processID << ": started\n";

    for (size_t index = 0; index < keyCount; index++) {
        arrIntKeys.push_back("int-" + to_string(index));
        arrStringKeys.push_back("string-" + to_string(index));
    }
    brutleTest(processID);

    cout << processID << ": ended\n";
    return 0;
}
