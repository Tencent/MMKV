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

/**
 * Per-platform test bootstrap. `commonTest` tests call [initializeIfPossible]
 * first; if it returns `false` the caller should `return` to skip the test on
 * platforms where MMKV cannot be initialized from a pure unit-test runtime
 * (notably Android unit tests without Robolectric / a Context).
 */
internal expect object MMKVTestEnv {
    /**
     * Initialize MMKV with an isolated test root directory. Safe to call
     * multiple times — subsequent calls are no-ops.
     *
     * @return true if MMKV is ready to use on this platform; false to skip.
     */
    fun initializeIfPossible(): Boolean

    /** Generate a unique MMKV ID for a test to avoid cross-test state. */
    fun uniqueID(prefix: String): String

    /**
     * True on platforms whose native bridge still has a known boolean
     * round-trip issue. Tests use this to gate the boolean assertions until
     * the platform binding is fixed.
     */
    val hasKnownBoolRoundTripIssue: Boolean
}
