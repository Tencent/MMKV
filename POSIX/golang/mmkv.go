/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2020 THL A29 Limited, a Tencent company.
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

package mmkv

// #cgo CXXFLAGS: -D CGO -D FORCE_POSIX -I ${SRCDIR}/../../Core -std=c++17
// #cgo LDFLAGS: -L. -lmmkv -L./Core -lcore -lz -lpthread
/*
#include "golang-bridge.h"
#include <stdlib.h>

typedef void* voidptr_t;

GoStringWrap wrapGoString(_GoString_ str) {
    GoStringWrap wrap = { _GoStringPtr(str), _GoStringLen(str) };
    return wrap;
}

static void setStringArray(char **array, char *str, size_t n) {
    array[n] = str;
}

static char *getStringArray(char **array, size_t n) {
    return array[n];
}

static void setSizeArray(uint32_t *array, uint32_t value, size_t n) {
    array[n] = value;
}

static uint32_t getSizeArray(uint32_t *array, size_t n) {
    return array[n];
}

static void freeStringArray(char **a, size_t size) {
    size_t i;
    for (i = 0; i < size; i++) {
        free(a[i]);
    }
}
*/
import "C"
import "unsafe"

const (
    MMKVLogDebug = iota // not available for release/product build
    MMKVLogInfo  // default level
    MMKVLogWarning
    MMKVLogError
    MMKVLogNone // special level used to disable all log messages
)

const (
    MMKV_SINGLE_PROCESS = 1 << iota
    MMKV_MULTI_PROCESS
)

const (
    OnErrorDiscard = iota
    OnErrorRecover
)

const (
    MMKVCRCCheckFail = iota
    MMKVFileLength
)

type ctorMMKV uintptr

type MMKV interface {
    SetBool(value bool, key string) bool
    SetInt32(value int32, key string) bool
    SetInt64(value int64, key string) bool
    SetUInt32(value uint32, key string) bool
    SetUInt64(value uint64, key string) bool
    SetFloat32(value float32, key string) bool
    SetFloat64(value float64, key string) bool
    SetString(value string, key string) bool
    SetBytes(value []byte, key string) bool

    GetBool(key string) bool
    GetBoolWithDefault(key string, defaultValue bool) bool
    GetInt32(key string) int32
    GetInt32WithDefault(key string, defaultValue int32) int32
    GetUInt32(key string) uint32
    GetUInt32WithDefault(key string, defaultValue uint32) uint32
    GetInt64(key string) int64
    GetInt64WithDefault(key string, defaultValue int64) int64
    GetUInt64(key string) uint64
    GetUInt64WithDefault(key string, defaultValue uint64) uint64
    GetFloat32(key string) float32
    GetFloat32WithDefault(key string, defaultValue float32) float32
    GetFloat64(key string) float64
    GetFloat64WithDefault(key string, defaultValue float64) float64
    GetString(key string) string
    GetBytes(key string) []byte

    RemoveKey(key string)
    RemoveKeys(keys []string)
    ClearAll()

    Count() uint64
    AllKeys() []string
    Contains(key string) bool
    TotalSize() uint64
    ActualSize() uint64

    MMAP_ID() string

    Sync(sync bool)
    ClearMemoryCache()
    Trim()
    Close()

    ReKey(newKey string) bool
    CryptKey() string
}

// Hello returns a greeting for the named person.
func Version() string {
    version := C.version()
    goStr := C.GoString(version)
    return goStr
}

func InitializeMMKV(rootDir string) {
    cRootDir := C.CString(rootDir)
    C.mmkvInitialize(cRootDir, MMKVLogInfo)
    C.free(unsafe.Pointer(cRootDir))
}

func InitializeMMKVWithLogLevel(rootDir string, logLevel int) {
    cRootDir := C.CString(rootDir)
    C.mmkvInitialize(cRootDir, C.int32_t(logLevel))
    C.free(unsafe.Pointer(cRootDir))
}

func OnExit() {
    C.onExit()
}

func PageSize() int32 {
    return int32(C.pageSize())
}

func DefaultMMKV() MMKV {
    mmkv := ctorMMKV(C.getDefaultMMKV(MMKV_SINGLE_PROCESS, nil))
    return MMKV(mmkv)
}

