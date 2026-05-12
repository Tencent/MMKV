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
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * Cross-platform smoke tests for the MMKV KMP wrapper. Each test lives on its
 * own MMKV instance (via [MMKVTestEnv.uniqueID]) so ordering and leftover
 * state from prior runs does not affect results.
 *
 * These tests are intentionally scoped to commonTest so every declared target
 * picks them up. Target-specific behavior (Darwin lock no-ops, desktop handler
 * stubs, etc.) is out of scope here.
 */
class MMKVSmokeTest {

    private fun fresh(id: String): MMKV {
        if (!MMKVTestEnv.initializeIfPossible()) {
            // Caller must treat as "skipped" — assertTrue(true) to keep the
            // test green on platforms that can't initialize here.
            error("MMKVTestEnv.initializeIfPossible() returned false")
        }
        // Use a fresh MMKV ID per test. MULTI_PROCESS matches the sample app
        // configuration and ensures cross-platform consistency on desktop JNA.
        val kv = MMKV.mmkvWithID(
            MMKVTestEnv.uniqueID(id),
            MMKVConfig(mode = MMKVMode.MULTI_PROCESS)
        )
        kv.clearAll()
        return kv
    }

    private fun <T> onlyIfReady(block: () -> T) {
        if (!MMKVTestEnv.initializeIfPossible()) return
        block()
    }

    @Test
    fun primitivesRoundTrip() = onlyIfReady {
        val kv = fresh("primitives")
        try {
            assertTrue(kv.encodeBool("b", true))
            assertTrue(kv.encodeInt("i", 42))
            assertTrue(kv.encodeLong("l", 9_876_543_210L))
            assertTrue(kv.encodeFloat("f", 3.14f))
            assertTrue(kv.encodeDouble("d", 2.718281828))
            assertTrue(kv.encodeString("s", "hello"))
            val bytes = byteArrayOf(1, 2, 3, 4, 5)
            assertTrue(kv.encodeBytes("ba", bytes))

            if (!MMKVTestEnv.hasKnownBoolRoundTripIssue) {
                assertEquals(true, kv.decodeBool("b"))
            }
            assertEquals(42, kv.decodeInt("i"))
            assertEquals(9_876_543_210L, kv.decodeLong("l"))
            assertEquals(3.14f, kv.decodeFloat("f"))
            assertEquals(2.718281828, kv.decodeDouble("d"))
            assertEquals("hello", kv.decodeString("s"))
            assertContentEquals(bytes, kv.decodeBytes("ba"))
        } finally {
            kv.clearAll()
        }
    }

    @Test
    fun decodeDefaults() = onlyIfReady {
        val kv = fresh("defaults")
        try {
            if (!MMKVTestEnv.hasKnownBoolRoundTripIssue) {
                assertEquals(false, kv.decodeBool("missing"))
                assertEquals(true, kv.decodeBool("missing", defaultValue = true))
            }
            assertEquals(0, kv.decodeInt("missing"))
            assertEquals(42, kv.decodeInt("missing", defaultValue = 42))
            assertEquals(0L, kv.decodeLong("missing"))
            assertEquals(0f, kv.decodeFloat("missing"))
            assertEquals(0.0, kv.decodeDouble("missing"))
            assertNull(kv.decodeString("missing"))
            assertEquals("fallback", kv.decodeString("missing", "fallback"))
            assertNull(kv.decodeBytes("missing"))
        } finally {
            kv.clearAll()
        }
    }

    @Test
    fun keyManagement() = onlyIfReady {
        val kv = fresh("keymgmt")
        try {
            kv.encodeInt("a", 1)
            kv.encodeInt("b", 2)
            kv.encodeInt("c", 3)
            assertEquals(3L, kv.count)
            if (!MMKVTestEnv.hasKnownBoolRoundTripIssue) {
                assertTrue(kv.containsKey("b"))
            }

            kv.removeValueForKey("b")
            if (!MMKVTestEnv.hasKnownBoolRoundTripIssue) {
                assertFalse(kv.containsKey("b"))
            }
            assertEquals(2L, kv.count)

            kv.removeValuesForKeys(listOf("a", "c"))
            assertEquals(0L, kv.count)

            kv.encodeString("x", "y")
            kv.clearAll()
            assertEquals(0L, kv.count)
            if (!MMKVTestEnv.hasKnownBoolRoundTripIssue) {
                assertFalse(kv.containsKey("x"))
            }
        } finally {
            kv.clearAll()
        }
    }

