// MMKV.cpp : Defines the exported functions for the DLL application.
//

#include "MMKV.h"

static int getpagesize(void) {
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return system_info.dwPageSize;
}

MMKV::MMKV() {}

MMKV::~MMKV() {}

int MMKV::foo() {
    OFSTRUCT of;
    HANDLE file = (HANDLE) CreateFile(L"testmm", GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == (HANDLE) HFILE_ERROR) {
        return -1;
    }
    SetFilePointer(file, getpagesize(), 0, FILE_BEGIN);
    SetEndOfFile(file);

    auto mmFile = CreateFileMapping(file, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (!mmFile) {
        return -2;
    }
    auto ptr = MapViewOfFile(mmFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!ptr) {
        return -3;
    }
    int value = 1024;
	//memcpy(ptr, &value, sizeof(value));
    memcpy(&value, ptr, sizeof(value));
    return value;
}