func DefaultMMKVWithMode(mode int) MMKV {
    mmkv := ctorMMKV(C.getDefaultMMKV(C.int(mode), nil))
    return MMKV(mmkv)
}

func DefaultMMKVWithModeAndCryptKey(mode int, cryptKey string) MMKV {
    cCryptKey := C.CString(cryptKey)
    mmkv := ctorMMKV(C.getDefaultMMKV(MMKV_SINGLE_PROCESS, cCryptKey))
    C.free(unsafe.Pointer(cCryptKey))
    return MMKV(mmkv)
}

func MMKVWithID(mmapID string) MMKV {
    cmmapID := C.CString(mmapID)
    mmkv := ctorMMKV(C.getMMKVWithID(cmmapID, MMKV_SINGLE_PROCESS, nil, nil))
    C.free(unsafe.Pointer(cmmapID))

    return MMKV(mmkv)
}

func MMKVWithIDAndMode(mmapID string, mode int) MMKV {
    cmmapID := C.CString(mmapID)
    mmkv := ctorMMKV(C.getMMKVWithID(cmmapID, C.int(mode), nil, nil))
    C.free(unsafe.Pointer(cmmapID))

    return MMKV(mmkv)
}

func MMKVWithIDAndModeAndCryptKey(mmapID string, mode int, cryptKey string) MMKV {
    cmmapID := C.CString(mmapID)
    cCryptKey := C.CString(cryptKey)

    mmkv := ctorMMKV(C.getMMKVWithID(cmmapID, C.int(mode), cCryptKey, nil))

    C.free(unsafe.Pointer(cmmapID))
    C.free(unsafe.Pointer(cCryptKey))

    return MMKV(mmkv)
}

// TODO: use _GoString_ to avoid string copying
func (kv ctorMMKV) SetBool(value bool, key string) bool {
    //cKey := C.CString(key)
    //ret := C.encodeBool(unsafe.Pointer(kv), cKey, C.bool(value))
    //C.free(unsafe.Pointer(cKey))
    cKey := C.wrapGoString(key)
    ret := C.encodeBool2(unsafe.Pointer(kv), cKey, C.bool(value))
    return bool(ret)
}

func (kv ctorMMKV) GetBool(key string) bool {
    return kv.GetBoolWithDefault(key, false)
}

func (kv ctorMMKV) GetBoolWithDefault(key string, defaultValue bool) bool {
    //cKey := C.CString(key)
    //value := C.decodeBool(unsafe.Pointer(kv), cKey, C.bool(defaultValue))
    //C.free(unsafe.Pointer(cKey))
    cKey := C.wrapGoString(key)
    value := C.decodeBool2(unsafe.Pointer(kv), cKey, C.bool(defaultValue))
    return bool(value)
}

func (kv ctorMMKV) SetInt32(value int32, key string) bool {
    cKey := C.CString(key)
    ret := C.encodeInt32(unsafe.Pointer(kv), cKey, C.int32_t(value))
    C.free(unsafe.Pointer(cKey))
    return bool(ret)
}

func (kv ctorMMKV) GetInt32(key string) int32 {
    return kv.GetInt32WithDefault(key, 0)
}

func (kv ctorMMKV) GetInt32WithDefault(key string, defaultValue int32) int32 {
    cKey := C.CString(key)
    value := C.decodeInt32(unsafe.Pointer(kv), cKey, C.int32_t(defaultValue))
    C.free(unsafe.Pointer(cKey))
    return int32(value)
}

func (kv ctorMMKV) SetUInt32(value uint32, key string) bool {
    cKey := C.CString(key)
    ret := C.encodeUInt32(unsafe.Pointer(kv), cKey, C.uint32_t(value))
    C.free(unsafe.Pointer(cKey))
    return bool(ret)
}

func (kv ctorMMKV) GetUInt32(key string) uint32 {
    return kv.GetUInt32WithDefault(key, 0)
}

func (kv ctorMMKV) GetUInt32WithDefault(key string, defaultValue uint32) uint32 {
    cKey := C.CString(key)
    value := C.decodeUInt32(unsafe.Pointer(kv), cKey, C.uint32_t(defaultValue))
    C.free(unsafe.Pointer(cKey))
    return uint32(value)
}

