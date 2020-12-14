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

// MMKV is a cross-platform key-value storage framework developed by WeChat.
package mmkv

// #cgo CXXFLAGS: -D CGO -D FORCE_POSIX -I ${SRCDIR}/../../Core -std=c++17
// #cgo LDFLAGS: -L. -lmmkv -L./Core -lcore -lz -lpthread
/*
#include "golang-bridge.h"
#include <stdlib.h>

typedef void* voidptr_t;

static GoStringWrap_t wrapGoString(_GoString_ str) {
    GoStringWrap_t wrap = { _GoStringPtr(str), _GoStringLen(str) };
    return wrap;
}

static GoStringWrap_t GoStringWrapNil() {
	GoStringWrap_t result = { 0, 0 };
	return result;
}

static GoStringWrap_t wrapGoByteSlice(const void *ptr, size_t len) {
    GoStringWrap_t wrap = { ptr, len };
    return wrap;
}

static void freeStringArray(GoStringWrap_t *a, size_t size) {
    for (size_t i = 0; i < size; i++) {
        free((void*) a[i].ptr);
    }
}
*/
import "C"
import "unsafe"

const (
	MMKVLogDebug = iota // not available for release/product build
	MMKVLogInfo         // default level
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
	GetBool(key string) bool
	GetBoolWithDefault(key string, defaultValue bool) bool

	SetInt32(value int32, key string) bool
	GetInt32(key string) int32
	GetInt32WithDefault(key string, defaultValue int32) int32

	SetInt64(value int64, key string) bool
	GetInt64(key string) int64
	GetInt64WithDefault(key string, defaultValue int64) int64

	SetUInt32(value uint32, key string) bool
	GetUInt32(key string) uint32
	GetUInt32WithDefault(key string, defaultValue uint32) uint32

	SetUInt64(value uint64, key string) bool
	GetUInt64(key string) uint64
	GetUInt64WithDefault(key string, defaultValue uint64) uint64

	SetFloat32(value float32, key string) bool
	GetFloat32(key string) float32
	GetFloat32WithDefault(key string, defaultValue float32) float32

	SetFloat64(value float64, key string) bool
	GetFloat64(key string) float64
	GetFloat64WithDefault(key string, defaultValue float64) float64

	// string value should be utf-8 encoded
	SetString(value string, key string) bool
	GetString(key string) string

	SetBytes(value []byte, key string) bool
	GetBytes(key string) []byte

	RemoveKey(key string)
	RemoveKeys(keys []string)

	// clear all key-values
	ClearAll()

	// return count of keys
	Count() uint64

	AllKeys() []string
	Contains(key string) bool

	// total size of the file
	TotalSize() uint64

	// actual used size of the file
	ActualSize() uint64

	// the mmapID of the instance
	MMAP_ID() string

	/* Synchronize memory to file. You don't need to call this, really, I mean it. Unless you worry about running out of battery.
	 * Pass true to perform synchronous write.
	 * Pass false to perform asynchronous write, return immediately.
	 */
	Sync(sync bool)
	// Clear all caches (on memory warning).
	ClearMemoryCache()

	/* Trim the file size to minimal.
	 * MMKV's size won't reduce after deleting key-values.
	 * Call this method after lots of deleting if you care about disk usage.
	 * Note that clearAll() has the similar effect.
	 */
	Trim()

	// Close the instance when it's no longer needed in the near future.
	// Any subsequent call to the instance is undefined behavior.
	Close()

	/* Change encryption key for the MMKV instance.
	 * The cryptKey is 16 bytes limited.
	 * You can transfer a plain-text MMKV into encrypted by setting an non-null, non-empty cryptKey.
	 * Or vice versa by passing cryptKey with null. See also checkReSetCryptKey().
	 */
	ReKey(newKey string) bool

	// Just reset the cryptKey (will not encrypt or decrypt anything).
	// Usually you should call this method after other process reKey() the multi-process mmkv.
	CheckReSetCryptKey(key string)

	// See also reKey().
	CryptKey() string
}

// Hello returns a greeting for the named person.
func Version() string {
	version := C.version()
	goStr := C.GoString(version)
	return goStr
}

func InitializeMMKV(rootDir string) {
	C.mmkvInitialize(C.wrapGoString(rootDir), MMKVLogInfo)
}

func InitializeMMKVWithLogLevel(rootDir string, logLevel int) {
	C.mmkvInitialize(C.wrapGoString(rootDir), C.int32_t(logLevel))
}

func OnExit() {
	C.onExit()
}

func PageSize() int32 {
	return int32(C.pageSize())
}

