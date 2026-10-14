#pragma once

// Unit-test doubles only. This header is never included by the OpenHarmony library build.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum napi_status { napi_ok, napi_invalid_arg };
enum class TestValueType { Undefined, Number, ArrayBuffer, Handle, String, Bool };

struct TestValue {
    TestValueType type = TestValueType::Undefined;
    double number = 0;
    void *data = nullptr;
    size_t capacity = 0;
    uint64_t handle = 0;
    std::string text;
    bool boolean = false;
    napi_status status = napi_ok;
};

using napi_env = void *;
using napi_value = TestValue *;
struct TestCallbackInfo {
    std::vector<napi_value> args;
};
using napi_callback_info = TestCallbackInfo *;

napi_status napi_get_arraybuffer_info(napi_env, napi_value, void **, size_t *);
napi_status napi_get_value_double(napi_env, napi_value, double *);
napi_status napi_get_cb_info(napi_env, napi_callback_info, size_t *, napi_value *, napi_value *, void **);
napi_status napi_create_arraybuffer(napi_env, size_t, void **, napi_value *);
