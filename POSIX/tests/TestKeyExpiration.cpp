/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2026 THL A29 Limited, a Tencent company.
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

#include <MMKV/MMKV.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

// GNU ld wraps only this executable's time() calls, including the static Core.
// No sleeps or production clock hooks are needed to cross expiration boundaries.
static std::time_t now = 1700000000;
static size_t timeCalls = 0;
extern "C" std::time_t __wrap_time(std::time_t *result) {
    timeCalls++;
    if (result) {
        *result = now;
    }
    return now;
}

static void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

static std::vector<char> snapshot(MMKV *kv, const char *suffix) {
    const auto path = MMKV::getRootDir() + "/" + kv->mmapID() + suffix;
    std::ifstream input(path, std::ios::binary);
    require(input.is_open(), "cannot open store snapshot");
    std::vector<char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    require(!input.bad(), "cannot read store snapshot");
    return bytes;
}

static void expectKeys(MMKV *kv, std::vector<std::string> expected, bool countFirst) {
    if (countFirst) {
        require(kv->count(true) == expected.size(), "filtered count includes expired keys");
    }
    auto actual = kv->allKeys(true);
    std::sort(actual.begin(), actual.end());
    std::sort(expected.begin(), expected.end());
    require(actual == expected, "filtered enumeration includes expired keys");
    require(kv->count(true) == expected.size(), "filtered count disagrees with enumeration");
    for (const auto &key : expected) {
        const auto value = key == "second" ? std::string(256, 'x') : key == "first" ? "short" : "forever";
        std::string actualValue;
        require(kv->getString(key, actualValue) && actualValue == value, "surviving value changed during cleanup");
    }
}

static void testExpiration(bool encrypted, bool countFirst, bool allExpire, int reopenMode) {
    now += 100;
    const std::string id = "expiration-" + std::to_string(encrypted) + std::to_string(countFirst) +
                           std::to_string(allExpire) + std::to_string(reopenMode);
    const std::string cryptKey = "expiration-test";
    const auto *key = encrypted ? &cryptKey : nullptr;
    auto *kv = MMKV::mmkvWithID(id, MMKV_SINGLE_PROCESS, key);
    require(kv != nullptr, "cannot open MMKV");
    require(kv->enableAutoKeyExpire(), "cannot enable expiration");
    require(kv->set("short", "first", 1), "cannot write first key");
    require(kv->set(std::string(256, 'x'), "second", 3), "cannot write second key");
    if (!allExpire) {
        require(kv->set("forever", "permanent", 0), "cannot write permanent key");
    }
    if (reopenMode == 2) {
        kv->close();
        kv = MMKV::mmkvWithID(id, static_cast<MMKVMode>(MMKV_SINGLE_PROCESS | MMKV_READ_ONLY), key);
        require(kv != nullptr && kv->isReadOnly(), "cannot reopen read-only");
    }
    auto initial = allExpire ? std::vector<std::string>{"first", "second"}
                             : std::vector<std::string>{"first", "second", "permanent"};
    expectKeys(kv, initial, countFirst);
    now += 2;
    auto survivors = allExpire ? std::vector<std::string>{"second"} : std::vector<std::string>{"second", "permanent"};
    expectKeys(kv, survivors, countFirst);
    const auto data = snapshot(kv, "");
    const auto metadata = snapshot(kv, ".crc");
    expectKeys(kv, survivors, countFirst);
    require(snapshot(kv, "") == data, "unchanged enumeration rewrote the data");
    require(snapshot(kv, ".crc") == metadata, "unchanged enumeration rewrote the metadata");
    if (reopenMode == 1) {
        kv->close();
        kv = MMKV::mmkvWithID(id, MMKV_SINGLE_PROCESS, key);
        require(kv != nullptr, "cannot reopen MMKV");
    }
    now += 2;
    const auto remaining = allExpire ? std::vector<std::string>{} : std::vector<std::string>{"permanent"};
    expectKeys(kv, remaining, countFirst);
    require(!kv->containsKey("second"), "expired key still exists");
    kv->close();
    kv = MMKV::mmkvWithID(id, MMKV_SINGLE_PROCESS, key);
    require(kv != nullptr, "cannot reopen after expiration");
    expectKeys(kv, remaining, countFirst);
    kv->close();
}

