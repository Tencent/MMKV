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
 * A facade that wraps a custom root directory.
 * All MMKV instances created from a [MMKVNameSpace] share the same root directory.
 */
expect class MMKVNameSpace {

    /** The root folder of this NameSpace. */
    val rootDir: String

    /** Create an MMKV instance with an unique ID within this namespace. */
    fun mmkvWithID(mmapID: String, config: MMKVConfig = MMKVConfig()): MMKV

    fun backupOneToDirectory(mmapID: String, dstDir: String): Boolean
    fun restoreOneFromDirectory(mmapID: String, srcDir: String): Boolean

    fun isFileValid(mmapID: String): Boolean
    fun removeStorage(mmapID: String): Boolean
    fun checkExist(mmapID: String): Boolean

    /** Free native resources. After calling this, the namespace handle is invalid. */
    fun close()

    companion object {
        /** Create a namespace with a custom root dir. */
        fun of(rootDir: String): MMKVNameSpace

        /** Get the default namespace (global root dir). */
        fun default(): MMKVNameSpace
    }
}