func (kv ctorMMKV) SetInt64(value int64, key string) bool {
    cKey := C.CString(key)
    ret := C.encodeInt64(unsafe.Pointer(kv), cKey, C.int64_t(value))
    C.free(unsafe.Pointer(cKey))
    return bool(ret)
}

func (kv ctorMMKV) GetInt64(key string) int64 {
    return kv.GetInt64WithDefault(key, 0)
}

func (kv ctorMMKV) GetInt64WithDefault(key string, defaultValue int64) int64 {
    cKey := C.CString(key)
    value := C.decodeInt64(unsafe.Pointer(kv), cKey, C.int64_t(defaultValue))
    C.free(unsafe.Pointer(cKey))
    return int64(value)
}

func (kv ctorMMKV) SetUInt64(value uint64, key string) bool {
    cKey := C.CString(key)
    ret := C.encodeUInt64(unsafe.Pointer(kv), cKey, C.uint64_t(value))
    C.free(unsafe.Pointer(cKey))
    return bool(ret)
}

func (kv ctorMMKV) GetUInt64(key string) uint64 {
    return kv.GetUInt64WithDefault(key, 0)
}

func (kv ctorMMKV) GetUInt64WithDefault(key string, defaultValue uint64) uint64 {
    cKey := C.CString(key)
    value := C.decodeUInt64(unsafe.Pointer(kv), cKey, C.uint64_t(defaultValue))
    C.free(unsafe.Pointer(cKey))
    return uint64(value)
}

func (kv ctorMMKV) SetFloat32(value float32, key string) bool {
    cKey := C.CString(key)
    ret := C.encodeFloat(unsafe.Pointer(kv), cKey, C.float(value))
    C.free(unsafe.Pointer(cKey))
    return bool(ret)
}

func (kv ctorMMKV) GetFloat32(key string) float32 {
    return kv.GetFloat32WithDefault(key, 0)
}

func (kv ctorMMKV) GetFloat32WithDefault(key string, defaultValue float32) float32 {
    cKey := C.CString(key)
    value := C.decodeFloat(unsafe.Pointer(kv), cKey, C.float(defaultValue))
    C.free(unsafe.Pointer(cKey))
    return float32(value)
}

func (kv ctorMMKV) SetFloat64(value float64, key string) bool {
    cKey := C.CString(key)
    ret := C.encodeDouble(unsafe.Pointer(kv), cKey, C.double(value))
    C.free(unsafe.Pointer(cKey))
    return bool(ret)
}

func (kv ctorMMKV) GetFloat64(key string) float64 {
    return kv.GetFloat64WithDefault(key, 0)
}

func (kv ctorMMKV) GetFloat64WithDefault(key string, defaultValue float64) float64 {
    cKey := C.CString(key)
    value := C.decodeDouble(unsafe.Pointer(kv), cKey, C.double(defaultValue))
    C.free(unsafe.Pointer(cKey))
    return float64(value)
}

func (kv ctorMMKV) SetString(value string, key string) bool {
    cKey := C.CString(key)
    cValue := C.CString(value)

    ret := C.encodeBytes(unsafe.Pointer(kv), cKey, unsafe.Pointer(cValue), C.uint64_t(len(value)));

    C.free(unsafe.Pointer(cKey))
    C.free(unsafe.Pointer(cValue))
    return bool(ret)
}

func (kv ctorMMKV) GetString(key string) string {
    cKey := C.CString(key)
    var length uint64

    cValue := C.decodeBytes(unsafe.Pointer(kv), cKey, (*C.uint64_t)(&length))
    value := C.GoString((*C.char)(cValue))

    C.free(unsafe.Pointer(cKey))
    C.free(unsafe.Pointer(cValue))
    return value
}

func (kv ctorMMKV) SetBytes(value []byte, key string) bool {
    cKey := C.CString(key)
    cValue := C.CBytes(value)

    ret := C.encodeBytes(unsafe.Pointer(kv), cKey, unsafe.Pointer(cValue), C.uint64_t(len(value)))

    C.free(unsafe.Pointer(cKey))
    C.free(unsafe.Pointer(cValue))
    return bool(ret)
}

