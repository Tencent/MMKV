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

import cocoapods.MMKV.MMKV as DarwinMMKV
import cocoapods.MMKV.MMKVNameSpace as DarwinMMKVNameSpace
import cocoapods.MMKV.MMKVConfig as DarwinMMKVConfig
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.cValue

@OptIn(ExperimentalForeignApi::class)
actual class MMKVNameSpace internal constructor(private val impl: DarwinMMKVNameSpace) {

    actual val rootDir: String
        get() = impl.rootPath()

    actual fun mmkvWithID(mmapID: String, config: MMKVConfig): MMKV =
        MMKV(impl.mmkvWithID(mmapID, config = config.toDarwinCValue())!!)

    actual fun backupOneToDirectory(mmapID: String, dstDir: String): Boolean =
        impl.backupOneMMKV(mmapID, toDirectory = dstDir)

    actual fun restoreOneFromDirectory(mmapID: String, srcDir: String): Boolean =
        impl.restoreOneMMKV(mmapID, fromDirectory = srcDir)

    actual fun isFileValid(mmapID: String): Boolean =
        impl.isFileValid(mmapID)

    actual fun removeStorage(mmapID: String): Boolean =
        impl.removeStorage(mmapID)

    actual fun checkExist(mmapID: String): Boolean =
        impl.checkExist(mmapID)

    actual fun close() {
        // Darwin MMKVNameSpace is autoreleased by ObjC; no explicit free needed.
    }

    actual companion object {
        actual fun of(rootDir: String): MMKVNameSpace =
            MMKVNameSpace(DarwinMMKV.nameSpace(rootDir)!!)

        actual fun default(): MMKVNameSpace =
            MMKVNameSpace(DarwinMMKV.defaultNameSpace()!!)
    }
}
