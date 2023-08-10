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
// #cgo LDFLAGS: -L./lib -lmmkv -lcore -lz -lpthread
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
import "math"

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
	MMKV_Expire_Never = iota
	MMKV_Expire_Minute = 60
	MMKV_Expire_Hour = 60 * 60
	MMKV_Expire_Day = 24 * 60 * 60
	MMKV_Expire_Month = 30 * 24 * 60 * 60
	MMKV_Expire_Year = 365 * 30 * 24 * 60 * 60
)

// MMBuffer a wrapper of native C memory, efficient for simple usage
// must call MMBuffer.Destroy() after no longer usage
type MMBuffer struct {
	ptr    uintptr
	length int
}

// Pointer the address of underlying memory
func (buffer MMBuffer) Pointer() uintptr {
	return buffer.ptr
}

// Length the size of underlying memory
func (buffer MMBuffer) Length() int {
	return buffer.length
}

// ByteSliceView get byte slice view of underlying memory
// the slice is valid as long as MMBuffer.Destroy() not called
func (buffer MMBuffer) ByteSliceView() []byte {
	bytes := (*[1 << 30]byte)(unsafe.Pointer(buffer.ptr))[0:buffer.length:buffer.length]
	return bytes
}

// StringView get string view of underlying memory
// the string is valid as long as MMBuffer.Destroy() not called
func (buffer MMBuffer) StringView() string {
	return *((*string)(unsafe.Pointer(&buffer)))
}

// Destroy must call Destroy() after no longer usage
func (buffer MMBuffer) Destroy() {
	C.free(unsafe.Pointer(buffer.ptr))
}

type MMKV interface {
	SetBool(value bool, key string) bool
	SetBoolExpire(value bool, key string, expireDuration uint32) bool
	GetBool(key string) bool
	GetBoolWithDefault(key string, defaultValue bool) bool

	SetInt32(value int32, key string) bool
	SetInt32Expire(value int32, key string, expireDuration uint32) bool
	GetInt32(key string) int32
	GetInt32WithDefault(key string, defaultValue int32) int32

	SetInt64(value int64, key string) bool
	SetInt64Expire(value int64, key string, expireDuration uint32) bool
	GetInt64(key string) int64
	GetInt64WithDefault(key string, defaultValue int64) int64

	SetUInt32(value uint32, key string) bool
	SetUInt32Expire(value uint32, key string, expireDuration uint32) bool
	GetUInt32(key string) uint32
	GetUInt32WithDefault(key string, defaultValue uint32) uint32

	SetUInt64(value uint64, key string) bool
	SetUInt64Expire(value uint64, key string, expireDuration uint32) bool
	GetUInt64(key string) uint64
	GetUInt64WithDefault(key string, defaultValue uint64) uint64

	SetFloat32(value float32, key string) bool
	SetFloat32Expire(value float32, key string, expireDuration uint32) bool
	GetFloat32(key string) float32
	GetFloat32WithDefault(key string, defaultValue float32) float32

	SetFloat64(value float64, key string) bool
	SetFloat64Expire(value float64, key string, expireDuration uint32) bool
	GetFloat64(key string) float64
	GetFloat64WithDefault(key string, defaultValue float64) float64

	// SetString string value should be utf-8 encoded
	SetString(value string, key string) bool
	SetStringExpire(value string, key string, expireDuration uint32) bool
	GetString(key string) string
	// GetStringBuffer get C memory directly (without memcpy), much more efferent for large value
	GetStringBuffer(key string) MMBuffer

	SetBytes(value []byte, key string) bool
	SetBytesExpire(value []byte, key string, expireDuration uint32) bool
	GetBytes(key string) []byte
	// GetBytesBuffer get C memory directly (without memcpy), much more efferent for large value
	GetBytesBuffer(key string) MMBuffer

	RemoveKey(key string)
	RemoveKeys(keys []string)

	// ClearAll clear all key-values
	ClearAll()

	// Count return count of keys
	Count() uint64
	// CountNonExpiredKeys same as Count() except that it filters expired keys
	CountNonExpiredKeys() uint64

	AllKeys() []string
	// AllNonExpireKeys same as AllKeys() except that it filters expired keys
	AllNonExpireKeys() []string

	Contains(key string) bool

	// TotalSize total size of the file
	TotalSize() uint64

	// ActualSize actual used size of the file
	ActualSize() uint64

	// MMAP_ID the mmapID of the instance
	MMAP_ID() string

	// Sync Synchronize memory to file. You don't need to call this, really, I mean it. Unless you worry about running out of battery.
	Sync(sync bool)
	// ClearMemoryCache Clear all caches (on memory warning).
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

	/* ReKey Change encryption key for the MMKV instance.
	 * The cryptKey is 16 bytes limited.
	 * You can transfer a plain-text MMKV into encrypted by setting an non-null, non-empty cryptKey.
	 * Or vice versa by passing cryptKey with null. See also checkReSetCryptKey().
	 */
	ReKey(newKey string) bool

	// CheckReSetCryptKey Just reset the cryptKey (will not encrypt or decrypt anything).
	// Usually you should call this method after other process reKey() the multi-process mmkv.
	CheckReSetCryptKey(key string)

	// See also reKey().
	CryptKey() string

	// EnableAutoKeyExpire passing MMKV_Expire_Never (0) means never expire
	EnableAutoKeyExpire(expireDurationInSecond uint32) bool

	DisableAutoKeyExpire() bool
}