func (kv ctorMMKV) GetBytes(key string) []byte {
    cKey := C.CString(key)
    var length uint64

    cValue := C.decodeBytes(unsafe.Pointer(kv), cKey, (*C.uint64_t)(&length))
    value := C.GoBytes(unsafe.Pointer(cValue), C.int(length))

    C.free(unsafe.Pointer(cKey))
    C.free(unsafe.Pointer(cValue))
    return value
}

func (kv ctorMMKV) RemoveKey(key string) {
    cKey := C.CString(key)
    C.removeValueForKey(unsafe.Pointer(kv), cKey);
    C.free(unsafe.Pointer(cKey))
}

func (kv ctorMMKV) RemoveKeys(keys []string) {
    keyArray := (**C.char)(C.calloc(C.size_t(len(keys)), C.sizeof_voidptr_t))
    sizeArray := (*C.uint32_t)(C.calloc(C.size_t(len(keys)), C.sizeof_voidptr_t))

    for index, key := range keys {
        C.setStringArray(keyArray, C.CString(key), C.size_t(index))
        C.setSizeArray(sizeArray, C.uint32_t(len(key)), C.size_t(index))
    }
    C.removeValuesForKeys(unsafe.Pointer(kv), keyArray, sizeArray, C.uint64_t(len(keys)))

    C.freeStringArray(keyArray, C.size_t(len(keys)))
    C.free(unsafe.Pointer(keyArray))
    C.free(unsafe.Pointer(sizeArray))
}

func (kv ctorMMKV) Count() uint64 {
    return uint64(C.count(unsafe.Pointer(kv)))
}

func (kv ctorMMKV) AllKeys() []string {
    var keyArray **C.char
    var sizeArray *C.uint32_t

    cCount := C.allKeys(unsafe.Pointer(kv), &keyArray, &sizeArray)
    count := uint64(cCount)
    if count == 0 {
        return []string{}
    }

    result := make([]string, count)
    for index := uint64(0); index < count; index++ {
        cStr := C.getStringArray(keyArray, C.size_t(index))
        cLen := C.getSizeArray(sizeArray, C.size_t(index))
        result[index] = C.GoStringN(cStr, C.int(cLen))
    }

    C.freeStringArray(keyArray, C.size_t(cCount))
    C.free(unsafe.Pointer(keyArray))
    C.free(unsafe.Pointer(sizeArray))
    return result
}

func (kv ctorMMKV) Contains(key string) bool {
    cKey := C.CString(key)
    ret := C.containsKey(unsafe.Pointer(kv), cKey)
    C.free(unsafe.Pointer(cKey))
    return bool(ret)
}

func (kv ctorMMKV) ClearAll() {
    C.clearAll(unsafe.Pointer(kv))
}

func (kv ctorMMKV) TotalSize() uint64 {
    return uint64(C.totalSize(unsafe.Pointer(kv)))
}

func (kv ctorMMKV) ActualSize() uint64 {
    return uint64(C.actualSize(unsafe.Pointer(kv)))
}

func (kv ctorMMKV) MMAP_ID() string {
    cStr := C.mmapID(unsafe.Pointer(kv))
    return C.GoString(cStr)
}

func (kv ctorMMKV) Sync(sync bool) {
    C.mmkvSync(unsafe.Pointer(kv), C.bool(sync))
}

func (kv ctorMMKV) ClearMemoryCache() {
    C.clearMemoryCache(unsafe.Pointer(kv))
}

func (kv ctorMMKV) Trim() {
    C.trim(unsafe.Pointer(kv))
}

func (kv ctorMMKV) Close() {
    C.mmkvClose(unsafe.Pointer(kv))
}

func (kv ctorMMKV) ReKey(newKey string) bool {
    cKey := C.CString(newKey)
    ret := C.reKey(unsafe.Pointer(kv), cKey, C.uint32_t(len(newKey)))
    C.free(unsafe.Pointer(cKey))
    return bool(ret)
}

func (kv ctorMMKV) CryptKey() string {
    var cLen C.uint32_t
    cStr := C.cryptKey(unsafe.Pointer(kv), &cLen)
    if cStr == nil || cLen == 0 {
        return ""
    }
    result := C.GoStringN((*C.char)(cStr), C.int(cLen))
    C.free(unsafe.Pointer(cStr))
    return result
}
