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
        mmkv->set("Hello, MMKV-н╒пе for Win32", "string");
    }
    string result;
    mmkv->getString("string", result);
    cout << "string = " << result << endl;
}

constexpr auto keyCount = 10000;
constexpr auto threadCount = 10;
static const string MMKV_ID = "thread_test";
vector<string> arrIntKeys;
vector<string> arrStringKeys;

DWORD WINAPI threadFunction(LPVOID lpParam) {
    auto threadIndex = (size_t) lpParam;
    auto mmkv = MMKV::mmkvWithID(MMKV_ID);
    mmkv->lock();
    cout << "thread " << threadIndex << " starts" << endl;
    mmkv->unlock();

    auto segmentCount = keyCount / threadCount;
    auto startIndex = segmentCount * threadIndex;
    for (auto index = startIndex; index < startIndex + segmentCount; index++) {
        mmkv->set(index, arrIntKeys[index]);
        mmkv->set("str-" + to_string(index), arrStringKeys[index]);
    }

    mmkv->lock();
    cout << "thread " << threadIndex << " ends" << endl;
    mmkv->unlock();
    return 0;
}

void threadTest() {

    HANDLE threadHandles[threadCount] = {0};
    for (size_t index = 0; index < threadCount; index++) {
        threadHandles[index] = CreateThread(nullptr, 0, threadFunction, (LPVOID) index, 0, nullptr);
    }
    WaitForMultipleObjects(threadCount, threadHandles, true, INFINITE);

    auto mmkv = MMKV::mmkvWithID(MMKV_ID);
    cout << "total count " << mmkv->count() << endl;
}

void brutleTest() {
    auto mmkv = MMKV::mmkvWithID(MMKV_ID);
    for (size_t i = 0; i < keyCount; i++) {
        mmkv->set(i, arrIntKeys[i]);
        mmkv->set("str-" + to_string(i), arrStringKeys[i]);
    }
}

void processTest() {
    constexpr auto processCount = 2;
    STARTUPINFO si[processCount] = {0};
    PROCESS_INFORMATION pi[processCount] = {0};

    for (auto index = 0; index < processCount; index++) {
        si[index].cb = sizeof(si[0]);
    }

    wchar_t path[MAX_PATH] = {0};
    GetModuleFileName(nullptr, path, MAX_PATH);
    PathRemoveFileSpec(path);
    PathAppend(path, L"Win32DemoProcess.exe");

    HANDLE processHandles[processCount] = {0};
    for (auto index = 0; index < processCount; index++) {
        if (!CreateProcess(path, nullptr, nullptr, nullptr, false, 0, nullptr, nullptr, &si[index],
                           &pi[index])) {
            cout << "CreateProcess failed: " << GetLastError() << endl;
            continue;
        }
        processHandles[index] = pi[index].hProcess;
    }

    WaitForMultipleObjects(processCount, processHandles, true, INFINITE);

    for (auto index = 0; index < processCount; index++) {
        CloseHandle(pi[index].hProcess);
        CloseHandle(pi[index].hThread);
    }

    auto mmkv = MMKV::mmkvWithID("process_test", MMKV_MULTI_PROCESS);
    cout << "total count of process_test: " << mmkv->count() << endl;
}

static void LogHandler(MMKVLogLevel level,
                       const std::string &file,
                       int line,
                       const std::string &function,
                       const std::string &message) {

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
    printf("redirecting-[%s] <%s:%d::%s> %s\n", desc, file.c_str(), line, function.c_str(),
           message.c_str());
}

int main() {
    locale::global(locale(""));
    wcout.imbue(locale(""));
    srand(GetTickCount());

    wstring rootDir = getAppDataRoaming(L"Tencent", L"н╒пе-MMKV");
    MMKV::initializeMMKV(rootDir);
    //MMKV::setLogLevel(MMKVLogNone);
    MMKV::regiserLogHandler(LogHandler);

    auto mmkv = MMKV::defaultMMKV();
    functionalTest(mmkv, false);

    for (size_t index = 0; index < keyCount; index++) {
        arrIntKeys.push_back("int-" + to_string(index));
        arrStringKeys.push_back("string-" + to_string(index));
    }

    //brutleTest();
    threadTest();
    processTest();
}
