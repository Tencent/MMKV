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

import kotlinx.cinterop.BetaInteropApi
import kotlinx.cinterop.ExperimentalForeignApi
import platform.Foundation.NSFileManager
import platform.Foundation.NSTemporaryDirectory
import kotlin.random.Random

private var initialized = false

@OptIn(ExperimentalForeignApi::class, BetaInteropApi::class)
internal actual object MMKVTestEnv {
    actual fun initialize() {
        if (initialized) {
            return
        }
        val root = uniquePath("root")
        MMKV.initialize(rootDir = root, logLevel = MMKVLogLevel.None)
        initialized = true
    }

    actual fun uniqueID(prefix: String): String = "$prefix-${Random.nextLong()}"

    actual fun uniquePath(prefix: String): String {
        val path = "${NSTemporaryDirectory()}mmkv-kmp-$prefix-${Random.nextLong()}"
        val created = NSFileManager.defaultManager.createDirectoryAtPath(
            path = path,
            withIntermediateDirectories = true,
            attributes = null,
            error = null,
        )
        check(created) { "Unable to create MMKV test directory: $path" }
        return path
    }
}
