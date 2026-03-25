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
#include <cassert>

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

string to_string(const vector<string> &arr, const char *sp = ", ") {
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

        mmkv->set("Hello, MMKV-微信 for Win32", "string");
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
        if (!CreateProcess(path, nullptr, nullptr, nullptr, false, 0, nullptr, nullptr, &si[index], &pi[index])) {
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

size_t getpagesize(void) {
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return system_info.dwPageSize;
}

void cornetSizeTest() {
    auto cryptKey = string("aes");
    auto mmkv = MMKV::mmkvWithID("cornerSize", MMKV_MULTI_PROCESS, &cryptKey);
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
    auto cryptKey = string("aes");
    auto mmkv = MMKV::mmkvWithID("fastRemoveCornerSize", MMKV_MULTI_PROCESS, &cryptKey);
    mmkv->clearAll();
    int64_t size = getpagesize() - 4;
    size -= 4;
    string key = "key";
    int64_t keySize = 3LL + 1LL;
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
void testBackup() {
    auto aesKey = string("cryptKey");
    auto mmapID = string("testEncrypt");
    auto mmkvRoot = MMKV::getRootDir();
    auto pos = mmkvRoot.rfind(L"\\");
    pos++;
    auto rootDir = mmkvRoot.replace(pos, wstring::npos, L"mmkv_backup");
    //wcout << "backup dir: " << mmkvRoot << endl;
    MMKV *mmkv = nullptr;
    //mmkv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, &aesKey);
    //mmkv->close();

	auto ret = MMKV::backupOneToDirectory(mmapID, rootDir);
    printf("backup one return %d\n", ret);
    if (ret) {
        mmkv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, &aesKey, &rootDir);
        cout << "after backup allKeys: " << ::to_string(mmkv->allKeys()) << endl;

		// otherwise it will fail in MMKV::backupAllToDirectory()
        mmkv->close();
    }

    auto count = MMKV::backupAllToDirectory(rootDir);
    printf("backup all count: %zu\n", count);
    if (count > 0) {
        mmkv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, &aesKey, &rootDir);
        cout << "check on backup [" << mmkv->mmapID() << "] allKeys: " << ::to_string(mmkv->allKeys(), ",\n") << endl;
        
        mmkv = MMKV::mmkvWithID("thread_test", MMKV_SINGLE_PROCESS, nullptr, &rootDir);
        cout << "check on backup [" << mmkv->mmapID() << "] allKeys count: " << mmkv->count() << endl;

        mmkv = MMKV::mmkvWithID("process_test", MMKV_MULTI_PROCESS, nullptr, &rootDir);
        cout << "check on backup [" << mmkv->mmapID() << "] allKeys count: " << mmkv->count() << endl;
    }
}

void testRestore() {
    auto aesKey = string("cryptKey");
    auto mmapID = string("testEncrypt");
    auto mmkvRoot = MMKV::getRootDir();
    auto pos = mmkvRoot.rfind(L"\\");
    pos++;
    auto rootDir = mmkvRoot.replace(pos, wstring::npos, L"mmkv_backup");
    //wcout << "restore dir: " << mmkvRoot << endl;
    auto mmkv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, &aesKey);
    mmkv->set((size_t)__LINE__, "test_restore_key");
    cout << "before restore [" << mmkv->mmapID() << "] allKeys: " << ::to_string(mmkv->allKeys(), ",\n") << endl;

    auto ret = MMKV::restoreOneFromDirectory(mmapID, rootDir);
    printf("restore one return %d\n", ret);
    if (ret) {
        cout << "after restore [" << mmkv->mmapID() << "] allKeys: " << ::to_string(mmkv->allKeys(), ",\n") << endl;
    }

    auto count = MMKV::restoreAllFromDirectory(rootDir);
    printf("restore all count: %zu\n", count);
    if (count > 0) {
        mmkv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, &aesKey);
        cout << "check on restore [" << mmkv->mmapID() << "] allKeys: " << ::to_string(mmkv->allKeys(), ",\n") << endl;

        mmkv = MMKV::mmkvWithID("thread_test");
        cout << "check on restore [" << mmkv->mmapID() << "] allKeys count: " << mmkv->count() << endl;

        mmkv = MMKV::mmkvWithID("process_test", MMKV_MULTI_PROCESS);
        cout << "check on restore [" << mmkv->mmapID() << "] allKeys count: " << mmkv->count() << endl;
    }
}

