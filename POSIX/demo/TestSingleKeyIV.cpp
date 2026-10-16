// Exercise single-key overwrites through the public API and inspect the
// persisted IV/ciphertext, rather than testing an isolated encryption helper.
#include "MMKV.h"
#include "MMBuffer.h"
#include "MMKVMetaInfo.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>

using namespace mmkv;

static void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

static bool passed = true;
static void check(bool condition, const char *message) {
    if (!condition) {
        std::fprintf(stderr, "test single-key IV: %s\n", message);
        passed = false;
    }
}

static constexpr const char *storageIDs[] = {
    "aes128", "aes256", "growth", "expiry", "plain", "borrowed", "transitions", "compaction"};

struct OwnedDirectory {
    std::string path;
    explicit OwnedDirectory(const char *base) {
        path = std::string(base && *base ? base : "/tmp") + "/mmkv-single-key-iv.XXXXXX";
        require(mkdtemp(path.data()) != nullptr, "cannot create owned scratch directory");
    }
    ~OwnedDirectory() {
        MMKV::onExit();
        // Only these files inside the directory created by this invocation
        // belong to the test. Never clean pre-existing temp directories.
        for (const char *id : storageIDs) {
            unlink((path + "/" + id).c_str());
            unlink((path + "/" + id + ".crc").c_str());
        }
        if (rmdir(path.c_str()) != 0) {
            std::fprintf(stderr, "test single-key IV: cannot remove owned directory %s\n", path.c_str());
            passed = false;
        }
    }
};

