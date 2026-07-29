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

import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class MMKVSmokeTest {

    private fun fresh(prefix: String): MMKV {
        MMKVTestEnv.initialize()
        return MMKV.mmkvWithID(MMKVTestEnv.uniqueID(prefix)).also {
            it.clearAll()
        }
    }

    @Test
    fun releaseVersionMatchesPublishedVersion() {
        MMKVTestEnv.initialize()
        assertEquals("v2.4.1", MMKV.version())
    }

    @Test
    fun primitivesAndBytesRoundTrip() {
        val kv = fresh("round-trip")
        try {
            assertTrue(kv.encodeBool("bool", true))
            assertTrue(kv.encodeInt("int", 42))
            assertTrue(kv.encodeLong("long", 9_876_543_210L))
            assertTrue(kv.encodeString("string", "MMKV"))

            val bytes = byteArrayOf(1, 2, 3, 4)
            assertTrue(kv.encodeBytes("bytes", bytes))

            assertTrue(kv.decodeBool("bool"))
            assertEquals(42, kv.decodeInt("int"))
            assertEquals(9_876_543_210L, kv.decodeLong("long"))
            assertEquals("MMKV", kv.decodeString("string"))
            assertContentEquals(bytes, kv.decodeBytes("bytes"))
        } finally {
            kv.clearAll()
        }
    }

    @Test
    fun emptyBytesRemainDistinctFromMissingValue() {
        val kv = fresh("empty-bytes")
        try {
            assertNull(kv.decodeBytes("missing"))
            assertTrue(kv.encodeBytes("empty", ByteArray(0)))
            assertTrue(kv.containsKey("empty"))
            assertContentEquals(ByteArray(0), kv.decodeBytes("empty"))
        } finally {
            kv.clearAll()
        }
    }

    @Test
    fun bufferAndFeatureStateUseNativeImplementation() {
        val kv = fresh("features")
        try {
            val bytes = "buffer-value".encodeToByteArray()
            assertTrue(kv.encodeBytes("buffer", bytes))
            val output = ByteArray(bytes.size)
            assertEquals(bytes.size, kv.writeValueToBuffer("buffer", output))
            assertContentEquals(bytes, output)

            assertFalse(kv.isExpirationEnabled)
            assertTrue(kv.enableAutoKeyExpire())
            assertTrue(kv.isExpirationEnabled)
            assertTrue(kv.disableAutoKeyExpire())
            assertFalse(kv.isExpirationEnabled)

            assertFalse(kv.isCompareBeforeSetEnabled)
            assertTrue(kv.enableCompareBeforeSet())
            assertTrue(kv.isCompareBeforeSetEnabled)
            assertTrue(kv.disableCompareBeforeSet())
            assertFalse(kv.isCompareBeforeSetEnabled)
        } finally {
            kv.clearAll()
        }
    }

    @Test
    fun namespaceIsUsable() {
        MMKVTestEnv.initialize()
        val namespace = MMKVNameSpace.of(MMKVTestEnv.uniquePath("namespace"))
        try {
            val id = MMKVTestEnv.uniqueID("namespace")
            val kv = namespace.mmkvWithID(id)
            assertTrue(kv.encodeString("key", "value"))
            assertTrue(namespace.checkExist(id))
            assertEquals("value", namespace.mmkvWithID(id).decodeString("key"))
        } finally {
            namespace.close()
            namespace.close()
        }
        assertFailsWith<IllegalStateException> {
            namespace.checkExist("closed")
        }
    }

    @Test
    fun closeIsIdempotentAndTerminal() {
        MMKVTestEnv.initialize()
        val id = MMKVTestEnv.uniqueID("close")
        run {
            val kv = MMKV.mmkvWithID(id)
            kv.clearAll()
            assertTrue(kv.encodeString("key", "value"))

            kv.close()
            kv.close()
            assertFailsWith<IllegalStateException> {
                kv.decodeString("key")
            }
        }

        // Two live wrappers backed by the same native instance are outside the
        // supported close() contract. The closed wrapper is scoped away before reopen.
        val reopened = MMKV.mmkvWithID(id)
        try {
            assertEquals("value", reopened.decodeString("key"))
        } finally {
            reopened.clearAll()
            reopened.close()
        }
    }

}