void testAutoExpire() {
    string mmapID = "testAutoExpire";
    // disable auto expire by config
    auto config = MMKVConfig();
    config.enableKeyExpire = false;
    config.recover = OnErrorRecover;
    // config.itemSizeLimit = 1;
    auto mmkv = MMKV::mmkvWithID(mmapID, config);
    mmkv->clearAll();
    mmkv->trim();
    mmkv->disableAutoKeyExpire(); // this call become a no-op

    mmkv->set(true, "auto_expire_key_1");

    // enable auto expire by config
    mmkv->close();
    config.enableKeyExpire = true;
    config.expiredInSeconds = 1;
    mmkv = MMKV::mmkvWithID(mmapID, config);
    mmkv->enableAutoKeyExpire(1); // this call become a no-op

    mmkv->set("never_expire_key_1", "never_expire_key_1", MMKV::ExpireNever);

    Sleep(2 * 1000);
    assert(mmkv->containsKey("auto_expire_key_1") == false);
    assert(mmkv->containsKey("never_expire_key_1") == true);

    mmkv->removeValueForKey("never_expire_key_1");
    mmkv->enableAutoKeyExpire(MMKV::ExpireNever);
    mmkv->set("never_expire_key_1", "never_expire_key_1");
    mmkv->set(true, "auto_expire_key_1", 1);
    Sleep(2 * 1000);
    assert(mmkv->containsKey("never_expire_key_1") == true);
    assert(mmkv->containsKey("auto_expire_key_1") == false);

    auto count = mmkv->count(true);
    cout << "count all non expire keys: " << count << endl;
    auto allKeys = mmkv->allKeys(true);
    cout << "all non expire keys: " << ::to_string(allKeys) << endl;
}

void testExpectedCapacity() {
    int len = 10000;
    std::string value(len, '0');
    value = "容量" + value;
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

void testRemoveStorage() {
    string mmapID = "test_remove";
    {
        auto mmkv = MMKV::mmkvWithID(mmapID, MMKV_MULTI_PROCESS);
        mmkv->set(true, "bool");
    }
    printf("check exist: %d\n", MMKV::checkExist(mmapID));
    MMKV::removeStorage(mmapID);
    printf("after remove, check exist: %d\n", MMKV::checkExist(mmapID));
    {
        auto mmkv = MMKV::mmkvWithID(mmapID, MMKV_MULTI_PROCESS);
        if (mmkv->count() != 0) {
            abort();
        }
    }

    mmapID = "test_remove/sg";
    auto rootDir = MMKV::getRootDir() + L"_1";
    auto mmkv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, nullptr, &rootDir);
    mmkv->set(true, "bool");
    printf("check exist: %d\n", MMKV::checkExist(mmapID, &rootDir));
    MMKV::removeStorage(mmapID, &rootDir);
    printf("after remove, check exist: %d\n", MMKV::checkExist(mmapID, &rootDir));
    mmkv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, nullptr, &rootDir);
    if (mmkv->count() != 0) {
        abort();
    }
}

void setReadOnly(const MMKVPath_t& path, bool readOnly) {
    // Get the current file attributes
    DWORD attributes = GetFileAttributes(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        // If the function fails, print an error message
        DWORD error = GetLastError();
        printf("Failed to get file attributes. Error code: %lu\n", error);
        return;
    }

    // alter the read-only attribute
    if (readOnly) {
        attributes |= FILE_ATTRIBUTE_READONLY;
    } else {
        attributes &= ~FILE_ATTRIBUTE_READONLY;
    }
    // Set the file attributes to the new value
    if (!SetFileAttributes(path.c_str(), attributes)) {
        // If the function fails, print an error message
        DWORD error = GetLastError();
        printf("Failed to set file attributes. Error code: %lu\n", error);
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

    auto path = MMKV::getRootDir() + MMKV_PATH_SLASH + string2MMKVPath_t(mmapID);
    setReadOnly(path, true);
    auto crcPath = path + L".crc";
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

void testNameSpace() {
    wstring rootDir = getAppDataRoaming(L"Tencent", L"微信-mmkv_namespace");
    // wstring rootDir = L"D:\\mmkv";
    auto ns = MMKV::nameSpace(rootDir);
    auto kv = ns.mmkvWithID("test_namespace");
    functionalTest(kv, false);
}

void testImport() {
    string mmapID = "test_import_src";
    auto src = MMKV::mmkvWithID(mmapID);
    src->set(true, "bool");
    src->set(std::numeric_limits<int32_t>::min(), "int");
    src->set(std::numeric_limits<uint64_t>::max(), "long");
    src->set("test import", "string");

    auto dst = MMKV::mmkvWithID("test_import_dst");
    dst->clearAll();
    dst->enableAutoKeyExpire(1);
    dst->set(true, "bool");
    dst->set(-1, "int");
    dst->set(0, "long");
    dst->set(mmapID, "string");

    auto count = dst->importFrom(src);
    assert(count == 4 && dst->count() == 4);
    assert(dst->getBool("bool"));
    assert(dst->getInt32("int") == std::numeric_limits<int32_t>::min());
    assert(dst->getUInt64("long") == std::numeric_limits<uint64_t>::max());
    string result;
    dst->getString("string", result);
    assert(result == "test import");
    Sleep(2 * 1000);
    assert(dst->count(true) == 0);
}

MMKV* testMMKV(const string& mmapID, const string* cryptKey, bool aes256, bool decodeOnly, const wstring* rootPath) {
    MMKV* kv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, cryptKey, rootPath, 0, aes256);
    functionalTest(kv, decodeOnly);
    return kv;
}

void testReKey() {
    string mmapID = "test/AES_reKey1";
    MMKV* kv = testMMKV(mmapID, nullptr, false, false, nullptr);

    string cryptKey = "Key_seq_1";
    kv->reKey(cryptKey);
    kv->clearMemoryCache();
    testMMKV(mmapID, &cryptKey, false, true, nullptr);

    string cryptKey2 = "Key_Seq_Very_Looooooooong";
    kv->reKey(cryptKey2, true);
    kv->clearMemoryCache();
    testMMKV(mmapID, &cryptKey2, true, true, nullptr);

    kv->reKey(string());
    kv->clearMemoryCache();
    testMMKV(mmapID, nullptr, false, true, nullptr);
}

// Helper to write raw bytes or convert to WideChar based on destination
void WriteUTF8ToStream(const char* utf8_str) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD consoleMode;

    if (GetConsoleMode(hOut, &consoleMode)) {
        // --- CONSOLE: Convert to UTF-16 and write ---
        int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
        if (wlen > 0) {
            wchar_t* wbuf = (wchar_t*)malloc(wlen * sizeof(wchar_t));
            if (wbuf) {
                MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wbuf, wlen);
                WriteConsoleW(hOut, wbuf, wlen - 1, NULL, NULL);
                free(wbuf);
            }
        }
    }
    else {
        // --- FILE/PIPE: Write raw UTF-8 bytes ---
        DWORD bytesWritten;
        WriteFile(hOut, utf8_str, (DWORD)strlen(utf8_str), &bytesWritten, NULL);
    }
}