func DefaultMMKV() MMKV {
	mmkv := ctorMMKV(C.getDefaultMMKV(MMKV_SINGLE_PROCESS, C.GoStringWrapNil()))
	return MMKV(mmkv)
}

func DefaultMMKVWithMode(mode int) MMKV {
	mmkv := ctorMMKV(C.getDefaultMMKV(C.int(mode), C.GoStringWrapNil()))
	return MMKV(mmkv)
}

func DefaultMMKVWithModeAndCryptKey(mode int, cryptKey string) MMKV {
	mmkv := ctorMMKV(C.getDefaultMMKV(MMKV_SINGLE_PROCESS, C.wrapGoString(cryptKey)))
	return MMKV(mmkv)
}

func MMKVWithID(mmapID string) MMKV {
	cStrNull := C.GoStringWrapNil()
	mmkv := ctorMMKV(C.getMMKVWithID(C.wrapGoString(mmapID), MMKV_SINGLE_PROCESS, cStrNull, cStrNull))
	return MMKV(mmkv)
}

func MMKVWithIDAndMode(mmapID string, mode int) MMKV {
	cStrNull := C.GoStringWrapNil()
	mmkv := ctorMMKV(C.getMMKVWithID(C.wrapGoString(mmapID), C.int(mode), cStrNull, cStrNull))
	return MMKV(mmkv)
}

func MMKVWithIDAndModeAndCryptKey(mmapID string, mode int, cryptKey string) MMKV {
	cStrNull := C.GoStringWrapNil()
	mmkv := ctorMMKV(C.getMMKVWithID(C.wrapGoString(mmapID), C.int(mode), C.wrapGoString(cryptKey), cStrNull))
	return MMKV(mmkv)
}

func (kv ctorMMKV) SetBool(value bool, key string) bool {
	ret := C.encodeBool(unsafe.Pointer(kv), C.wrapGoString(key), C.bool(value))
	return bool(ret)
}

func (kv ctorMMKV) GetBool(key string) bool {
	return kv.GetBoolWithDefault(key, false)
}

func (kv ctorMMKV) GetBoolWithDefault(key string, defaultValue bool) bool {
	value := C.decodeBool(unsafe.Pointer(kv), C.wrapGoString(key), C.bool(defaultValue))
	return bool(value)
}

func (kv ctorMMKV) SetInt32(value int32, key string) bool {
	ret := C.encodeInt32(unsafe.Pointer(kv), C.wrapGoString(key), C.int32_t(value))
	return bool(ret)
}

func (kv ctorMMKV) GetInt32(key string) int32 {
	return kv.GetInt32WithDefault(key, 0)
}

func (kv ctorMMKV) GetInt32WithDefault(key string, defaultValue int32) int32 {
	value := C.decodeInt32(unsafe.Pointer(kv), C.wrapGoString(key), C.int32_t(defaultValue))
	return int32(value)
}

func (kv ctorMMKV) SetUInt32(value uint32, key string) bool {
	ret := C.encodeUInt32(unsafe.Pointer(kv), C.wrapGoString(key), C.uint32_t(value))
	return bool(ret)
}

func (kv ctorMMKV) GetUInt32(key string) uint32 {
	return kv.GetUInt32WithDefault(key, 0)
}

func (kv ctorMMKV) GetUInt32WithDefault(key string, defaultValue uint32) uint32 {
	value := C.decodeUInt32(unsafe.Pointer(kv), C.wrapGoString(key), C.uint32_t(defaultValue))
	return uint32(value)
}

func (kv ctorMMKV) SetInt64(value int64, key string) bool {
	ret := C.encodeInt64(unsafe.Pointer(kv), C.wrapGoString(key), C.int64_t(value))
	return bool(ret)
}

func (kv ctorMMKV) GetInt64(key string) int64 {
	return kv.GetInt64WithDefault(key, 0)
}

func (kv ctorMMKV) GetInt64WithDefault(key string, defaultValue int64) int64 {
	value := C.decodeInt64(unsafe.Pointer(kv), C.wrapGoString(key), C.int64_t(defaultValue))
	return int64(value)
}

func (kv ctorMMKV) SetUInt64(value uint64, key string) bool {
	ret := C.encodeUInt64(unsafe.Pointer(kv), C.wrapGoString(key), C.uint64_t(value))
	return bool(ret)
}

func (kv ctorMMKV) GetUInt64(key string) uint64 {
	return kv.GetUInt64WithDefault(key, 0)
}

