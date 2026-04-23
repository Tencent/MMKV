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
import kotlinx.cinterop.BetaInteropApi
import kotlinx.cinterop.ExperimentalForeignApi
import platform.Foundation.NSFileManager
import platform.Foundation.NSTemporaryDirectory

private var initialized = false

@OptIn(ExperimentalForeignApi::class, BetaInteropApi::class)
internal actual object MMKVTestEnv {
    actual fun initializeIfPossible(): Boolean {
        if (!initialized) {
            val tmp = NSTemporaryDirectory()
            val root = "${tmp}mmkv-kmp-darwin-tests-${Random.nextLong()}"
            NSFileManager.defaultManager.createDirectoryAtPath(
                path = root,
                withIntermediateDirectories = true,
                attributes = null,
                error = null,
            )
            MMKV.initialize(rootDir = root)
            initialized = true
        }
        return true
    }

    actual fun uniqueID(prefix: String): String = "$prefix-${Random.nextLong()}"

    actual val hasKnownBoolRoundTripIssue: Boolean = false
}