static std::vector<uint8_t> readFile(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    require(input.is_open(), "cannot open snapshot file");
    std::vector<uint8_t> result((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    require(!input.bad(), "cannot read snapshot file");
    return result;
}

struct Snapshot {
    std::array<uint8_t, AES_IV_LEN> iv;
    std::vector<uint8_t> ciphertext;
};

static Snapshot snapshot(MMKV *kv, const std::string &root) {
    kv->sync();
    auto metaBytes = readFile(root + "/" + kv->mmapID() + ".crc");
    require(metaBytes.size() >= sizeof(MMKVMetaInfo), "truncated metadata snapshot");
    MMKVMetaInfo meta;
    std::memcpy(&meta, metaBytes.data(), sizeof(meta));
    auto data = readFile(root + "/" + kv->mmapID());
    require(meta.m_actualSize == kv->actualSize(), "persisted actual size does not match instance");
    require(data.size() >= sizeof(uint32_t) + meta.m_actualSize, "truncated data snapshot");
    data.resize(sizeof(uint32_t) + meta.m_actualSize);
    Snapshot result;
    std::memcpy(result.iv.data(), meta.m_vector, result.iv.size());
    result.ciphertext = std::move(data);
    return result;
}

static MMKV *openKV(const char *id, const MMKVConfig &config) {
    auto kv = MMKV::mmkvWithID(id, config);
    require(kv != nullptr, "cannot open storage");
    return kv;
}

static void verifyString(MMKV *kv, const std::string &expected) {
    std::string actual;
    require(kv->getString("k", actual) && actual == expected, "decoded value differs after overwrite");
    require(kv->count() == 1, "overwrite did not preserve a single key");
}

static MMKV *reloadAndReopen(MMKV *kv, const char *id, const MMKVConfig &config, const std::string &expected) {
    verifyString(kv, expected);
    kv->clearMemoryCache();
    verifyString(kv, expected);
    require(MMKV::isFileValid(id), "persisted overwrite fails CRC validation");
    kv->close();
    kv = openKV(id, config);
    verifyString(kv, expected);
    return kv;
}

static bool preservesCiphertextAndAppends(const Snapshot &before, const Snapshot &after) {
    // The raw four-byte size header changes as entries are appended. Every
    // previously active encrypted byte must retain its position and content.
    return after.ciphertext.size() > before.ciphertext.size() &&
           std::equal(before.ciphertext.begin() + sizeof(uint32_t), before.ciphertext.end(),
                      after.ciphertext.begin() + sizeof(uint32_t));
}

static void checkEncryptedReplacement(const Snapshot &before, const Snapshot &after) {
    check(after.iv != before.iv || preservesCiphertextAndAppends(before, after),
          "encrypted replacement rewrote existing ciphertext without changing the IV");
}

static void securityRegression(const Snapshot &first, const Snapshot &second) {
    if (first.iv == second.iv) {
        // Historical entries remain when replacement appends. Their identical
        // prefix is safe only if the new entry starts beyond the previous end.
        check(preservesCiphertextAndAppends(first, second),
              "same-IV replacement did not preserve ciphertext and append beyond its active end");
        return;
    }
    // Disk offsets 0..3 hold the unencrypted size; 4..7 are the random
    // MiniPB size holder. With key "k" and a ten-byte string, 8..11 are
    // fixed framing and 12..19 are eight value bytes in the first AES block.
    // CFB's first-block keystream depends only on the initial IV and key.
    require(first.ciphertext.size() >= 20 && second.ciphertext.size() >= 20,
            "snapshot too short for known-plaintext regression");
    bool sameFraming = true;
    bool recoveredValue = true;
    for (size_t i = 8; i < 12; ++i) {
        sameFraming &= first.ciphertext[i] == second.ciphertext[i];
    }
    for (size_t i = 12; i < 20; ++i) {
        recoveredValue &= uint8_t('A' ^ first.ciphertext[i] ^ second.ciphertext[i]) == uint8_t('B');
    }
    check(!sameFraming, "fixed framing ciphertext is identical across overwrites");
    check(!recoveredValue, "known plaintext recovers all eight bytes of the replacement value");
}

static void overwriteSizes(const std::string &root, const char *id, const MMKVConfig &config,
                           bool encrypted, bool inspectLeak = false) {
    auto kv = openKV(id, config);
    // Preallocated storage lets replacements cross direct, heap-backed, and
    // offset-backed holder sizes without requiring capacity expansion.
    require(kv->set(std::string(10, 'Z'), "k"), "initial write failed");
    auto previous = snapshot(kv, root);
    Snapshot knownPlaintext;
    const std::vector<std::string> values = {
        std::string(10, 'A'), std::string(10, 'B'), std::string(100, 'C'),
        std::string(300, 'D'), std::string(32768, 'E'), std::string(3, 'F'), std::string(300, 'G')};
    for (size_t i = 0; i < values.size(); ++i) {
        require(kv->set(values[i], "k"), "overwrite failed");
        auto current = snapshot(kv, root);
        if (encrypted) {
            checkEncryptedReplacement(previous, current);
        }
        if (inspectLeak && i == 0) {
            knownPlaintext = current;
        } else if (inspectLeak && i == 1) {
            securityRegression(knownPlaintext, current);
        }
        kv = reloadAndReopen(kv, id, config, values[i]);
        previous = std::move(current);
    }
    kv->close();
}

static void capacityGrowth(const std::string &root, const MMKVConfig &config) {
    auto kv = openKV("growth", config);
    require(kv->set(std::string(10, 'A'), "k"), "initial growth write failed");
    auto initialCapacity = kv->totalSize();
    const std::string large(initialCapacity * 4, 'B');
    // The first oversized replacement expands storage; the next replacement
    // exercises its existing append/compaction lifecycle at the larger size.
    require(kv->set(large, "k"), "capacity expansion failed");
    check(kv->totalSize() > initialCapacity, "growth scenario did not expand the file");
    auto before = snapshot(kv, root);
    kv = reloadAndReopen(kv, "growth", config, large);
    const std::string replacement(large.size(), 'C');
    require(kv->set(replacement, "k"), "overwrite after expansion failed");
    auto after = snapshot(kv, root);
    checkEncryptedReplacement(before, after);
    kv = reloadAndReopen(kv, "growth", config, replacement);
    kv->close();
}

static void borrowedInput(const std::string &root, const MMKVConfig &config) {
    auto kv = openKV("borrowed", config);
    require(kv->set(std::string(100, 'A'), "k"), "initial borrowed-input write failed");
    auto before = snapshot(kv, root);
    std::string source(100, 'B');
    MMBuffer borrowed(source.data(), source.size(), MMBufferNoCopy);
    require(kv->set(borrowed, "k"), "borrowed-input overwrite failed");
    auto after = snapshot(kv, root);
    checkEncryptedReplacement(before, after);
    // Replacing the caller's bytes must not change the cached value.
    source.assign(source.size(), 'X');
    kv = reloadAndReopen(kv, "borrowed", config, std::string(100, 'B'));
    kv->close();
}

static void dictionaryTransitions(const std::string &root, const MMKVConfig &config) {
    auto kv = openKV("transitions", config);
    require(kv->set(std::string(300, 'A'), "k"), "initial transition write failed");
    auto initial = snapshot(kv, root);
    require(kv->set(std::string(300, 'B'), "k"), "transition overwrite failed");
    auto overwritten = snapshot(kv, root);
    checkEncryptedReplacement(initial, overwritten);
    // Appending after a replacement must continue from the current crypter state,
    // while reads of the offset-backed first value retain its saved status.
    require(kv->set(std::string(300, 'C'), "other"), "append after overwrite failed");
    auto verifyTwoKeys = [](MMKV *instance) {
        std::string first, second;
        require(instance->getString("k", first) && first == std::string(300, 'B'),
                "first value corrupted by append after overwrite");
        require(instance->getString("other", second) && second == std::string(300, 'C'),
                "appended value does not decode with new IV");
        require(instance->count() == 2, "append did not retain both keys");
    };
    auto appended = snapshot(kv, root);
    check(appended.iv == overwritten.iv, "ordinary append unexpectedly changed the IV");
    verifyTwoKeys(kv);
    kv->clearMemoryCache();
    verifyTwoKeys(kv);
    require(MMKV::isFileValid("transitions"), "append after overwrite fails CRC validation");
    kv->close();
    kv = openKV("transitions", config);
    verifyTwoKeys(kv);

    require(kv->removeValueForKey("other") && kv->removeValueForKey("k"), "cannot remove last keys");
    require(kv->count() == 0 && kv->actualSize() > 0, "empty dictionary lacks retained log for override");
    auto removed = snapshot(kv, root);
    require(kv->set(std::string(300, 'D'), "k"), "insert after last-key removal failed");
    auto inserted = snapshot(kv, root);
    check(inserted.iv != removed.iv, "insert into emptied dictionary reused the persisted IV");
    kv = reloadAndReopen(kv, "transitions", config, std::string(300, 'D'));
    kv->close();
}

static void forcedCompaction(const std::string &root, MMKVConfig config) {
    config.expectedCapacity = 0;
    auto kv = openKV("compaction", config);
    require(kv->set(std::string(300, 'A'), "k"), "initial compaction write failed");
    const size_t valueSize = kv->totalSize() / 10;
    auto previous = snapshot(kv, root);
    bool rotatedIV = false;
    std::string value;
    // Roughly ten entries fill the initial mapping. Repeated replacements
    // must compact old entries and persist a fresh IV at least once.
    for (size_t i = 0; i < 32; ++i) {
        value.assign(valueSize, char('B' + i % 24));
        require(kv->set(value, "k"), "compaction replacement failed");
        auto current = snapshot(kv, root);
        checkEncryptedReplacement(previous, current);
        verifyString(kv, value);
        if (current.iv != previous.iv) {
            rotatedIV = true;
            kv = reloadAndReopen(kv, "compaction", config, value);
        }
        previous = std::move(current);
    }
    check(rotatedIV, "repeated replacements did not exercise compaction with a fresh IV");
    kv = reloadAndReopen(kv, "compaction", config, value);
    kv->close();
}

int main(int argc, char **argv) {
    try {
        require(argc <= 2, "usage: TestSingleKeyIV [scratch-directory]");
        {
            OwnedDirectory directory(argc == 2 ? argv[1] : std::getenv("TMPDIR"));
            MMKV::initializeMMKV(directory.path, MMKVLogNone);
            MMKVConfig config;
            config.expectedCapacity = 128 * 1024;
#ifndef MMKV_DISABLE_CRYPT
            const std::string key128 = "0123456789abcdef";
            config.cryptKey = &key128;
            overwriteSizes(directory.path, "aes128", config, true, true);
            const std::string key256 = "0123456789abcdef0123456789abcdef";
            config.cryptKey = &key256;
            config.aes256 = true;
            overwriteSizes(directory.path, "aes256", config, true, true);
            config.cryptKey = &key128;
            config.aes256 = false;
            borrowedInput(directory.path, config);
            dictionaryTransitions(directory.path, config);
            forcedCompaction(directory.path, config);
            config.enableKeyExpire = true;
            config.expiredInSeconds = 3600;
            overwriteSizes(directory.path, "expiry", config, true);
            config.enableKeyExpire = std::nullopt;
            config.expiredInSeconds = 0;
            config.expectedCapacity = 0;
            capacityGrowth(directory.path, config);
            config.cryptKey = nullptr;
#endif
            config.expectedCapacity = 128 * 1024;
            overwriteSizes(directory.path, "plain", config, false);
        }
        if (passed) {
            std::puts("test single-key IV: safe replacements, compaction IVs, reload/reopen, sizes and plain control passed");
        }
        return passed ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "test single-key IV: %s\n", error.what());
        return EXIT_FAILURE;
    }
}
