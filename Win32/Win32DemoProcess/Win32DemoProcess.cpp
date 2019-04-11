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

#include "pch.h"

#include <MMKV/MMKV.h>
#include <chrono>
#include <iostream>
#include <string>

using namespace std;

wstring getAppDataRoaming(const wstring &company, const wstring &appName) {
    wchar_t roaming[MAX_PATH] = {0};
    auto size = GetEnvironmentVariable(L"appdata", roaming, MAX_PATH);
    if (size >= MAX_PATH || size == 0) {
        cout << "fail to get %appdata%: " << GetLastError() << endl;
        return L"";
    } else {
        wstring result(roaming, size);
        result += L"\\" + company;
        result += L"\\" + appName;
        return result;
    }
}

constexpr auto keyCount = 1000;
static const string MMKV_ID = "process_test";
vector<string> arrIntKeys;
vector<string> arrStringKeys;

void brutleTest(DWORD processID) {
    using hclock = chrono::high_resolution_clock;
    auto start = hclock::now();

    auto mmkv = MMKV::mmkvWithID(MMKV_ID, MMKV_MULTI_PROCESS);
    for (size_t i = 0; i < keyCount; i++) {
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
    srand(GetTickCount());

    wstring rootDir = getAppDataRoaming(L"Tencent", L"н╒пе-MMKV");
    MMKV::initializeMMKV(rootDir);

    auto processID = GetCurrentProcessId();
    cout << processID << ": started\n";

    for (size_t index = 0; index < keyCount; index++) {
        arrIntKeys.push_back("int-" + to_string(index));
        arrStringKeys.push_back("string-" + to_string(index));
    }
    brutleTest(processID);

    cout << processID << ": ended\n";
}
