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

#include "MMKV_IO.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace std;
using namespace mmkv;
namespace fs = std::filesystem;

static string legacyModifiedUtf8NulKey() {
    string key = "account";
    key.append("\xC0\x80", 2);
    key.append("admin");
    return key;
}

static void assertRejectedWithoutPartialImport(MMKV *source, MMKV *destination, const string &invalidKey) {
    source->clearAll();
    destination->clearAll();
    assert(source->set(int32_t{3}, "ordinary-source-key"));
    assert(source->set(int32_t{7}, invalidKey));
    assert(destination->set(int32_t{11}, "destination-only-key"));

    auto result = internal::CheckedImportAccess::importFrom(destination, source);
    assert(result.incompatibleKeyRejected);
    assert(result.count == 0);
    assert(destination->count() == 1);
    assert(destination->getInt32("destination-only-key") == 11);
    assert(!destination->containsKey("ordinary-source-key"));
    assert(!destination->containsKey(invalidKey));
}

static void assertReverseImportsComplete(MMKV *first, MMKV *second) {
    first->clearAll();
    second->clearAll();
    assert(first->set(int32_t{1}, "first-key"));
    assert(second->set(int32_t{2}, "second-key"));

    auto selfImport = internal::CheckedImportAccess::importFrom(first, first);
    assert(!selfImport.incompatibleKeyRejected);
    assert(selfImport.count == 1);

    atomic<bool> start{false};
    mutex completionMutex;
    condition_variable completionCondition;
    size_t completed = 0;
    auto worker = [&](MMKV *destination, MMKV *source) {
        while (!start.load(memory_order_acquire)) {
            this_thread::yield();
        }
        for (size_t iteration = 0; iteration < 200; ++iteration) {
            auto result = internal::CheckedImportAccess::importFrom(destination, source);
            assert(!result.incompatibleKeyRejected);
            assert(result.count > 0);
        }
        {
            lock_guard<mutex> lock(completionMutex);
            completed++;
        }
        completionCondition.notify_all();
    };

    thread forward(worker, first, second);
    thread reverse(worker, second, first);
    thread watchdog([&] {
        unique_lock<mutex> lock(completionMutex);
        if (!completionCondition.wait_for(lock, chrono::seconds(10), [&] { return completed == 2; })) {
            fputs("reverse MMKV imports timed out (possible AB/BA lock-order deadlock)\n", stderr);
            fflush(stderr);
            std::_Exit(124);
        }
    });

    start.store(true, memory_order_release);
    forward.join();
    reverse.join();
    watchdog.join();
    assert(first->getInt32("second-key") == 2);
    assert(second->getInt32("first-key") == 1);
}