static void testCompletedWritebackFastPath(bool encrypted) {
    now += 100;
    const std::string id = encrypted ? "fast-path-encrypted" : "fast-path-plain";
    const std::string cryptKey = "expiration-test";
    auto *kv = MMKV::mmkvWithID(id, MMKV_SINGLE_PROCESS, encrypted ? &cryptKey : nullptr);
    require(kv != nullptr, "cannot open fast-path MMKV");
    require(kv->enableAutoKeyExpire(), "cannot enable fast-path expiration");
    // Keep the compacted store larger than one page so every trim reaches fullWriteback().
    for (int i = 0; i < 512; i++) {
        require(kv->set(std::string(256, 'x'), "long-lived-" + std::to_string(i), 600), "cannot write long-lived key");
    }
    require(kv->set("short", "expired-later", 1), "cannot write short-lived key");
    require(kv->set("remove", "first") && kv->set("remove", "second"), "cannot write removable values");
    require(kv->removeValuesForKeys({"first", "second"}), "cannot complete writeback");
    require(kv->actualSize() > static_cast<size_t>(sysconf(_SC_PAGESIZE)), "fast-path store is too small");
    now += 2;
    const auto callsBeforeTrim = timeCalls;
    for (int i = 0; i < 100; i++) {
        kv->trim();
    }
    std::printf("Completed-writeback trims (%s): %zu clock reads for 100 calls\n", encrypted ? "encrypted" : "plain",
                timeCalls - callsBeforeTrim);
    require(timeCalls == callsBeforeTrim, "ordinary trim rescanned expiration after completed writeback");
    require(kv->count() == 513, "ordinary trim unexpectedly filtered expired keys");
    require(kv->count(true) == 512, "explicit expiration cleanup skipped after ordinary trim");
    require(kv->allKeys(true).size() == 512, "filtered enumeration disagrees after ordinary trim");
    std::string value;
    require(kv->getString("long-lived-0", value) && value == std::string(256, 'x'), "trim changed surviving value");
    kv->close();
}

static void testWithoutExpiration() {
    auto *kv = MMKV::mmkvWithID("no-expiration");
    require(kv != nullptr, "cannot open non-expiring MMKV");
    require(kv->set("forever", "permanent"), "cannot write non-expiring value");
    require(kv->set("remove", "first") && kv->set("remove", "second"), "cannot write removable values");
    require(kv->removeValuesForKeys({"first", "second"}), "cannot compact non-expiring MMKV");
    const auto compactedSize = kv->actualSize();
    now += 100;
    expectKeys(kv, {"permanent"}, true);
    expectKeys(kv, {"permanent"}, false);
    require(kv->actualSize() == compactedSize, "non-expiring enumeration changed the data size");
    kv->close();
}

int main() {
    char root[] = "mmkv-expiration-XXXXXX";
    if (!mkdtemp(root)) {
        return 1;
    }
    int result = 0;
    MMKV::initializeMMKV(std::filesystem::absolute(root).string(), MMKVLogWarning);
    try {
        int cases = 0;
        for (bool encrypted : {false, true}) {
#ifdef MMKV_DISABLE_CRYPT
            if (encrypted) {
                continue;
            }
#endif
            testCompletedWritebackFastPath(encrypted);
            for (bool countFirst : {false, true}) {
                for (bool allExpire : {false, true}) {
                    for (int reopenMode : {0, 1, 2}) {
                        testExpiration(encrypted, countFirst, allExpire, reopenMode);
                        cases++;
                    }
                }
            }
        }
        testWithoutExpiration();
        std::printf("Passed %d expiration cases and non-expiring enumeration\n", cases);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Key expiration regression: %s\n", error.what());
        result = 1;
    }
    MMKV::onExit();
    std::filesystem::remove_all(root);
    return result;
}