func (kv ctorMMKV) GetUInt64WithDefault(key string, defaultValue uint64) uint64 {
	value := C.decodeUInt64(unsafe.Pointer(kv), C.wrapGoString(key), C.uint64_t(defaultValue))
	return uint64(value)
}

func (kv ctorMMKV) SetFloat32(value float32, key string) bool {
	ret := C.encodeFloat(unsafe.Pointer(kv), C.wrapGoString(key), C.float(value))
	return bool(ret)
}

func (kv ctorMMKV) GetFloat32(key string) float32 {
	return kv.GetFloat32WithDefault(key, 0)
}

func (kv ctorMMKV) GetFloat32WithDefault(key string, defaultValue float32) float32 {
	value := C.decodeFloat(unsafe.Pointer(kv), C.wrapGoString(key), C.float(defaultValue))
	return float32(value)
}

func (kv ctorMMKV) SetFloat64(value float64, key string) bool {
	ret := C.encodeDouble(unsafe.Pointer(kv), C.wrapGoString(key), C.double(value))
	return bool(ret)
}

func (kv ctorMMKV) GetFloat64(key string) float64 {
	return kv.GetFloat64WithDefault(key, 0)
}

func (kv ctorMMKV) GetFloat64WithDefault(key string, defaultValue float64) float64 {
	value := C.decodeDouble(unsafe.Pointer(kv), C.wrapGoString(key), C.double(defaultValue))
	return float64(value)
}

func (kv ctorMMKV) SetString(value string, key string) bool {
	cValue := C.wrapGoString(value)
	ret := C.encodeBytes(unsafe.Pointer(kv), C.wrapGoString(key), cValue)
	return bool(ret)
}

func (kv ctorMMKV) GetString(key string) string {
	var length uint64

	cValue := C.decodeBytes(unsafe.Pointer(kv), C.wrapGoString(key), (*C.uint64_t)(&length))
	value := C.GoStringN((*C.char)(cValue), C.int(length))

	C.free(unsafe.Pointer(cValue))
	return value
}

func (kv ctorMMKV) SetBytes(value []byte, key string) bool {
	cValue := C.wrapGoByteSlice(unsafe.Pointer(&value[0]), C.size_t(len(value)))
	ret := C.encodeBytes(unsafe.Pointer(kv), C.wrapGoString(key), cValue)
	return bool(ret)
}

func (kv ctorMMKV) GetBytes(key string) []byte {
	var length uint64

	cValue := C.decodeBytes(unsafe.Pointer(kv), C.wrapGoString(key), (*C.uint64_t)(&length))
	value := C.GoBytes(unsafe.Pointer(cValue), C.int(length))

	C.free(unsafe.Pointer(cValue))
	return value
}

func (kv ctorMMKV) RemoveKey(key string) {
	C.removeValueForKey(unsafe.Pointer(kv), C.wrapGoString(key))
}

func (kv ctorMMKV) RemoveKeys(keys []string) {
	keyArray := (*C.struct_GoStringWrap)(unsafe.Pointer(&keys[0]))
	C.removeValuesForKeys(unsafe.Pointer(kv), keyArray, C.uint64_t(len(keys)))
}

func (kv ctorMMKV) Count() uint64 {
	return uint64(C.count(unsafe.Pointer(kv)))
}

func (kv ctorMMKV) AllKeys() []string {
	count := uint64(0)
	keyArray := C.allKeys(unsafe.Pointer(kv), (*C.uint64_t)(&count))
	if keyArray == nil || count == 0 {
		return []string{}
	}
	// turn C array into go slice with offset(0), length(count) & capacity(count)
	keys := (*[1 << 30]C.struct_GoStringWrap)(unsafe.Pointer(keyArray))[0:count:count]

	// Actually the keys IS a go string slice, but we need to COPY the elements for the caller to use.
	// Too bad go doesn't has destructors, hence we can't simply TRANSFER ownership of C memory.
	result := make([]string, count)
	for index := uint64(0); index < count; index++ {
		key := keys[index]
		result[index] = C.GoStringN(key.ptr, C.int(key.length))
	}

	C.freeStringArray(keyArray, C.size_t(count))
	C.free(unsafe.Pointer(keyArray))
	return result
}

func (kv ctorMMKV) Contains(key string) bool {
	ret := C.containsKey(unsafe.Pointer(kv), C.wrapGoString(key))
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
	ret := C.reKey(unsafe.Pointer(kv), C.wrapGoString(newKey))
	return bool(ret)
}

func (kv ctorMMKV) CheckReSetCryptKey(key string) {
	ret := C.checkReSetCryptKey(unsafe.Pointer(kv), C.wrapGoString(key))
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
