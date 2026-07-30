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

import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import java.util.UUID

internal actual object MMKVTestEnv {
    private var initialized = false

    private val context
        get() = InstrumentationRegistry.getInstrumentation().targetContext

    actual fun initialize() {
        if (initialized) {
            return
        }
        MMKV.initialize(context, logLevel = MMKVLogLevel.None)
        initialized = true
    }

    actual fun uniqueID(prefix: String): String = "$prefix-${UUID.randomUUID()}"

    actual fun uniquePath(prefix: String): String =
        File(context.cacheDir, "mmkv-kmp-$prefix-${UUID.randomUUID()}").also {
            check(it.mkdirs()) { "Unable to create MMKV test directory: $it" }
        }.absolutePath
}
