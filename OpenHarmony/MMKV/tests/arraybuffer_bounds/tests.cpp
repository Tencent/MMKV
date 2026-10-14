#include "CheckedArrayBufferSlice.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>

static size_t checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    std::cerr << "Failed at line " << __LINE__ << ": " << #condition << '\n'; std::abort(); } } while (false)
#define NAPI_CALL(call) do { if ((call) != napi_ok) { return nullptr; } } while (false)

napi_status napi_get_arraybuffer_info(napi_env, napi_value value, void **data, size_t *length) {
    if (!value || value->type != TestValueType::ArrayBuffer || value->status != napi_ok) {
        return napi_invalid_arg;
    }
    *data = value->data;
    *length = value->capacity;
    return napi_ok;
}

napi_status napi_get_value_double(napi_env, napi_value value, double *number) {
    if (!value || value->type != TestValueType::Number || value->status != napi_ok) {
        return napi_invalid_arg;
    }
    *number = value->number;
    return napi_ok;
}

napi_status napi_get_cb_info(napi_env, napi_callback_info info, size_t *argc, napi_value *args, napi_value *, void **) {
    *argc = std::min(*argc, info->args.size());
    std::copy_n(info->args.begin(), *argc, args);
    return napi_ok;
}

napi_status napi_create_arraybuffer(napi_env, size_t length, void **data, napi_value *result) {
    static TestValue value;
    static std::vector<uint8_t> storage;
    storage.resize(length);
    value.type = TestValueType::ArrayBuffer;
    value.data = length == 0 ? nullptr : storage.data();
    value.capacity = length;
    *data = value.data;
    *result = &value;
    return napi_ok;
}

static TestValue number(double value) {
    TestValue result;
    result.type = TestValueType::Number;
    result.number = value;
    return result;
}

static napi_value BoolToNValue(napi_env, bool value) {
    static TestValue result;
    result.type = TestValueType::Bool;
    result.boolean = value;
    return &result;
}

static uint64_t NValueToUInt64(napi_env, napi_value value) {
    CHECK(value && value->type == TestValueType::Handle);
    return value->handle;
}

static std::string NValueToString(napi_env, napi_value value) {
    CHECK(value && value->type == TestValueType::String);
    return value->text;
}

static uint32_t NValueToUInt32(napi_env, napi_value value) {
    CHECK(value && value->type == TestValueType::Number);
    return static_cast<uint32_t>(value->number);
}

static bool IsNValueUndefined(napi_env, napi_value value) {
    CHECK(value != nullptr); // An omitted optional argument must not be inspected.
    return value->type == TestValueType::Undefined;
}

namespace mmkv {
enum MMBufferCopyFlag { MMBufferNoCopy };
class MMBuffer {
    uint8_t empty = 0;
public:
    void *data = &empty;
    size_t size = 0;
    MMBuffer() = default;
    MMBuffer(void *pointer, size_t length, MMBufferCopyFlag) : data(pointer), size(length) {}
    void *getPtr() const { return data; }
    size_t length() const { return size; }
};
} // namespace mmkv

// The store only models synchronous set dispatch and preservation of the previous value.
class MMKV {
    bool store(void *data, size_t size, bool expiring) {
        ++setCalls;
        usedExpiration = expiring;
        lastPointer = data;
        stored.resize(size);
        if (size > 0) {
            CHECK(data != nullptr);
            std::memcpy(stored.data(), data, size);
        }
        return true;
    }
public:
    size_t setCalls = 0;
    bool usedExpiration = false;
    void *lastPointer = nullptr;
    std::vector<uint8_t> stored;
    bool set(const mmkv::MMBuffer &value, const std::string &) { return store(value.data, value.length(), false); }
    bool set(const mmkv::MMBuffer &value, const std::string &, uint32_t) { return store(value.data, value.length(), true); }
    template <typename T> bool set(std::span<T> value, const std::string &) {
        return store(value.data(), value.size_bytes(), false);
    }
    template <typename T> bool set(std::span<T> value, const std::string &, uint32_t) {
        return store(value.data(), value.size_bytes(), true);
    }
};

using mmkv::MMBuffer;
#include "native_callbacks.inc"

using Encoder = napi_value (*)(napi_env, napi_callback_info);

