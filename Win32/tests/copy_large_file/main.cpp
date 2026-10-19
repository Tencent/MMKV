// Exercise the handle-based copy path so this regression fails even before
// path-based restores enable post-copy truncation in PR #1697.
#include "MemoryFile.h"
#include <winioctl.h>

#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using mmkv::File;
using mmkv::OpenFlag;

static void check(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct ScratchDirectory {
    fs::path path = fs::temp_directory_path() /
                    (L"mmkv-copy-large-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                     std::to_wstring(GetTickCount()));

    ScratchDirectory() {
        check(fs::create_directory(path), "scratch directory already exists");
    }

    ~ScratchDirectory() {
        std::error_code error;
        fs::remove_all(path, error);
        if (error) {
            std::fprintf(stderr, "Could not remove test scratch directory: %s\n", error.message().c_str());
        }
    }
};

static void seek(HANDLE file, size_t offset) {
    LARGE_INTEGER position = {};
    position.QuadPart = offset;
    check(SetFilePointerEx(file, position, nullptr, FILE_BEGIN) != 0, "SetFilePointerEx failed");
}

static void writeByte(HANDLE file, size_t offset, char value) {
    seek(file, offset);
    DWORD written = 0;
    check(WriteFile(file, &value, 1, &written, nullptr) && written == 1, "WriteFile failed");
}

static char readByte(HANDLE file, size_t offset) {
    seek(file, offset);
    char value = 0;
    DWORD read = 0;
    check(ReadFile(file, &value, 1, &read, nullptr) && read == 1, "ReadFile failed");
    return value;
}

static void createSparseFile(const fs::path &path, size_t size, char marker) {
    File file(path.native(), OpenFlag::ReadWrite | OpenFlag::Create);
    check(file.isFileValid(), "could not create test file");
    DWORD returned = 0;
    check(DeviceIoControl(file.getFd(), FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned, nullptr) != 0,
          "test requires a filesystem supporting sparse files");
    seek(file.getFd(), size);
    check(SetEndOfFile(file.getFd()) != 0, "could not size test file");
    writeByte(file.getFd(), 0, marker);
    writeByte(file.getFd(), size - 1, marker);
}

static void testCopy(const fs::path &root, const wchar_t *name, size_t sourceSize) {
    auto source = root / (std::wstring(name) + L"-source");
    auto destination = root / (std::wstring(name) + L"-destination");
    createSparseFile(source, sourceSize, 'S');
    createSparseFile(destination, sourceSize + 4096, 'D');
    File file(destination.native(), OpenFlag::ReadWrite);
    check(file.isFileValid(), "could not open destination");
    writeByte(file.getFd(), sourceSize / 2, 'D');

    bool copied = mmkv::copyFileContent(source.native(), file.getFd());
    auto destinationSize = file.getActualFileSize();
    std::printf("source=%zu destination=%zu copied=%d\n", sourceSize, destinationSize, copied);
    check(copied, "copyFileContent failed");
    check(destinationSize == sourceSize, "destination was not truncated to the source size");
    check(readByte(file.getFd(), 0) == 'S', "first byte was not copied");
    check(readByte(file.getFd(), sourceSize / 2) == 0, "sparse source content was not copied");
    check(readByte(file.getFd(), sourceSize - 1) == 'S', "last byte was not copied");
}

int main() {
    try {
        ScratchDirectory scratch;
        constexpr size_t boundary = size_t{1} << 31;
        check(fs::space(scratch.path).available > uint64_t{boundary} + 16 * 1024 * 1024,
              "test requires slightly more than 2 GiB of free disk space");
        testCopy(scratch.path, L"small", 4096);
        testCopy(scratch.path, L"2GiB", boundary);
        std::puts("copy_large_file_test: 2/2 passed");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "copy_large_file_test: FAILED: %s\n", error.what());
        return 1;
    }
}
