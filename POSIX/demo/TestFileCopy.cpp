#undef NDEBUG
#include "MMKV.h"
#include "MemoryFile.h"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <fcntl.h>
#include <string>
#include <unistd.h>

// Link against the static core so these hooks affect only this test executable.
static bool shortTransfers = false;
static bool interruptRead = false;
static bool interruptWrite = false;
static bool zeroWrite = false;
static int destinationFD = -1;

extern "C" ssize_t read(int fd, void *buffer, size_t size) {
    using Function = ssize_t (*)(int, void *, size_t);
    static auto original = reinterpret_cast<Function>(dlsym(RTLD_NEXT, "read"));
    if (interruptRead) {
        interruptRead = false;
        errno = EINTR;
        return -1;
    }
    return original(fd, buffer, shortTransfers ? std::min<size_t>(size, 7) : size);
}

extern "C" ssize_t write(int fd, const void *buffer, size_t size) {
    using Function = ssize_t (*)(int, const void *, size_t);
    static auto original = reinterpret_cast<Function>(dlsym(RTLD_NEXT, "write"));
    if (fd == destinationFD) {
        if (interruptWrite) {
            interruptWrite = false;
            errno = EINTR;
            return -1;
        }
        if (zeroWrite) {
            return 0;
        }
        if (shortTransfers) {
            size = std::min<size_t>(size, 3);
        }
    }
    return original(fd, buffer, size);
}

int main() {
    char directory[] = "/tmp/mmkv-file-copy.XXXXXX";
    assert(mkdtemp(directory));
    MMKV::initializeMMKV(directory, MMKVLogNone);
    const auto source = std::string(directory) + "/source";
    const auto destination = std::string(directory) + "/destination";
    const std::string data(257, 'v');
    auto sourceFD = open(source.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    assert(sourceFD >= 0);
    assert(write(sourceFD, data.data(), data.size()) == data.size());
    close(sourceFD);

    for (int mode = 0; mode < 4; mode++) {
        destinationFD = open(destination.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
        assert(destinationFD >= 0);
        assert(ftruncate(destinationFD, 512) == 0);
        shortTransfers = (mode != 0);
        interruptRead = interruptWrite = (mode == 2);
        zeroWrite = (mode == 3);
        auto copied = mmkv::copyFileContent(source, destinationFD);
        assert(copied == !zeroWrite);
        assert(!interruptRead && !interruptWrite);
        shortTransfers = zeroWrite = false;
        if (copied) {
            assert(lseek(destinationFD, 0, SEEK_END) == data.size());
            std::string result(data.size(), '\0');
            assert(pread(destinationFD, result.data(), result.size(), 0) == result.size());
            assert(result == data);
        }
        close(destinationFD);
        destinationFD = -1;
    }
    unlink(source.c_str());
    unlink(destination.c_str());
    rmdir(directory);
    printf("test file copy transfers: passed\n");
}