static void exerciseEncoder(Encoder encode, size_t elementSize) {
    alignas(uint64_t) uint8_t data[24];
    for (size_t i = 0; i < sizeof(data); ++i) { data[i] = static_cast<uint8_t>(i + 1); }
    MMKV kv;
    TestValue handle;
    handle.type = TestValueType::Handle;
    handle.handle = reinterpret_cast<uintptr_t>(&kv);
    TestValue key;
    key.type = TestValueType::String;
    key.text = "preserved";
    TestValue buffer;
    buffer.type = TestValueType::ArrayBuffer;
    buffer.data = data;
    buffer.capacity = sizeof(data);
    auto offset = number(elementSize);
    auto length = number(elementSize * 2);
    auto expiration = number(17);
    TestValue undefined;
    TestCallbackInfo info{{&handle, &key, &buffer, &offset, &length}};
    auto call = [&]() { auto result = encode(nullptr, &info); CHECK(result); return result->boolean; };

    CHECK(call()); // Omitted expiration selects the ordinary overload.
    CHECK(!kv.usedExpiration);
    CHECK(kv.lastPointer == data + elementSize); // Still zero-copy up to synchronous set.
    CHECK(kv.stored == std::vector<uint8_t>(data + elementSize, data + elementSize * 3));
    info.args.push_back(&undefined);
    CHECK(call());
    CHECK(!kv.usedExpiration);
    info.args[5] = &expiration;
    CHECK(call());
    CHECK(kv.usedExpiration);
    info.args.resize(5);

    auto reject = [&]() {
        auto previous = kv.stored;
        auto callsBefore = kv.setCalls;
        CHECK(!call());
        CHECK(kv.setCalls == callsBefore);
        CHECK(kv.stored == previous);
    };
    for (double bad : {-1.0, 0.5, std::numeric_limits<double>::quiet_NaN(),
                       std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
                       4294967296.0, 4294967297.0, 4294967295.0}) {
        offset.number = bad;
        reject();
        offset.number = elementSize;
        length.number = bad;
        reject();
        length.number = elementSize * 2;
    }
    offset.number = sizeof(data) + 1;
    length.number = 0;
    reject();
    offset.number = sizeof(data);
    length.number = elementSize;
    reject();
    offset.number = sizeof(data) - elementSize;
    length.number = elementSize * 2;
    reject();
    offset.number = 0;
    length.number = elementSize;
    buffer.status = napi_invalid_arg;
    reject();
    buffer.status = napi_ok;
    buffer.type = TestValueType::String;
    reject();
    buffer.type = TestValueType::ArrayBuffer;
    buffer.data = nullptr;
    reject();
    buffer.data = data;
    offset.type = TestValueType::String;
    reject();
    offset.type = TestValueType::Number;
    length.status = napi_invalid_arg;
    reject();
    length.status = napi_ok;
    if (elementSize > 1) {
        length.number = elementSize + 1;
        reject();
        length.number = elementSize;
        offset.number = 1;
        reject();
        offset.number = 0;
        buffer.data = data + 1; // Offset alignment alone is insufficient.
        buffer.capacity = sizeof(data) - 1;
        reject();
        buffer.data = data;
        buffer.capacity = sizeof(data);
    }
    for (size_t argc = 0; argc < 5; ++argc) {
        info.args.resize(argc);
        reject();
        info.args = {&handle, &key, &buffer, &offset, &length};
    }
    // Exact end, interior zero-length, and null-backed empty buffers are accepted.
    length.number = 0;
    for (double emptyOffset : {0.0, 1.0, static_cast<double>(sizeof(data))}) {
        offset.number = emptyOffset;
        CHECK(call());
        CHECK(kv.stored.empty());
    }
    offset.number = 0;
    buffer.data = nullptr;
    buffer.capacity = 0;
    CHECK(call());
    CHECK(kv.stored.empty());
    length.number = 1;
    reject();
}

int main() {
    uint32_t parsed = 0;
    auto maximum = number(4294967295.0);
    CHECK(mmkv::ohos::NValueToSliceUInt32(nullptr, &maximum, parsed));
    CHECK(parsed == UINT32_MAX);
    auto negativeZero = number(-0.0);
    CHECK(mmkv::ohos::NValueToSliceUInt32(nullptr, &negativeZero, parsed));
    CHECK(parsed == 0);
    MMBuffer empty;
    auto emptyResult = MMBufferToNValue(nullptr, empty);
    CHECK(emptyResult && emptyResult->capacity == 0 && emptyResult->data == nullptr);
    uint8_t bytes[] = {1, 2, 3};
    MMBuffer nonempty(bytes, sizeof(bytes), mmkv::MMBufferNoCopy);
    auto byteResult = MMBufferToNValue(nullptr, nonempty);
    CHECK(byteResult && byteResult->capacity == sizeof(bytes));
    CHECK(std::memcmp(byteResult->data, bytes, sizeof(bytes)) == 0);
    exerciseEncoder(encodeBytes, 1);
    exerciseEncoder(encodeArray<int32_t>, sizeof(int32_t));
    exerciseEncoder(encodeArray<uint32_t>, sizeof(uint32_t));
    exerciseEncoder(encodeArray<int64_t>, sizeof(int64_t));
    exerciseEncoder(encodeArray<uint64_t>, sizeof(uint64_t));
    std::cout << checks << " checks passed (production callbacks/helper with N-API and MMKV test doubles).\n";
}