// Main VarArg Function
void PrintUTF8(const char* format, ...) {
    va_list args;

    // 1. Calculate required length
    // We pass NULL/0 to vsnprintf just to get the required size (excluding null terminator)
    va_start(args, format);
    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (len < 0) return; // Encoding error or invalid format

    // 2. Allocate buffer (len + 1 for null terminator)
    // Using malloc ensures we don't overflow the stack with huge strings
    char* buf = (char*)malloc(len + 1);
    if (!buf) return; // Out of memory

    // 3. Format the string into the buffer
    va_start(args, format);
    vsnprintf(buf, len + 1, format, args);
    va_end(args);

    // 4. Send to output helper
    WriteUTF8ToStream(buf);

    // 5. Cleanup
    free(buf);
}

class MyMMKVHandler : public mmkv::MMKVHandler {
public:
    void mmkvLog(MMKVLogLevel level, const char* file, int line, const char* function, MMKVLog_t message) override {
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
        PrintUTF8("redirecting-[%s] <%s:%d::%s> %s\n", desc, file, line, function, message.c_str());
    }
};

static MyMMKVHandler g_handler;

int main() {
    // Get the original global locale
    std::locale originalLocale = std::locale::global(std::locale());
    std::cout << "Original locale: " << originalLocale.name() << std::endl;
    //locale::global(locale(""));
    //locale::global(locale(".UTF8"));
    //SetConsoleOutputCP(CP_UTF8);
    //SetConsoleCP(CP_UTF8);

    std::locale newLocale = std::locale();
    std::cout << "New locale: " << newLocale.name() << std::endl;
    // wcout.imbue(locale(""));

    srand((unsigned) GetTickCount64());

    // test NameSpace before initializeMMKV()
    testNameSpace();

    wstring rootDir = getAppDataRoaming(L"Tencent", L"微信-MMKV");
    MMKV::initializeMMKV(rootDir, MMKVLogInfo, &g_handler);
    //MMKV::setLogLevel(MMKVLogNone);
    //MMKV::registerLogHandler(LogHandler);

    //auto mmkv = MMKV::defaultMMKV();
    auto cryptKey = string("cryptKey");
    auto mmkv = MMKV::mmkvWithID("testEncrypt", MMKV_SINGLE_PROCESS, &cryptKey);
    functionalTest(mmkv, false);

    for (size_t index = 0; index < keyCount; index++) {
        arrIntKeys.push_back("int-" + to_string(index));
        arrStringKeys.push_back("string-" + to_string(index));
    }

    //fastRemoveCornetSizeTest();
    cornetSizeTest();
    //brutleTest();
    threadTest();
    processTest();
    testBackup();
    testRestore();
    testAutoExpire();
    testExpectedCapacity();
    testRemoveStorage();
    testReadOnly();
    testImport();
    testReKey();
}
