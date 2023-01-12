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

/*
#include "golang-bridge.h"
#include <stdlib.h>
*/
import "C"

// the callback type of Logger
type LogHandler func(level int, file string, line int, function string, message string)

var gLogHandler LogHandler

// register a log callback
//
// Deprecated: use mmkv.InitializeMMKVWithLogLevelAndHandler(rootDir, logLevel, logHandler) instead.
func RegisterLogHandler(logHandler LogHandler) {
	gLogHandler = logHandler

	C.setWantsLogRedirect(C.bool(true))
}

// unregister a log callback
func UnRegisterLogHandler() {
	gLogHandler = nil

	C.setWantsLogRedirect(C.bool(false))
}

//export myLogHandler
func myLogHandler(level int, file string, line int, function string, message string) {
	if gLogHandler != nil {
		gLogHandler(level, file, line, function, message)
	}
}

const (
	OnErrorDiscard = iota // When there's an error, MMKV will discard everything by default.
	OnErrorRecover        // When there's an error, MMKV will try to recover as much data as possible.
)

const (
	MMKVCRCCheckFail = iota
	MMKVFileLength
)

// the callback type of error handler
// error is either MMKVCRCCheckFail or MMKVFileLength
// return OnErrorDiscard (default) or OnErrorRecover
type ErrorHandler func(mmapID string, error int) int

var gErrorHandler ErrorHandler

// register a error callback
func RegisterErrorHandler(errorHandler ErrorHandler) {
	gErrorHandler = errorHandler

	C.setWantsErrorHandle(C.bool(true))
}

// unregister a error callback
func UnRegisterErrorHandler() {
	gErrorHandler = nil

	C.setWantsErrorHandle(C.bool(false))
}

//export myErrorHandler
func myErrorHandler(mmapID string, error int) int {
	if gErrorHandler != nil {
		return gErrorHandler(mmapID, error)
	}
	return OnErrorDiscard
}

// the type of content change handler
type ContentChangeHandler func(mmapID string)

var gContentChangeHandler ContentChangeHandler

// register a content change callback
func RegisterContentChangeHandler(contentChangeHandler ContentChangeHandler) {
	gContentChangeHandler = contentChangeHandler

	C.setWantsContentChangeHandle(C.bool(true))
}

// unregister a content change callback
func UnRegisterContentChangeHandler() {
	gContentChangeHandler = nil

	C.setWantsContentChangeHandle(C.bool(false))
}

//export myContentChangeHandler
func myContentChangeHandler(mmapID string) {
	if gContentChangeHandler != nil {
		gContentChangeHandler(mmapID)
	}
}
