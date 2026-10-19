// Cross-process regression test for MMKV restores on Windows.
//
// MMKV::restoreOneFromDirectory() restores an uncached destination by opening the destination
// file and copying the backup into it. Opening that file with CREATE_ALWAYS fails with
// ERROR_USER_MAPPED_FILE (1224) while another process keeps the destination mapped, even when
// the backup and the destination have the same size. This test starts a second process that
// really maps the destination and then restores an equal-size backup.
//
// The test also covers the two size-changing cases the restore has to keep working:
// an un-mapped destination larger than the backup is truncated to the backup size, and a mapped
// destination larger than the backup keeps its size (Windows cannot truncate a mapped file)
// while still receiving the restored payload and CRC.

#include <MMKV/MMKV.h>
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <functional>
#include <string>

using namespace std;
namespace fs = std::filesystem;

static const char *MMAP_ID = "restore_mapped";
static const char *PAYLOAD_KEY = "payload";

[[noreturn]] static void fail(const string &message) {
    printf("restore_mapped_test: FAILED: %s\n", message.c_str());
    fflush(stdout);
    exit(EXIT_FAILURE);
}

static void check(bool condition, const string &message) {
    if (!condition) {
        fail(message);
    }
}

static wstring joinPath(const wstring &dir, const wstring &name) {
    if (!dir.empty() && (dir.back() == L'\\' || dir.back() == L'/')) {
        return dir + name;
    }
    return dir + L"\\" + name;
}

static wstring uniqueName(const wchar_t *suffix) {
    return L"mmkv-restore-mapped-" + to_wstring(GetCurrentProcessId()) + L"-" + suffix;
}

static wstring makeScratchRoot(int argc, wchar_t **argv) {
    wstring base;
    if (argc >= 2) {
        base = argv[1];
        check(fs::is_directory(base), "the scratch directory passed on the command line does not exist");
    } else {
        wchar_t tempPath[MAX_PATH] = {0};
        auto length = GetTempPathW(MAX_PATH, tempPath);
        check(length > 0 && length < MAX_PATH, "GetTempPathW");
        base.assign(tempPath, length);
    }
    auto root = joinPath(base, uniqueName(L"scratch"));
    check(!fs::exists(root), "the scratch directory already exists: " + MMKVPath_t2String(root));
    fs::create_directories(root);
    return root;
}

static void writeInstance(const wstring &rootDir, const string &value) {
    auto mmkv = MMKV::mmkvWithID(MMAP_ID, MMKV_SINGLE_PROCESS, nullptr, &rootDir);
    check(mmkv != nullptr, "create the MMKV instance under " + MMKVPath_t2String(rootDir));
    mmkv->clearAll();
    check(mmkv->set(value, PAYLOAD_KEY), "write the payload");
    mmkv->sync(MMKV_SYNC);
    mmkv->close();
}

static string readPayload(const wstring &rootDir) {
    auto mmkv = MMKV::mmkvWithID(MMAP_ID, MMKV_SINGLE_PROCESS, nullptr, &rootDir);
    check(mmkv != nullptr, "open the restored MMKV instance");
    string value;
    check(mmkv->getString(PAYLOAD_KEY, value), "read the restored payload");
    mmkv->close();
    return value;
}

// Runs in the second process: keep the destination mapped until the parent signals stop.
static int holdMappedDestination(const wstring &rootDir, const wstring &readyName, const wstring &stopName) {
    MMKV::initializeMMKV(rootDir, MMKVLogWarning);
    auto mmkv = MMKV::mmkvWithID(MMAP_ID, MMKV_MULTI_PROCESS);
    if (!mmkv) {
        printf("restore_mapped_test child: fail to open %s\n", MMAP_ID);
        return 1;
    }
    string value;
    if (!mmkv->getString(PAYLOAD_KEY, value)) { // force the file to be mapped
        printf("restore_mapped_test child: fail to read the destination payload\n");
        return 1;
    }

    auto ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, readyName.c_str());
    if (!ready) {
        printf("restore_mapped_test child: fail to open the ready event, error %lu\n", GetLastError());
        return 1;
    }
    SetEvent(ready);
    CloseHandle(ready);

    auto stop = OpenEventW(SYNCHRONIZE, FALSE, stopName.c_str());
    if (!stop) {
        printf("restore_mapped_test child: fail to open the stop event, error %lu\n", GetLastError());
        return 1;
    }
    auto waited = WaitForSingleObject(stop, 60 * 1000);
    CloseHandle(stop);
    return waited == WAIT_OBJECT_0 ? 0 : 1;
}

// Starts this executable again so the destination is mapped by another process.
static void withMappedDestination(const wstring &dstDir, const std::function<void()> &restore) {
    auto readyName = uniqueName(L"ready");
    auto stopName = uniqueName(L"stop");
    auto ready = CreateEventW(nullptr, TRUE, FALSE, readyName.c_str());
    auto stop = CreateEventW(nullptr, TRUE, FALSE, stopName.c_str());
    check(ready != nullptr && stop != nullptr, "create the synchronization events");

    wchar_t exePath[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wstring commandLine = L"\"" + wstring(exePath) + L"\" --hold-mapped \"" + dstDir + L"\" \"" + readyName + L"\" \"" +
                          stopName + L"\"";
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    check(CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi) != 0,
          "start the process that maps the destination");

    check(WaitForSingleObject(ready, 30 * 1000) == WAIT_OBJECT_0, "wait until the destination is mapped");
    restore();

    SetEvent(stop);
    check(WaitForSingleObject(pi.hProcess, 30 * 1000) == WAIT_OBJECT_0, "wait for the mapping process to exit");
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    check(exitCode == 0, "the mapping process exited with an error");
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(ready);
    CloseHandle(stop);
}

