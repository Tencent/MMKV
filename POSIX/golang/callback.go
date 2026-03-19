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

const (
	OnErrorDiscard = iota // When there's an error, MMKV will discard everything by default.
	OnErrorRecover        // When there's an error, MMKV will try to recover as much data as possible.
)

const (
	MMKVCRCCheckFail = iota
	MMKVFileLength
)

// Handler is the unified callback interface for MMKV.
type Handler interface {
	// WantLogRedirect returns true to enable log redirecting.
	WantLogRedirect() bool

	// MMKVLog is called for each log message when log redirecting is enabled.
	MMKVLog(level int, file string, line int, function string, message string)

	// OnMMKVCRCCheckFail is called when a CRC check fails.
	// Return OnErrorDiscard (default) or OnErrorRecover.
	OnMMKVCRCCheckFail(mmapID string) int

	// OnMMKVFileLengthError is called when a file length mismatch is detected.
	// Return OnErrorDiscard (default) or OnErrorRecover.
	OnMMKVFileLengthError(mmapID string) int

	// WantContentChangeNotification returns true to enable inter-process content change notifications.
	WantContentChangeNotification() bool

	// OnContentChangedByOuterProcess is called when content is changed by another process.
	OnContentChangedByOuterProcess(mmapID string)

	// OnMMKVContentLoadSuccessfully is called when an MMKV file is loaded successfully.
	OnMMKVContentLoadSuccessfully(mmapID string)
}

// DefaultHandler provides default implementations for all Handler methods.
// Embed this in your handler struct to only override the methods you need.
type DefaultHandler struct{}

func (d DefaultHandler) WantLogRedirect() bool                     { return false }
func (d DefaultHandler) MMKVLog(level int, file string, line int, function string, message string) {}
func (d DefaultHandler) OnMMKVCRCCheckFail(mmapID string) int      { return OnErrorDiscard }
func (d DefaultHandler) OnMMKVFileLengthError(mmapID string) int   { return OnErrorDiscard }
func (d DefaultHandler) WantContentChangeNotification() bool       { return false }
func (d DefaultHandler) OnContentChangedByOuterProcess(mmapID string) {}
func (d DefaultHandler) OnMMKVContentLoadSuccessfully(mmapID string) {}

var gHandler Handler

// RegisterHandler registers a unified callback handler for MMKV.
func RegisterHandler(handler Handler) {
	gHandler = handler

	wantLog := handler.WantLogRedirect()
	wantContent := handler.WantContentChangeNotification()
	C.setWantsHandler(C.bool(true), C.bool(wantLog), C.bool(wantContent))
}

// UnRegisterHandler unregisters the callback handler for MMKV.
func UnRegisterHandler() {
	gHandler = nil

	C.setWantsHandler(C.bool(false), C.bool(false), C.bool(false))
}

//export myLogHandler
func myLogHandler(level int, file string, line int, function string, message string) {
	if gHandler != nil {
		gHandler.MMKVLog(level, file, line, function, message)
	}
}

//export myErrorHandler
func myErrorHandler(mmapID string, error int) int {
	if gHandler != nil {
		if error == MMKVCRCCheckFail {
			return gHandler.OnMMKVCRCCheckFail(mmapID)
		} else if error == MMKVFileLength {
			return gHandler.OnMMKVFileLengthError(mmapID)
		}
	}
	return OnErrorDiscard
}

//export myContentChangeHandler
func myContentChangeHandler(mmapID string) {
	if gHandler != nil {
		gHandler.OnContentChangedByOuterProcess(mmapID)
	}
}

//export myContentLoadedHandler
func myContentLoadedHandler(mmapID string) {
	if gHandler != nil {
		gHandler.OnMMKVContentLoadSuccessfully(mmapID)
	}
}
