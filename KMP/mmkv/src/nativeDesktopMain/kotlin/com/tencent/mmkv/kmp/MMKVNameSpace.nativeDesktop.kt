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

import kotlinx.cinterop.*
import mmkv.*

@OptIn(ExperimentalForeignApi::class)
actual class MMKVNameSpace internal constructor(private val handle: COpaquePointer) {

    actual val rootDir: String
        get() = mmkv_namespace_root_dir(handle)?.toKString() ?: ""

    actual fun mmkvWithID(mmapID: String, config: MMKVConfig): MMKV {
        return withNativeConfig(config) { cfg ->
            MMKV(mmkv_namespace_mmkv_with_id(handle, mmapID, cfg)!!)
        }
    }

    actual fun backupOneToDirectory(mmapID: String, dstDir: String): Boolean =
        mmkv_namespace_backup_one(handle, mmapID, dstDir)

    actual fun restoreOneFromDirectory(mmapID: String, srcDir: String): Boolean =
        mmkv_namespace_restore_one(handle, mmapID, srcDir)

    actual fun isFileValid(mmapID: String): Boolean =
        mmkv_namespace_is_file_valid(handle, mmapID)

    actual fun removeStorage(mmapID: String): Boolean =
        mmkv_namespace_remove_storage(handle, mmapID)

    actual fun checkExist(mmapID: String): Boolean =
        mmkv_namespace_check_exist(handle, mmapID)

    actual fun close() = mmkv_namespace_free(handle)

    actual companion object {
        actual fun of(rootDir: String): MMKVNameSpace =
            MMKVNameSpace(mmkv_namespace(rootDir)!!)

        actual fun default(): MMKVNameSpace =
            MMKVNameSpace(mmkv_default_namespace()!!)
    }
}
