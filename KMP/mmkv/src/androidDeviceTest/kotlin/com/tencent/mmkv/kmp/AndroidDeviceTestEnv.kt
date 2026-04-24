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

import androidx.test.platform.app.InstrumentationRegistry
import kotlin.random.Random

internal object AndroidDeviceTestEnv {
    private var initialized = false

    fun initializeIfPossible(): Boolean {
        if (!initialized) {
            MMKV.initialize(InstrumentationRegistry.getInstrumentation().targetContext)
            initialized = true
        }
        return true
    }

    fun uniqueID(prefix: String): String = "$prefix-${Random.nextLong()}"
}
