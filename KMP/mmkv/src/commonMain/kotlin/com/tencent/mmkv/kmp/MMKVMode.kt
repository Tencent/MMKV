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
 * MMKV instance mode constants.
 */
object MMKVMode {
    /** Single-process mode. The default mode on an MMKV instance. */
    const val SINGLE_PROCESS = 1 shl 0

    /** Multi-process mode. */
    const val MULTI_PROCESS = 1 shl 1

    /** Read-only mode. */
    const val READ_ONLY = 1 shl 5
}

/**
 * MMKV expire duration constants (in seconds).
 */
object MMKVExpireDuration {
    const val Never: UInt = 0u
    const val InMinute: UInt = 60u
    const val InHour: UInt = 3_600u
    const val InDay: UInt = 86_400u
    const val InMonth: UInt = 2_592_000u
    const val InYear: UInt = 946_080_000u
}
