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
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class MMKVLegacyStringTest {

    @Test
    fun stringsRoundTripAcrossKmpAndAndroidJavaEncoding() {
        MMKVTestEnv.initialize()
        val id = MMKVTestEnv.uniqueID("kmp-java-string-bridge")
        val kmp = MMKV.mmkvWithID(id)
        val android = AndroidMMKV.mmkvWithID(id)
        val value = "before\u0000\uD83D\uDE00after"
        try {
            assertTrue(kmp.encodeString("written-by-kmp", value))
            assertEquals(value, android.decodeString("written-by-kmp"))

            assertTrue(android.encode("written-by-java", value))
            assertEquals(value, kmp.decodeString("written-by-java"))
        } finally {
            kmp.clearAll()
            kmp.close()
        }
    }

    @Test
    fun modifiedUtf8StringsFromOlderAndroidKmpReleasesRemainReadable() {
        MMKVTestEnv.initialize()
        val kv = MMKV.mmkvWithID(MMKVTestEnv.uniqueID("legacy-modified-utf8"))
        try {
            val values = listOf(
                "before\u0000after",
                "before\uD83D\uDE00after",
            )
            values.forEachIndexed { index, value ->
                val key = "legacy-$index"
                val legacyBytes = value.encodeModifiedUtf8()
                assertFalse(legacyBytes.contentEquals(value.encodeToByteArray()))
                assertTrue(kv.encodeBytes(key, legacyBytes))
                assertContentEquals(legacyBytes, kv.decodeBytes(key))
                assertEquals(value, kv.decodeString(key))
            }
        } finally {
            kv.clearAll()
            kv.close()
        }
    }

    @Test
    fun malformedBytesUseLenientUtf8WithoutEnteringJniStringDecoding() {
        MMKVTestEnv.initialize()
        val kv = MMKV.mmkvWithID(MMKVTestEnv.uniqueID("malformed-utf8"))
        try {
            val malformedValues = listOf(
                byteArrayOf(0xFF.toByte()),
                byteArrayOf(0xC0.toByte(), 0xAF.toByte()),
                byteArrayOf(0xED.toByte(), 0xA0.toByte()),
            )
            malformedValues.forEachIndexed { index, bytes ->
                val key = "malformed-$index"
                assertTrue(kv.encodeBytes(key, bytes))
                assertEquals(bytes.decodeToString(), kv.decodeString(key))
            }
        } finally {
            kv.clearAll()
            kv.close()
        }
    }

    @Test
    fun legacyNulKeysCanBeEnumeratedAndExplicitlyRemovedButNotImported() {
        MMKVTestEnv.initialize()
        val legacyKey = "account\u0000admin"

        val sourceID = MMKVTestEnv.uniqueID("legacy-nul-key-source")
        val androidSource = AndroidMMKV.mmkvWithID(sourceID)
        assertTrue(androidSource.encode("ordinary-source-key", 3))
        assertTrue(androidSource.encode(legacyKey, 7))
        val source = MMKV.mmkvWithID(sourceID)

        val uncheckedDestination = AndroidMMKV.mmkvWithID(MMKVTestEnv.uniqueID("legacy-nul-key-unchecked-destination"))
        try {
            assertEquals(2L, uncheckedDestination.importFrom(androidSource))
            assertTrue(uncheckedDestination.containsKey(legacyKey))
        } finally {
            uncheckedDestination.clearAll()
            uncheckedDestination.close()
        }

        val destination = MMKV.mmkvWithID(MMKVTestEnv.uniqueID("legacy-nul-key-destination"))
        try {
            assertTrue(destination.encodeInt("destination-only-key", 11))
            assertTrue(legacyKey in source.allKeys)
            assertFailsWith<IllegalArgumentException> { source.decodeInt(legacyKey) }
            assertFailsWith<IllegalArgumentException> { destination.importFrom(source) }
            assertEquals(1L, destination.count)
            assertEquals(11, destination.decodeInt("destination-only-key"))
            assertFalse(destination.containsKey("ordinary-source-key"))

            source.removeLegacyNulKey(legacyKey)
            assertFalse(androidSource.containsKey(legacyKey))
            assertFalse(legacyKey in source.allKeys)
        } finally {
            source.clearAll()
            destination.clearAll()
            source.close()
            destination.close()
        }
    }
}

/** Encode UTF-16 code units the way JNI GetStringUTFChars did for legacy KMP writes. */
private fun String.encodeModifiedUtf8(): ByteArray {
    val result = ArrayList<Byte>(length * 3)
    for (char in this) {
        val code = char.code
        when {
            code == 0 -> {
                result += 0xC0.toByte()
                result += 0x80.toByte()
            }
            code <= 0x7F -> result += code.toByte()
            code <= 0x7FF -> {
                result += (0xC0 or (code shr 6)).toByte()
                result += (0x80 or (code and 0x3F)).toByte()
            }
            else -> {
                result += (0xE0 or (code shr 12)).toByte()
                result += (0x80 or ((code shr 6) and 0x3F)).toByte()
                result += (0x80 or (code and 0x3F)).toByte()
            }
        }
    }
    return result.toByteArray()
}