type ctorMMKV uintptr

// Version return the version of MMKV
func Version() string {
	version := C.version()
	goStr := C.GoString(version)
	return goStr
}

/*
MMKV must be initialized before any usage.
* Generally speaking you should do this inside main():

	func main() {
		mmkv.InitializeMMKV("/path/to/my/working/dir")
		// other logic
	}
*/
func InitializeMMKV(rootDir string) {
	C.mmkvInitialize(C.wrapGoString(rootDir), MMKVLogInfo, C.bool(false))
}

// Same as the function InitializeMMKV() above, except that you can customize MMKV's log level by passing logLevel.
// You can even turnoff logging by passing MMKVLogNone, which we don't recommend doing.
func InitializeMMKVWithLogLevel(rootDir string, logLevel int) {
	C.mmkvInitialize(C.wrapGoString(rootDir), C.int32_t(logLevel), C.bool(false))
}

// Same as the function InitializeMMKVWithLogLevel() above, except that you can provide a logHandler at the very beginning.
func InitializeMMKVWithLogLevelAndHandler(rootDir string, logLevel int, logHandler LogHandler) {
	gLogHandler = logHandler
	C.mmkvInitialize(C.wrapGoString(rootDir), C.int32_t(logLevel), C.bool(true))
}

// OnExit Call before App exists, it's just fine not calling it on most case (except when the device shutdown suddenly).
func OnExit() {
	C.onExit()
}

// PageSize return the page size of memory
func PageSize() int32 {
	return int32(C.pageSize())
}

// DefaultMMKV a generic purpose instance in single-process mode.
func DefaultMMKV() MMKV {
	mmkv := ctorMMKV(C.getDefaultMMKV(MMKV_SINGLE_PROCESS, C.GoStringWrapNil()))
	return MMKV(mmkv)
}

// DefaultMMKVWithMode a generic purpose instance in single-process or multi-process mode.
func DefaultMMKVWithMode(mode int) MMKV {
	mmkv := ctorMMKV(C.getDefaultMMKV(C.int(mode), C.GoStringWrapNil()))
	return MMKV(mmkv)
}

// DefaultMMKVWithModeAndCryptKey an encrypted generic purpose instance in single-process or multi-process mode.
func DefaultMMKVWithModeAndCryptKey(mode int, cryptKey string) MMKV {
	mmkv := ctorMMKV(C.getDefaultMMKV(MMKV_SINGLE_PROCESS, C.wrapGoString(cryptKey)))
	return MMKV(mmkv)
}

// MMKVWithID an instance with specific location ${MMKV Root}/mmapID, in single-process mode.
func MMKVWithID(mmapID string) MMKV {
	cStrNull := C.GoStringWrapNil()
	mmkv := ctorMMKV(C.getMMKVWithID(C.wrapGoString(mmapID), MMKV_SINGLE_PROCESS, cStrNull, cStrNull))
	return MMKV(mmkv)
}