int main() {
    auto testRoot = getenv("MMKV_TEST_ROOT");
    assert(testRoot && testRoot[0] != '\0');
    fs::path testRootPath(testRoot);
    fs::create_directories(testRootPath);
    auto rootTemplate = (testRootPath / "mmkv-import-validation-XXXXXX").string();
    vector<char> mutableTemplate(rootTemplate.begin(), rootTemplate.end());
    mutableTemplate.push_back('\0');
    auto rootCString = mkdtemp(mutableTemplate.data());
    assert(rootCString);
    fs::path root(rootCString);
    MMKV::initializeMMKV(root.string(), MMKVLogNone);

    auto source = MMKV::mmkvWithID("source");
    auto destination = MMKV::mmkvWithID("destination");
    assert(source && destination);

    const auto modifiedUtf8Nul = legacyModifiedUtf8NulKey();
    const string rawNul("account\0admin", 13);
    const vector<string> incompatibleKeys = {
        modifiedUtf8Nul,
        rawNul,
        string("invalid-lead-") + static_cast<char>(0xFF),
        string("stray-continuation-") + static_cast<char>(0x80),
        string("truncated-") + static_cast<char>(0xC2),
        string("bad-continuation-") + string("\xC2\x41", 2),
        string("overlong-three-") + string("\xE0\x80\x80", 3),
        string("canonical-four-") + string("\xF0\x9F\x98\x80", 4),
    };
    assert(!internal::CheckedImportAccess::isKeyCompatibleForAndroid(""));
    for (const auto &key : incompatibleKeys) {
        assert(!internal::CheckedImportAccess::isKeyCompatibleForAndroid(key));
        assertRejectedWithoutPartialImport(source, destination, key);
    }

    // The existing unchecked C++/Java import contract remains unchanged.
    source->clearAll();
    destination->clearAll();
    assert(source->set(int32_t{3}, "ordinary-source-key"));
    assert(source->set(int32_t{7}, modifiedUtf8Nul));
    assert(destination->importFrom(source) == 2);
    assert(destination->containsKey(modifiedUtf8Nul));

    source->clearAll();
    destination->clearAll();
    string modifiedUtf8EmojiKey = "emoji-";
    modifiedUtf8EmojiKey.append("\xED\xA0\xBD\xED\xB8\x80", 6);
    const vector<string> compatibleKeys = {
        "ordinary-source-key",
        string("copyright-") + string("\xC2\xA9", 2),
        string("han-") + string("\xE4\xB8\x96", 3),
        modifiedUtf8EmojiKey,
    };
    for (const auto &key : compatibleKeys) {
        assert(internal::CheckedImportAccess::isKeyCompatibleForAndroid(key));
        assert(source->set(int32_t{3}, key));
    }
    auto validResult = internal::CheckedImportAccess::importFrom(destination, source);
    assert(!validResult.incompatibleKeyRejected);
    assert(validResult.count == compatibleKeys.size());
    for (const auto &key : compatibleKeys) {
        assert(destination->getInt32(key) == 3);
    }

    // Key compatibility rejection is atomic, but write failures after that
    // preflight may leave a partial import. Only successful writes are counted.
    MMKVConfig limitedConfig;
    limitedConfig.itemSizeLimit = 64;
    auto limitedDestination = MMKV::mmkvWithID("limited-destination", limitedConfig);
    assert(limitedDestination);
    source->clearAll();
    limitedDestination->clearAll();
    assert(source->set(int32_t{9}, "small-item"));
    assert(source->set(string(512, 'x'), "oversized-item"));
    auto partialResult = internal::CheckedImportAccess::importFrom(limitedDestination, source);
    assert(!partialResult.incompatibleKeyRejected);
    assert(partialResult.count == 1);
    assert(limitedDestination->getInt32("small-item") == 9);
    assert(!limitedDestination->containsKey("oversized-item"));
    limitedDestination->clearAll();
    limitedDestination->close();

    source->clearAll();
    destination->clearAll();
    assert(source->set(int32_t{3}, "ordinary-source-key"));

    atomic<bool> stop{false};
    thread writer([&] {
        while (!stop.load(memory_order_relaxed)) {
            assert(source->set(int32_t{7}, modifiedUtf8Nul));
            this_thread::yield();
            assert(source->removeValueForKey(modifiedUtf8Nul));
            this_thread::yield();
        }
    });

    for (size_t iteration = 0; iteration < 2000; ++iteration) {
        destination->clearAll();
        auto result = internal::CheckedImportAccess::importFrom(destination, source);
        assert(!destination->containsKey(modifiedUtf8Nul));
        if (result.incompatibleKeyRejected) {
            assert(result.count == 0);
            assert(!destination->containsKey("ordinary-source-key"));
        } else {
            assert(result.count == 1);
            assert(destination->getInt32("ordinary-source-key") == 3);
        }
    }

    stop.store(true, memory_order_relaxed);
    writer.join();
    assertReverseImportsComplete(source, destination);
    source->clearAll();
    destination->clearAll();
    source->close();
    destination->close();
    MMKV::onExit();
    fs::remove_all(root);
}
