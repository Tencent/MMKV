// Win32Demo.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

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
}

int main() {
    locale::global(locale(""));
    wcout.imbue(locale(""));

    wstring rootDir = getAppDataRoaming(L"Tencent", L"н╒пе-MMKV");
    MMKV::initializeMMKV(rootDir);

    auto mmkv = MMKV::defaultMMKV();
    functionalTest(mmkv, false);
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started:
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
