// Link the static core so syscall hooks affect only this test executable.
#include "MMKV.h"
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <mutex>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

static void require(bool condition, const char *message) {
    if (!condition) {
        std::fprintf(stderr, "test removeStorage: %s\n", message);
        _exit(EXIT_FAILURE);
    }
}

using MutexLock = int (*)(pthread_mutex_t *);
using Flock = int (*)(int, int);
using Unlink = int (*)(const char *);
static MutexLock originalMutexLock;
static Flock originalFlock;
static Unlink originalUnlink;

static thread_local bool captureMutex = false;
static thread_local bool writerThread = false;
static thread_local bool removerThread = false;
static thread_local bool closeObserved = false;
static pthread_mutex_t *instanceMutex;
static dev_t crcDevice;
static ino_t crcInode;
static std::mutex gateMutex;
static std::condition_variable gateCondition;
static bool writerReady = false;
static bool writerReleased = false;
static std::thread *writer;

extern "C" int pthread_mutex_lock(pthread_mutex_t *mutex) noexcept(noexcept(::pthread_mutex_lock(mutex))) {
    if (!originalMutexLock) {
        originalMutexLock = reinterpret_cast<MutexLock>(dlsym(RTLD_NEXT, "pthread_mutex_lock"));
    }
    if (captureMutex) {
        instanceMutex = mutex;
    }
    if (removerThread && !closeObserved && mutex == instanceMutex) {
        closeObserved = true;
        {
            std::lock_guard<std::mutex> guard(gateMutex);
            writerReleased = true;
        }
        gateCondition.notify_one();
        // Complete the only outstanding operation before actually closing the
        // instance. This does not require racing instance destruction to work.
        writer->join();
    }
    return originalMutexLock(mutex);
}

extern "C" int flock(int fd, int operation) noexcept(noexcept(::flock(fd, operation))) {
    if (!originalFlock) {
        originalFlock = reinterpret_cast<Flock>(dlsym(RTLD_NEXT, "flock"));
    }
    if (writerThread && operation == LOCK_EX) {
        struct stat info {};
        require(fstat(fd, &info) == 0, "cannot identify writer CRC fd");
        if (info.st_dev == crcDevice && info.st_ino == crcInode) {
            std::unique_lock<std::mutex> guard(gateMutex);
            writerReady = true;
            gateCondition.notify_one();
            require(gateCondition.wait_for(guard, std::chrono::seconds(2), [] { return writerReleased; }),
                    "harness: remover did not reach the cached instance mutex");
            guard.unlock();
            // An immediate kernel result, rather than a deadlock timeout,
            // distinguishes the inverted order. No other holder exists here.
            const int result = originalFlock(fd, LOCK_EX | LOCK_NB);
            if (result != 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
                std::fprintf(stderr, "test removeStorage: lock inversion: deletion flock is held while "
                                     "close waits for the writer's instance mutex\n");
                _exit(2);
            }
            require(result == 0, "writer could not acquire the CRC flock");
            return result;
        }
    }
    return originalFlock(fd, operation);
}

static void sendByte(int fd, char value) {
    ssize_t result;
    do {
        result = write(fd, &value, 1);
    } while (result < 0 && errno == EINTR);
    require(result == 1, "harness: pipe write failed");
}

static char receiveByte(int fd) {
    pollfd descriptor {fd, POLLIN, 0};
    int result;
    do {
        result = poll(&descriptor, 1, 2000);
    } while (result < 0 && errno == EINTR);
    require(result == 1, "harness: pipe notification timed out");
    char value;
    require(read(fd, &value, 1) == 1, "harness: pipe notification missing");
    return value;
}

static std::string interceptedPath;
static int probeCommandFD = -1;
static int probeResponseFD = -1;
static int completionFD = -1;

extern "C" int unlink(const char *path) noexcept(noexcept(::unlink(path))) {
    if (!originalUnlink) {
        originalUnlink = reinterpret_cast<Unlink>(dlsym(RTLD_NEXT, "unlink"));
    }
    if (probeCommandFD >= 0 && interceptedPath == path) {
        sendByte(probeCommandFD, 'L');
        require(receiveByte(probeResponseFD) == 'B', "deletion did not retain cross-process exclusion");
    }
    return originalUnlink(path);
}

static MMKV *openKV(const char *id) {
    auto kv = MMKV::mmkvWithID(id, MMKV_MULTI_PROCESS);
    require(kv != nullptr, "cannot open storage");
    return kv;
}

static void assertRemovedAndReopen(const char *id) {
    require(!MMKV::checkExist(id), "storage remains after removal");
    auto reopened = openKV(id);
    require(!reopened->containsKey("key"), "reopened storage retains removed data");
    require(reopened->set(int32_t(3), "key"), "reopened storage is not writable");
    reopened->close();
    require(MMKV::removeStorage(id), "cannot remove reopened storage");
}

static void normalDeletion(const std::string &) {
    for (bool cached : {true, false}) {
        const char *id = cached ? "cached" : "uncached";
        auto kv = openKV(id);
        require(kv->set(int32_t(1), "key"), "initial write failed");
        if (!cached) {
            kv->close();
        }
        require(MMKV::removeStorage(id), "normal removal failed");
        // The cached pointer is intentionally never used after removeStorage.
        assertRemovedAndReopen(id);
    }
}

