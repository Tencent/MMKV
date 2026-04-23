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

import kotlin.random.Random

private val lock = Any()
private var initialized = false

internal actual object MMKVTestEnv {
    actual fun initializeIfPossible(): Boolean {
        synchronized(lock) {
            if (!initialized) {
                val tmp = System.getProperty("java.io.tmpdir") ?: "/tmp"
                val root = "$tmp/mmkv-kmp-desktop-tests-${Random.nextLong()}"
                java.io.File(root).mkdirs()
                MMKV.initialize(root)
                initialized = true
            }
        }
        return true
    }

    actual fun uniqueID(prefix: String): String = "$prefix-${Random.nextLong()}"

    // JNA Boolean return marshaling reads 4 bytes, but the C bridge returns
    // 1-byte `bool`; upper bytes are ABI-indeterminate. Fix tracked for a
    // follow-up pass (needs a custom TypeMapper or BYTE-sized wrapper).
    actual val hasKnownBoolRoundTripIssue: Boolean = true
}
