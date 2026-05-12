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
 */

package com.tencent.mmkv.kmp

import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class MMKVAndroidRuntimeTest {
    @Test
    fun initializesAndRoundTripsThroughAndroidAar() {
        assertTrue(AndroidDeviceTestEnv.initializeIfPossible())

        val kv = MMKV.mmkvWithID(AndroidDeviceTestEnv.uniqueID("android-runtime"))
        kv.clearAll()
        try {
            assertTrue(kv.encodeString("platform", "android"))
            assertEquals("android", kv.decodeString("platform"))
        } finally {
            kv.clearAll()
        }
    }

    @Test
    fun statusGettersReflectRuntimeState() {
        assertTrue(AndroidDeviceTestEnv.initializeIfPossible())

        val plain = MMKV.mmkvWithID(AndroidDeviceTestEnv.uniqueID("status-plain"))
        val encrypted = MMKV.mmkvWithID(
            AndroidDeviceTestEnv.uniqueID("status-crypt"),
            MMKVConfig(cryptKey = "device-key")
        )
        try {
            assertFalse(plain.isExpirationEnabled)
            assertFalse(plain.isEncryptionEnabled)
            assertFalse(plain.isCompareBeforeSetEnabled)
            assertTrue(encrypted.isEncryptionEnabled)
        } finally {
            plain.clearAll()
            encrypted.clearAll()
        }
    }

    @Test
    fun writeValueToBufferUsesNativePath() {
        assertTrue(AndroidDeviceTestEnv.initializeIfPossible())

        val kv = MMKV.mmkvWithID(AndroidDeviceTestEnv.uniqueID("buffer"))
        kv.clearAll()
        try {
            val bytes = byteArrayOf(1, 3, 5, 7)
            assertTrue(kv.encodeBytes("bytes", bytes))
            val buffer = ByteArray(bytes.size)
            assertEquals(bytes.size, kv.writeValueToBuffer("bytes", buffer))
            assertContentEquals(bytes, buffer)
        } finally {
            kv.clearAll()
        }
    }

    @Test
    fun compareBeforeSetStateIsObservable() {
        assertTrue(AndroidDeviceTestEnv.initializeIfPossible())

        val kv = MMKV.mmkvWithID(AndroidDeviceTestEnv.uniqueID("compare"))
        kv.clearAll()
        try {
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
    fun expirationStateIsObservable() {
        assertTrue(AndroidDeviceTestEnv.initializeIfPossible())

        val kv = MMKV.mmkvWithID(AndroidDeviceTestEnv.uniqueID("expire"))
        kv.clearAll()
        try {
            assertFalse(kv.isExpirationEnabled)
            assertTrue(kv.enableAutoKeyExpire())
            assertTrue(kv.isExpirationEnabled)
            assertTrue(kv.disableAutoKeyExpire())
            assertFalse(kv.isExpirationEnabled)
        } finally {
            kv.clearAll()
        }
    }

    @Test
    fun lockUnlockTryLockDoNotCrashOnDevice() {
        assertTrue(AndroidDeviceTestEnv.initializeIfPossible())

        val kv = MMKV.mmkvWithID(
            AndroidDeviceTestEnv.uniqueID("lock"),
            MMKVConfig(mode = MMKVMode.MULTI_PROCESS)
        )
        try {
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
    fun handlerRegistrationLifecycleOnDevice() {
        assertTrue(AndroidDeviceTestEnv.initializeIfPossible())

        MMKV.registerHandler(object : MMKVHandler() {
            override fun wantLogRedirect(): Boolean = true
            override fun wantContentChangeNotification(): Boolean = true
        })
        val kv = MMKV.mmkvWithID(AndroidDeviceTestEnv.uniqueID("handler"))
        try {
            assertTrue(kv.encodeString("key", "value"))
            kv.checkContentChanged()
        } finally {
            MMKV.unRegisterHandler()
            kv.clearAll()
        }
    }

    @Test
    fun namespaceIsolationOnDevice() {
        assertTrue(AndroidDeviceTestEnv.initializeIfPossible())

        val namespaceA = MMKVNameSpace.of(AndroidDeviceTestEnv.uniquePath("ns-a"))
        val namespaceB = MMKVNameSpace.of(AndroidDeviceTestEnv.uniquePath("ns-b"))
        val id = AndroidDeviceTestEnv.uniqueID("same-id")
        try {
            val kvA = namespaceA.mmkvWithID(id)
            val kvB = namespaceB.mmkvWithID(id)
            kvA.clearAll()
            kvB.clearAll()

            assertTrue(kvA.encodeString("scoped", "a"))
            assertTrue(kvB.encodeString("scoped", "b"))
            assertEquals("a", namespaceA.mmkvWithID(id).decodeString("scoped"))
            assertEquals("b", namespaceB.mmkvWithID(id).decodeString("scoped"))
        } finally {
            namespaceA.close()
            namespaceB.close()
        }
    }
}
