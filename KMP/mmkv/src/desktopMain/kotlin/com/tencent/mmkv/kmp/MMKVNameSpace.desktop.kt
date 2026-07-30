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

import com.sun.jna.Pointer

actual class MMKVNameSpace internal constructor(private val handle: Pointer) {

    private val lib = MMKVLib.INSTANCE

    actual val rootDir: String
        get() = lib.mmkv_namespace_root_dir(handle) ?: ""

    actual fun mmkvWithID(mmapID: String, config: MMKVConfig): MMKV =
        MMKV(lib.mmkv_namespace_mmkv_with_id(handle, mmapID, config.toJna())!!)

    actual fun backupOneToDirectory(mmapID: String, dstDir: String): Boolean =
        lib.mmkv_namespace_backup_one(handle, mmapID, dstDir)

    actual fun restoreOneFromDirectory(mmapID: String, srcDir: String): Boolean =
        lib.mmkv_namespace_restore_one(handle, mmapID, srcDir)

    actual fun isFileValid(mmapID: String): Boolean =
        lib.mmkv_namespace_is_file_valid(handle, mmapID)

    actual fun removeStorage(mmapID: String): Boolean =
        lib.mmkv_namespace_remove_storage(handle, mmapID)

    actual fun checkExist(mmapID: String): Boolean =
        lib.mmkv_namespace_check_exist(handle, mmapID)

    actual fun close() = lib.mmkv_namespace_free(handle)

    actual companion object {
        actual fun of(rootDir: String): MMKVNameSpace =
            MMKVNameSpace(MMKVLib.INSTANCE.mmkv_namespace(rootDir)!!)

        actual fun default(): MMKVNameSpace =
            MMKVNameSpace(MMKVLib.INSTANCE.mmkv_default_namespace()!!)
    }
}
