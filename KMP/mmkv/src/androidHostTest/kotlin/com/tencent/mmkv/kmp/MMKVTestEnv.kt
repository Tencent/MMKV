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

// Android host tests run on a JVM without a Context, so MMKV native
// initialization is not reachable. Instrumented tests would be required to
// exercise the Android code path. We skip cleanly here rather than NPE.
internal actual object MMKVTestEnv {
    actual fun initializeIfPossible(): Boolean = false
    actual fun uniqueID(prefix: String): String = "$prefix-${Random.nextLong()}"
    actual val hasKnownBoolRoundTripIssue: Boolean = false
}
