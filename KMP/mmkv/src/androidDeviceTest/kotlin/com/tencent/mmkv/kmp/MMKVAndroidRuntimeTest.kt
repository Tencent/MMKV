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
import kotlin.test.assertEquals
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
}
