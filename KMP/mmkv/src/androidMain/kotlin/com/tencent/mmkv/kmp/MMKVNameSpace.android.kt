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

import com.tencent.mmkv.MMKV as AndroidMMKV
import com.tencent.mmkv.NameSpace as AndroidNameSpace
import com.tencent.mmkv.MMKVConfig as AndroidMMKVConfig

actual class MMKVNameSpace internal constructor(private val impl: AndroidNameSpace) {

    actual val rootDir: String
        get() = impl.rootDir

    actual fun mmkvWithID(mmapID: String, config: MMKVConfig): MMKV =
        MMKV(impl.mmkvWithID(mmapID, config.toAndroid()))

    actual fun backupOneToDirectory(mmapID: String, dstDir: String): Boolean =
        impl.backupOneToDirectory(mmapID, dstDir)

    actual fun restoreOneFromDirectory(mmapID: String, srcDir: String): Boolean =
        impl.restoreOneMMKVFromDirectory(mmapID, srcDir)

    actual fun isFileValid(mmapID: String): Boolean =
        impl.isFileValid(mmapID)

    actual fun removeStorage(mmapID: String): Boolean =
        impl.removeStorage(mmapID)

    actual fun checkExist(mmapID: String): Boolean =
        impl.checkExist(mmapID)

    actual fun close() {
        // Android NameSpace is a plain wrapper; no native resources to free.
    }

    actual companion object {
        actual fun of(rootDir: String): MMKVNameSpace =
            MMKVNameSpace(AndroidMMKV.nameSpace(rootDir))

        actual fun default(): MMKVNameSpace =
            MMKVNameSpace(AndroidMMKV.defaultNameSpace())
    }
}
