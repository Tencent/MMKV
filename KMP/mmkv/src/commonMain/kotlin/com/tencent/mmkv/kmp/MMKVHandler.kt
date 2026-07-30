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

package com.tencent.mmkv.kmp

/**
 * Unified callback handler for MMKV.
 * Callback is called on the operating thread of the MMKV instance.
 */
abstract class MMKVHandler {

    /**
     * By default MMKV will discard all data on crc32-check failure.
     * @param mmapID The unique ID of the MMKV instance.
     * @return Return [MMKVRecoverStrategic.OnErrorRecover] to recover any data on the file.
     */
    open fun onMMKVCRCCheckFail(mmapID: String): MMKVRecoverStrategic = MMKVRecoverStrategic.OnErrorDiscard

    /**
     * By default MMKV will discard all data on file length mismatch.
     * @param mmapID The unique ID of the MMKV instance.
     * @return Return [MMKVRecoverStrategic.OnErrorRecover] to recover any data on the file.
     */
    open fun onMMKVFileLengthError(mmapID: String): MMKVRecoverStrategic = MMKVRecoverStrategic.OnErrorDiscard

    /**
     * @return Return true if you want log redirecting.
     */
    open fun wantLogRedirect(): Boolean = false

    /**
     * Log redirecting.
     * @param level The level of this log.
     * @param file The file name of this log.
     * @param line The line of code of this log.
     * @param function The function name of this log.
     * @param message The content of this log.
     */
    open fun mmkvLog(level: MMKVLogLevel, file: String, line: Int, function: String, message: String) {}

    /**
     * @return Return true if you want inter-process content change notification.
     */
    open fun wantContentChangeNotification(): Boolean = false

    /**
     * Inter-process content change notification.
     * @param mmapID The unique ID of the changed MMKV instance.
     */
    open fun onContentChangedByOuterProcess(mmapID: String) {}

    /**
     * Called when an MMKV file is loaded successfully.
     * @param mmapID The unique ID of the loaded MMKV instance.
     */
    open fun onMMKVContentLoadSuccessfully(mmapID: String) {}
}
