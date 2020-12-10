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
// #include "golang-bridge.h"
// #include <stdlib.h>
import "C"
// import "fmt"
import "unsafe"

const (
    MMKVLogDebug = iota // not available for release/product build
    MMKVLogInfo  // default level
    MMKVLogWarning
    MMKVLogError
    MMKVLogNone // special level used to disable all log messages
)

const (
    MMKV_SINGLE_PROCESS = iota << 1
    MMKV_MULTI_PROCESS = iota << 2
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
/*
    SetBool(value bool, key string) bool
    SetInt32(value int32, key string) bool
    SetInt64(value int64, key string) bool
    SetUInt32(value uint32, key string) bool
    SetUInt64(value uint64, key string) bool
    SetFloat32(value float32, key string) bool
    SetFloat64(value float64, key string) bool
    SetString(value string, key string) bool

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

    RemoveKey(key string)
    RemoveKeys(keys []string)
    ClearAll()

    Contains(key string) bool
    Count() uint64
    AllKeys() []string*/
    TotalSize() uint64
    ActualSize() uint64
/*
    MMAP_ID() string

    Sync(sync bool)
    Trim()
    Close()

    ReKey(newKey string) bool
    CryptKey() string
    */
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

func MMKVWithID(mmapID string) MMKV {
    cmmapID := C.CString(mmapID)
    mmkv := ctorMMKV(C.getMMKVWithID(cmmapID, MMKV_SINGLE_PROCESS, nil, nil))
    C.free(unsafe.Pointer(cmmapID))

    return MMKV(mmkv)
}

func (kv ctorMMKV) TotalSize() uint64 {
    return uint64(C.totalSize(unsafe.Pointer(kv)))
}

func (kv ctorMMKV) ActualSize() uint64 {
    return uint64(C.actualSize(unsafe.Pointer(kv)))
}
