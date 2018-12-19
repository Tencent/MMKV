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

#define NOMINMAX // undefine max/min

#include "MMKV.h"
#include "pch.h"
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

void functionalTest(MMKV *mmkv, bool decodeOnly) {
    if (!decodeOnly) {
        mmkv->setBool(true, "bool");
    }
    cout << "bool = " << mmkv->getBoolForKey("bool") << endl;

    if (!decodeOnly) {
        mmkv->setInt32(1024, "int32");
    }
    cout << "int32 = " << mmkv->getInt32ForKey("int32") << endl;

    if (!decodeOnly) {
        mmkv->setUInt32(numeric_limits<uint32_t>::max(), "uint32");
    }
    cout << "uint32 = " << mmkv->getUInt32ForKey("uint32") << endl;

    if (!decodeOnly) {
        mmkv->setInt64(numeric_limits<int64_t>::min(), "int64");
    }
    cout << "int64 = " << mmkv->getInt64ForKey("int64") << endl;

    if (!decodeOnly) {
        mmkv->setUInt64(numeric_limits<uint64_t>::max(), "uint64");
    }
    cout << "uint64 = " << mmkv->getUInt64ForKey("uint64") << endl;

    if (!decodeOnly) {
        mmkv->setFloat(3.14f, "float");
    }
    cout << "float = " << mmkv->getFloatForKey("float") << endl;

    if (!decodeOnly) {
        mmkv->setDouble(numeric_limits<double>::max(), "double");
    }
    cout << "double = " << mmkv->getDoubleForKey("double") << endl;

    if (!decodeOnly) {
        mmkv->setStringForKey("Hello, MMKV-н╒пе for Win32 ", "string");
    }
    string result;
    mmkv->getStringForKey("string", result);
    cout << "string = " << result << endl;
}

constexpr auto keyCount = 10000;
constexpr auto threadCount = 10;
static const string threadMMKVID = "thread_test";
vector<string> arrIntKeys;
vector<string> arrStringKeys;

DWORD WINAPI threadFunction(LPVOID lpParam) {
    auto threadIndex = (size_t) lpParam;
    auto mmkv = MMKV::mmkvWithID(threadMMKVID, 0, MMKV_MULTI_PROCESS);
    mmkv->lock();
    cout << "thread " << threadIndex << " starts" << endl;
    mmkv->unlock();

    auto segmentCount = keyCount / threadCount;
    auto startIndex = segmentCount * threadIndex;
    for (auto i = startIndex; i < startIndex + segmentCount; i++) {
        mmkv->setInt32(i, arrIntKeys[i]);
        mmkv->setStringForKey("str-" + i, arrStringKeys[i]);
    }

    mmkv->lock();
    cout << "thread " << threadIndex << " ends" << endl;
    mmkv->unlock();
    return 0;
}

void threadTest() {

    HANDLE threadHandles[threadCount] = {0};
    for (size_t i = 0; i < threadCount; i++) {
        threadHandles[i] = CreateThread(nullptr, 0, threadFunction, (LPVOID) i, 0, nullptr);
    }
    WaitForMultipleObjects(threadCount, threadHandles, true, INFINITE);

    auto mmkv = MMKV::mmkvWithID(threadMMKVID, 0, MMKV_MULTI_PROCESS);
    cout << "total count " << mmkv->count() << endl;
}

void brutleTest() {
    auto mmkv = MMKV::mmkvWithID(threadMMKVID);
    for (size_t i = 0; i < keyCount; i++) {
        mmkv->setInt32(i, arrIntKeys[i]);
        mmkv->setStringForKey("str-" + i, arrStringKeys[i]);
    }
}

int main() {
    locale::global(locale(""));
    wcout.imbue(locale(""));
    srand(GetTickCount());

    wstring rootDir = getAppDataRoaming(L"Tencent", L"н╒пе-MMKV");
    MMKV::initializeMMKV(rootDir);

    auto mmkv = MMKV::defaultMMKV();
    functionalTest(mmkv, false);

    for (size_t i = 0; i < keyCount; i++) {
        arrIntKeys.push_back("int-" + to_string(i));
        arrStringKeys.push_back("string-" + to_string(i));
    }

    threadTest();
    //brutleTest();
}
