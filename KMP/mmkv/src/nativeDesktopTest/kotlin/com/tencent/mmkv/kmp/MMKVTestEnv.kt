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
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.toKString
import platform.posix.getenv
import platform.posix.mkdir

private var initialized = false

@OptIn(ExperimentalForeignApi::class)
internal actual object MMKVTestEnv {
    actual fun initializeIfPossible(): Boolean {
        if (!initialized) {
            val tmpBase = getenv("TMPDIR")?.toKString()
                ?: getenv("TEMP")?.toKString()
                ?: "/tmp"
            val root = "$tmpBase/mmkv-kmp-nd-tests-${Random.nextLong()}"
            // MMKV native code will create the root dir during initialize().
            MMKV.initialize(rootDir = root)
            initialized = true
        }
        return true
    }

    actual fun uniqueID(prefix: String): String = "$prefix-${Random.nextLong()}"

    actual fun uniquePath(prefix: String): String {
        val tmpBase = getenv("TMPDIR")?.toKString()
            ?: getenv("TEMP")?.toKString()
            ?: "/tmp"
        val path = "$tmpBase/mmkv-kmp-$prefix-${Random.nextLong()}"
        mkdir(path, 0x1C0u)
        return path
    }

    actual val hasKnownBoolRoundTripIssue: Boolean = false
}
