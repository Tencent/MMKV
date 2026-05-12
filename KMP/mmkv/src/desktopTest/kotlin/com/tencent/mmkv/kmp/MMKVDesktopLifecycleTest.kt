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

import java.util.concurrent.CountDownLatch
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.concurrent.thread
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class MMKVDesktopLifecycleTest {
    @Test
    fun contentLoadCallbackFiresAfterRegistration() {
        assertTrue(MMKVTestEnv.initializeIfPossible())

        val id = MMKVTestEnv.uniqueID("desktop-load")
        val loadedIDs = CopyOnWriteArrayList<String>()
        val latch = CountDownLatch(1)
        MMKV.registerHandler(object : MMKVHandler() {
            override fun onMMKVContentLoadSuccessfully(mmapID: String) {
                loadedIDs += mmapID
                if (mmapID == id) {
                    latch.countDown()
                }
            }
        })

        val kv = MMKV.mmkvWithID(id)
        try {
            assertTrue(kv.encodeString("key", "value"))
            assertTrue(latch.await(5, TimeUnit.SECONDS), "content-load callback did not fire for $id; loaded=$loadedIDs")
        } finally {
            MMKV.unRegisterHandler()
            kv.clearAll()
        }
    }

    @Test
    fun logCallbackFiresAfterRegistration() {
        assertTrue(MMKVTestEnv.initializeIfPossible())

        val messages = CopyOnWriteArrayList<String>()
        val latch = CountDownLatch(1)
        MMKV.registerHandler(object : MMKVHandler() {
            override fun wantLogRedirect(): Boolean = true
            override fun mmkvLog(level: MMKVLogLevel, file: String, line: Int, function: String, message: String) {
                messages += message
                latch.countDown()
            }
        })

        val kv = MMKV.mmkvWithID(MMKVTestEnv.uniqueID("desktop-log"))
        try {
            assertTrue(kv.encodeString("key", "value"))
            assertTrue(latch.await(5, TimeUnit.SECONDS), "log callback did not fire")
            assertTrue(messages.isNotEmpty())
        } finally {
            MMKV.unRegisterHandler()
            kv.clearAll()
        }
    }

    @Test
    fun unregisterStopsCallbacksForNewInstances() {
        assertTrue(MMKVTestEnv.initializeIfPossible())

        val calls = CopyOnWriteArrayList<String>()
        MMKV.registerHandler(object : MMKVHandler() {
            override fun onMMKVContentLoadSuccessfully(mmapID: String) {
                calls += mmapID
            }
        })
        MMKV.unRegisterHandler()

        val kv = MMKV.mmkvWithID(MMKVTestEnv.uniqueID("desktop-unregistered"))
        try {
            assertTrue(kv.encodeString("key", "value"))
            Thread.sleep(200)
            assertTrue(calls.isEmpty(), "handler was called after unregister: $calls")
        } finally {
            kv.clearAll()
        }
    }

    @Test
    fun closeAndReopenSameID() {
        assertTrue(MMKVTestEnv.initializeIfPossible())

        val id = MMKVTestEnv.uniqueID("desktop-close")
        val kv = MMKV.mmkvWithID(id)
        assertTrue(kv.encodeString("persisted", "value"))
        kv.close()

        val reopened = MMKV.mmkvWithID(id)
        try {
            assertEquals("value", reopened.decodeString("persisted"))
        } finally {
            reopened.clearAll()
        }
    }

    @Test
    fun concurrentHandlerRegistrationDoesNotCrash() {
        assertTrue(MMKVTestEnv.initializeIfPossible())

        val failed = AtomicBoolean(false)
        val stop = AtomicBoolean(false)
        val kv = MMKV.mmkvWithID(MMKVTestEnv.uniqueID("desktop-concurrent"))

        val writer = thread(start = true) {
            try {
                var i = 0
                while (!stop.get()) {
                    kv.encodeInt("i", i++)
                    kv.decodeInt("i")
                }
            } catch (_: Throwable) {
                failed.set(true)
            }
        }

        try {
            repeat(100) {
                MMKV.registerHandler(object : MMKVHandler() {
                    override fun wantLogRedirect(): Boolean = true
                })
                MMKV.unRegisterHandler()
            }
        } finally {
            stop.set(true)
            writer.join(5_000)
            MMKV.unRegisterHandler()
            kv.clearAll()
        }

        assertFalse(failed.get(), "concurrent reads/writes failed while registering handlers")
    }
}
