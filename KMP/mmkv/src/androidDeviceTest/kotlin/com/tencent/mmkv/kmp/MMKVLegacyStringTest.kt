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
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class MMKVLegacyStringTest {

    @Test
    fun modifiedUtf8ValueFromAndroidRemainsReadable() {
        MMKVTestEnv.initialize()
        val id = MMKVTestEnv.uniqueID("legacy-modified-utf8")
        val android = AndroidMMKV.mmkvWithID(id)
        val kmp = MMKV.mmkvWithID(id)
        val value = "before\u0000\uD83D\uDE00after"
        try {
            assertTrue(android.encode("legacy", value))
            assertEquals(value, kmp.decodeString("legacy"))
        } finally {
            kmp.clearAll()
            kmp.close()
        }
    }
}