static void writerOrdering(const std::string &root) {
    auto kv = openKV("ordering");
    require(kv->set(int32_t(1), "key"), "initial write failed");
    struct stat info {};
    require(stat((root + "/ordering.crc").c_str(), &info) == 0, "cannot stat CRC file");
    crcDevice = info.st_dev;
    crcInode = info.st_ino;
    captureMutex = true;
    kv->lock_thread();
    captureMutex = false;
    kv->unlock_thread();
    require(instanceMutex != nullptr, "instance mutex hook was not reached");

    std::thread activeWriter([kv] {
        writerThread = true;
        require(kv->set(int32_t(2), "key"), "writer operation failed");
        writerThread = false;
    });
    writer = &activeWriter;
    {
        std::unique_lock<std::mutex> guard(gateMutex);
        require(gateCondition.wait_for(guard, std::chrono::seconds(2), [] { return writerReady; }),
                "harness: writer did not reach exclusive flock");
    }
    removerThread = true;
    require(MMKV::removeStorage("ordering"), "ordered removal failed");
    removerThread = false;
    require(closeObserved && !activeWriter.joinable(), "cached close did not finish the writer first");
    assertRemovedAndReopen("ordering");
}

static void crossProcessExclusion(const std::string &root) {
    for (bool cached : {true, false}) {
        const char *id = cached ? "excluded-cached" : "excluded-uncached";
        const auto crcPath = root + "/" + id + ".crc";
        int commands[2], responses[2];
        require(pipe(commands) == 0 && pipe(responses) == 0, "cannot create probe pipes");
        // Fork before creating this instance. The probe then opens CRC itself;
        // it cannot share an inherited flock open-file description with MMKV.
        pid_t probe = fork();
        require(probe >= 0, "cannot fork flock probe");
        if (probe == 0) {
            close(completionFD);
            close(commands[1]);
            close(responses[0]);
            require(receiveByte(commands[0]) == 'L', "missing deletion notification");
            int fd = open(crcPath.c_str(), O_RDONLY);
            require(fd >= 0, "probe cannot open separate CRC fd");
            int result = originalFlock(fd, LOCK_EX | LOCK_NB);
            bool excluded = result < 0 && (errno == EWOULDBLOCK || errno == EAGAIN);
            if (result == 0) {
                originalFlock(fd, LOCK_UN);
            }
            sendByte(responses[1], excluded ? 'B' : 'F');
            require(receiveByte(commands[0]) == 'R', "missing removal completion notification");
            require(originalFlock(fd, LOCK_EX | LOCK_NB) == 0, "deletion flock remains after removal");
            sendByte(responses[1], 'U');
            close(fd);
            _exit(0);
        }
        close(commands[0]);
        close(responses[1]);
        auto kv = openKV(id);
        require(kv->set(int32_t(1), "key"), "initial write failed");
        if (!cached) {
            kv->close();
        }
        interceptedPath = root + "/" + id;
        probeCommandFD = commands[1];
        probeResponseFD = responses[0];
        require(MMKV::removeStorage(id), "exclusion removal failed");
        probeCommandFD = probeResponseFD = -1;
        sendByte(commands[1], 'R');
        require(receiveByte(responses[0]) == 'U', "probe did not observe deletion lock release");
        close(commands[1]);
        close(responses[0]);
        int status;
        require(waitpid(probe, &status, 0) == probe && WIFEXITED(status) && WEXITSTATUS(status) == 0,
                "cross-process flock probe failed");
        assertRemovedAndReopen(id);
    }
}

static bool runScenario(void (*scenario)(const std::string &), const std::string &root) {
    int completion[2];
    require(pipe(completion) == 0, "cannot create completion pipe");
    pid_t child = fork();
    require(child >= 0, "cannot fork scenario");
    if (child == 0) {
        close(completion[0]);
        completionFD = completion[1];
        MMKV::initializeMMKV(root, MMKVLogNone);
        scenario(root);
        _exit(0);
    }
    close(completion[1]);
    pollfd descriptor {completion[0], POLLIN, 0};
    int result;
    do {
        result = poll(&descriptor, 1, 5000);
    } while (result < 0 && errno == EINTR);
    if (result != 1) {
        std::fprintf(stderr, "test removeStorage: harness scenario failed to finish\n");
        kill(child, SIGKILL);
    }
    close(completion[0]);
    int status;
    require(waitpid(child, &status, 0) == child, "cannot reap scenario");
    return result == 1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(int argc, char **argv) {
    // Resolve once before threads/forks; hooks remain confined to this binary.
    originalMutexLock = reinterpret_cast<MutexLock>(dlsym(RTLD_NEXT, "pthread_mutex_lock"));
    originalFlock = reinterpret_cast<Flock>(dlsym(RTLD_NEXT, "flock"));
    originalUnlink = reinterpret_cast<Unlink>(dlsym(RTLD_NEXT, "unlink"));
    require(originalMutexLock && originalFlock && originalUnlink, "cannot resolve syscall hooks");
    require(argc <= 2, "usage: TestRemoveStorage [scratch-directory]");
    const char *base = argc == 2 ? argv[1] : std::getenv("TMPDIR");
    std::string directory = std::string(base && *base ? base : "/tmp") + "/mmkv-remove-storage.XXXXXX";
    require(mkdtemp(directory.data()) != nullptr, "cannot create owned scratch directory");
    bool passed = runScenario(normalDeletion, directory);
    passed = runScenario(writerOrdering, directory) && passed;
    passed = runScenario(crossProcessExclusion, directory) && passed;
    // Only these paths in the directory created above can have been generated
    // by this test, including a failing child. Never inspect other temp paths.
    for (const char *id : {"cached", "uncached", "ordering", "excluded-cached", "excluded-uncached"}) {
        originalUnlink((directory + "/" + id).c_str());
        originalUnlink((directory + "/" + id + ".crc").c_str());
    }
    require(rmdir(directory.c_str()) == 0, "cannot remove owned scratch directory");
    require(passed, "a scenario failed");
    std::puts("test removeStorage: cached/uncached deletion, writer ordering, cross-process exclusion passed");
}