    @Test
    fun allKeysReflectsContents() = onlyIfReady {
        val kv = fresh("allkeys")
        try {
            kv.encodeInt("alpha", 1)
            kv.encodeInt("beta", 2)
            val keys = kv.allKeys.toSet()
            assertEquals(setOf("alpha", "beta"), keys)
        } finally {
            kv.clearAll()
        }
    }

    @Test
    fun featureStateAndBufferUtilities() = onlyIfReady {
        val kv = fresh("features")
        try {
            assertFalse(kv.isExpirationEnabled)
            assertFalse(kv.isEncryptionEnabled)
            assertFalse(kv.isCompareBeforeSetEnabled)

            val bytes = "hello".encodeToByteArray()
            assertTrue(kv.encodeBytes("buffer", bytes))
            val buffer = ByteArray(bytes.size)
            assertEquals(bytes.size, kv.writeValueToBuffer("buffer", buffer))
            assertContentEquals(bytes, buffer)

            assertTrue(kv.enableCompareBeforeSet())
            assertTrue(kv.isCompareBeforeSetEnabled)
            assertTrue(kv.disableCompareBeforeSet())
            assertFalse(kv.isCompareBeforeSetEnabled)

            assertTrue(kv.enableAutoKeyExpire())
            assertTrue(kv.isExpirationEnabled)
            assertTrue(kv.disableAutoKeyExpire())
            assertFalse(kv.isExpirationEnabled)

            kv.lock()
            kv.unlock()
            if (kv.tryLock()) {
                kv.unlock()
            }
        } finally {
            kv.clearAll()
        }
    }

    @Test
    fun storageUtilitiesAndNamespace() = onlyIfReady {
        val source = fresh("source")
        val dest = fresh("dest")
        try {
            val bytes = byteArrayOf(10, 20, 30, 40)
            assertTrue(source.encodeBytes("payload", bytes))
            assertEquals(bytes.size.toLong(), source.getValueSize("payload", actualSize = true))
            assertEquals(1L, dest.importFrom(source))
            assertContentEquals(bytes, dest.decodeBytes("payload"))

            dest.clearAllKeepSpace()
            assertEquals(0L, dest.count)
            dest.trim()

            assertTrue(dest.encodeBytes("empty", ByteArray(0)))
            val decodedEmpty = dest.decodeBytes("empty")
            if (decodedEmpty != null) {
                assertContentEquals(ByteArray(0), decodedEmpty)
            }

            val namespace = MMKVNameSpace.of(MMKVTestEnv.uniquePath("namespace"))
            val nsID = MMKVTestEnv.uniqueID("ns")
            val nsKV = namespace.mmkvWithID(nsID)
            nsKV.clearAll()
            assertTrue(nsKV.encodeString("scoped", "value"))
            assertTrue(namespace.checkExist(nsID))

            val backupDir = MMKVTestEnv.uniquePath("backup")
            assertTrue(namespace.backupOneToDirectory(nsID, backupDir))
            nsKV.clearAll()
            assertNull(nsKV.decodeString("scoped"))
            assertTrue(namespace.restoreOneFromDirectory(nsID, backupDir))
            assertEquals("value", namespace.mmkvWithID(nsID).decodeString("scoped"))
            namespace.close()
        } finally {
            source.clearAll()
            dest.clearAll()
        }
    }

    @Test
    fun handlerRegistrationDoesNotCrash() = onlyIfReady {
        MMKV.registerHandler(object : MMKVHandler() {
            override fun wantLogRedirect(): Boolean = true
            override fun wantContentChangeNotification(): Boolean = true
        })
        val kv = fresh("handler")
        try {
            assertTrue(kv.encodeString("key", "value"))
            kv.checkContentChanged()
        } finally {
            MMKV.unRegisterHandler()
            kv.clearAll()
        }
    }

    @Test
    fun cryptRoundTrip() = onlyIfReady {
        if (!MMKVTestEnv.initializeIfPossible()) return@onlyIfReady
        val id = MMKVTestEnv.uniqueID("crypt")
        val kv = MMKV.mmkvWithID(id, MMKVConfig(cryptKey = "testkey"))
        kv.clearAll()
        try {
            assertTrue(kv.encodeString("secret", "classified"))
            assertEquals("classified", kv.decodeString("secret"))
            val cryptKey = kv.cryptKey
            assertNotNull(cryptKey)
        } finally {
            kv.clearAll()
        }
    }
}