// MMKVWithIDAndMode an instance with specific location ${MMKV Root}/mmapID, in single-process or multi-process mode.
func MMKVWithIDAndMode(mmapID string, mode int) MMKV {
	cStrNull := C.GoStringWrapNil()
	mmkv := ctorMMKV(C.getMMKVWithID(C.wrapGoString(mmapID), C.int(mode), cStrNull, cStrNull))
	return MMKV(mmkv)
}

// MMKVWithIDAndModeAndCryptKey an encrypted instance with specific location ${MMKV Root}/mmapID, in single-process or multi-process mode.
func MMKVWithIDAndModeAndCryptKey(mmapID string, mode int, cryptKey string) MMKV {
	cStrNull := C.GoStringWrapNil()
	mmkv := ctorMMKV(C.getMMKVWithID(C.wrapGoString(mmapID), C.int(mode), C.wrapGoString(cryptKey), cStrNull))
	return MMKV(mmkv)
}

// BackupOneToDirectory backup one MMKV instance (from the root dir of MMKV) to dstDir
func BackupOneToDirectory(mmapID string, dstDir string) bool {
	cStrNull := C.GoStringWrapNil()
	ret := C.backupOneToDirectory(C.wrapGoString(mmapID), C.wrapGoString(dstDir), cStrNull)
	return bool(ret)
}

// RestoreOneFromDirectory restore one MMKV instance from srcDir (to the root dir of MMKV)
func RestoreOneFromDirectory(mmapID string, srcDir string) bool {
	cStrNull := C.GoStringWrapNil()
	ret := C.restoreOneFromDirectory(C.wrapGoString(mmapID), C.wrapGoString(srcDir), cStrNull)
	return bool(ret)
}

// BackupAllToDirectory backup all MMKV instance (from the root dir of MMKV) to dstDir
// return count of MMKV successfully backup-ed
func BackupAllToDirectory(dstDir string) uint64 {
	cStrNull := C.GoStringWrapNil()
	ret := C.backupAllToDirectory(C.wrapGoString(dstDir), cStrNull)
	return uint64(ret)
}

// RestoreAllFromDirectory restore all MMKV instance from srcDir (to the root dir of MMKV)
// return count of MMKV successfully restored
func RestoreAllFromDirectory(srcDir string) uint64 {
	cStrNull := C.GoStringWrapNil()
	ret := C.restoreAllFromDirectory(C.wrapGoString(srcDir), cStrNull)
	return uint64(ret)
}

func (kv ctorMMKV) SetBool(value bool, key string) bool {
	ret := C.encodeBool(unsafe.Pointer(kv), C.wrapGoString(key), C.bool(value))
	return bool(ret)
}