static void testTruncatesLargerDestination(const wstring &rootDir) {
    auto backupDir = joinPath(rootDir, L"truncate-backup");
    auto dstDir = joinPath(rootDir, L"truncate-dst");
    fs::create_directories(backupDir);
    fs::create_directories(dstDir);

    const string backupValue(100, 'a');
    const string dstValue(6000, 'b');
    writeInstance(backupDir, backupValue);
    writeInstance(dstDir, dstValue);

    auto backupPath = joinPath(backupDir, string2MMKVPath_t(MMAP_ID));
    auto dstPath = joinPath(dstDir, string2MMKVPath_t(MMAP_ID));
    auto backupSize = fs::file_size(backupPath);
    check(backupSize < fs::file_size(dstPath), "the destination should start larger than the backup");

    check(MMKV::restoreOneFromDirectory(MMAP_ID, backupDir, &dstDir),
          "restoreOneFromDirectory over a larger destination");
    check(fs::file_size(dstPath) == backupSize, "the larger destination should be truncated to the backup size");
    check(readPayload(dstDir) == backupValue, "the truncated destination should contain the backup payload");
    printf("restore_mapped_test: larger un-mapped destination truncation passed\n");
}

static void testMappedEqualSizeRestore(const wstring &rootDir) {
    auto backupDir = joinPath(rootDir, L"equal-backup");
    auto dstDir = joinPath(rootDir, L"equal-dst");
    fs::create_directories(backupDir);
    fs::create_directories(dstDir);

    const string backupValue(100, 'c');
    const string dstValue(100, 'd');
    writeInstance(backupDir, backupValue);
    writeInstance(dstDir, dstValue);

    auto backupPath = joinPath(backupDir, string2MMKVPath_t(MMAP_ID));
    auto dstPath = joinPath(dstDir, string2MMKVPath_t(MMAP_ID));
    check(fs::file_size(backupPath) == fs::file_size(dstPath),
          "the backup and the destination should have the same size");

    withMappedDestination(dstDir, [&] {
        // no MMKV instance for the destination in this process: the restore takes the by-file-path
        // path, which opens the destination with the same flags as the reported failure
        check(MMKV::restoreOneFromDirectory(MMAP_ID, backupDir, &dstDir),
              "restoreOneFromDirectory over a mapped equal-size destination");
        check(MMKV::restoreAllFromDirectory(backupDir, &dstDir) == 1,
              "restoreAllFromDirectory over a mapped equal-size destination");
    });

    check(readPayload(dstDir) == backupValue, "the mapped destination should contain the backup payload");
    printf("restore_mapped_test: equal-size mapped destination passed\n");
}

static void testMappedLargerDestinationKeepsSize(const wstring &rootDir) {
    auto backupDir = joinPath(rootDir, L"mapped-larger-backup");
    auto dstDir = joinPath(rootDir, L"mapped-larger-dst");
    fs::create_directories(backupDir);
    fs::create_directories(dstDir);

    const string backupValue(100, 'e');
    const string dstValue(6000, 'f');
    writeInstance(backupDir, backupValue);
    writeInstance(dstDir, dstValue);

    auto backupPath = joinPath(backupDir, string2MMKVPath_t(MMAP_ID));
    auto dstPath = joinPath(dstDir, string2MMKVPath_t(MMAP_ID));
    auto backupSize = fs::file_size(backupPath);
    auto dstSize = fs::file_size(dstPath);
    check(backupSize < dstSize, "the mapped destination should start larger than the backup");

    withMappedDestination(dstDir, [&] {
        check(MMKV::restoreOneFromDirectory(MMAP_ID, backupDir, &dstDir),
              "restoreOneFromDirectory over a mapped larger destination");
    });

    // Windows cannot shrink a file that another process has mapped. The restore keeps the file
    // size, but copies both the payload and the CRC meta, so readers see the restored data.
    check(fs::file_size(dstPath) == dstSize, "the mapped destination should keep its original size");
    check(readPayload(dstDir) == backupValue, "the mapped larger destination should contain the backup payload");
    printf("restore_mapped_test: larger mapped destination kept its size and received the payload\n");
}

int wmain(int argc, wchar_t **argv) {
    if (argc >= 5 && wcscmp(argv[1], L"--hold-mapped") == 0) {
        return holdMappedDestination(argv[2], argv[3], argv[4]);
    }

    auto rootDir = makeScratchRoot(argc, argv);
    MMKV::initializeMMKV(rootDir, MMKVLogWarning);

    testTruncatesLargerDestination(rootDir);
    testMappedEqualSizeRestore(rootDir);
    testMappedLargerDestinationKeepsSize(rootDir);

    fs::remove_all(rootDir);
    printf("restore_mapped_test: 3/3 passed\n");
    return 0;
}