func (kv ctorMMKV) SetBoolExpire(value bool, key string, expireDuration uint32) bool {
	ret := C.encodeBool_v2(unsafe.Pointer(kv), C.wrapGoString(key), C.bool(value), C.uint32_t(expireDuration))
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

func (kv ctorMMKV) SetInt32Expire(value int32, key string, expireDuration uint32) bool {
	ret := C.encodeInt32_v2(unsafe.Pointer(kv), C.wrapGoString(key), C.int32_t(value), C.uint32_t(expireDuration))
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

func (kv ctorMMKV) SetUInt32Expire(value uint32, key string, expireDuration uint32) bool {
	ret := C.encodeUInt32_v2(unsafe.Pointer(kv), C.wrapGoString(key), C.uint32_t(value), C.uint32_t(expireDuration))
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

func (kv ctorMMKV) SetInt64Expire(value int64, key string, expireDuration uint32) bool {
	ret := C.encodeInt64_v2(unsafe.Pointer(kv), C.wrapGoString(key), C.int64_t(value), C.uint32_t(expireDuration))
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

func (kv ctorMMKV) SetUInt64Expire(value uint64, key string, expireDuration uint32) bool {
	ret := C.encodeUInt64_v2(unsafe.Pointer(kv), C.wrapGoString(key), C.uint64_t(value), C.uint32_t(expireDuration))
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

func (kv ctorMMKV) SetFloat32Expire(value float32, key string, expireDuration uint32) bool {
	ret := C.encodeFloat_v2(unsafe.Pointer(kv), C.wrapGoString(key), C.float(value), C.uint32_t(expireDuration))
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

func (kv ctorMMKV) SetFloat64Expire(value float64, key string, expireDuration uint32) bool {
	ret := C.encodeDouble_v2(unsafe.Pointer(kv), C.wrapGoString(key), C.double(value), C.uint32_t(expireDuration))
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

func (kv ctorMMKV) SetStringExpire(value string, key string, expireDuration uint32) bool {
	cValue := C.wrapGoString(value)
	ret := C.encodeBytes_v2(unsafe.Pointer(kv), C.wrapGoString(key), cValue, C.uint32_t(expireDuration))
	return bool(ret)
}

func (kv ctorMMKV) GetString(key string) string {
	var length uint64

	cValue := C.decodeBytes(unsafe.Pointer(kv), C.wrapGoString(key), (*C.uint64_t)(&length))
	value := C.GoStringN((*C.char)(cValue), C.int(length))

	C.free(unsafe.Pointer(cValue))
	return value
}

func (kv ctorMMKV) GetStringBuffer(key string) MMBuffer {
	return kv.GetBytesBuffer(key)
}

func (kv ctorMMKV) SetBytes(value []byte, key string) bool {
	cValue := C.wrapGoByteSlice(unsafe.Pointer(&value[0]), C.size_t(len(value)))
	ret := C.encodeBytes(unsafe.Pointer(kv), C.wrapGoString(key), cValue)
	return bool(ret)
}

func (kv ctorMMKV) SetBytesExpire(value []byte, key string, expireDuration uint32) bool {
	cValue := C.wrapGoByteSlice(unsafe.Pointer(&value[0]), C.size_t(len(value)))
	ret := C.encodeBytes_v2(unsafe.Pointer(kv), C.wrapGoString(key), cValue, C.uint32_t(expireDuration))
	return bool(ret)
}

func (kv ctorMMKV) GetBytes(key string) []byte {
	var length uint64

	cValue := C.decodeBytes(unsafe.Pointer(kv), C.wrapGoString(key), (*C.uint64_t)(&length))
	value := C.GoBytes(unsafe.Pointer(cValue), C.int(length))

	C.free(unsafe.Pointer(cValue))
	return value
}

func (kv ctorMMKV) GetBytesBuffer(key string) MMBuffer {
	var length uint64

	cValue := C.decodeBytes(unsafe.Pointer(kv), C.wrapGoString(key), (*C.uint64_t)(&length))
	value := MMBuffer{uintptr(cValue), int(length)}

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
	return uint64(C.count(unsafe.Pointer(kv), C.bool(false)))
}

func (kv ctorMMKV) CountNonExpiredKeys() uint64 {
	return uint64(C.count(unsafe.Pointer(kv), C.bool(true)))
}

func (kv ctorMMKV) allKeys(filterExpire bool) []string {
	count := uint64(0)
	keyArray := C.allKeys(unsafe.Pointer(kv), (*C.uint64_t)(&count), C.bool(filterExpire))
	if keyArray == nil || count == 0 {
		return []string{}
	}
	// turn C array into go slice with offset(0), length(count) & capacity(count)
	keys := (*[math.MaxInt32 / unsafe.Sizeof(C.struct_GoStringWrap{})]C.struct_GoStringWrap)(unsafe.Pointer(keyArray))[0:count:count]

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

func (kv ctorMMKV) AllKeys() []string {
	return kv.allKeys(false)
}

func (kv ctorMMKV) AllNonExpireKeys() []string {
	return kv.allKeys(true)
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
	C.checkReSetCryptKey(unsafe.Pointer(kv), C.wrapGoString(key))
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

func (kv ctorMMKV) EnableAutoKeyExpire(expireDurationInSecond uint32) bool {
	ret := C.enableAutoExpire(unsafe.Pointer(kv), C.uint32_t(expireDurationInSecond))
	return bool(ret)
}

func (kv ctorMMKV) DisableAutoKeyExpire() bool {
	ret := C.disableAutoExpire(unsafe.Pointer(kv))
	return bool(ret)
}
