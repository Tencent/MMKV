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

import android.content.Context
import androidx.startup.Initializer

/**
 * AndroidX App Startup [Initializer] that captures the application [Context] during process
 * attach, so a later [MMKV.Companion.initialize] call can run without an explicit [Context].
 *
 * This Initializer MUST NOT call MMKV's native `initialize()` or any function that triggers
 * native init: it only captures and stores the [Context].
 */
class MMKVContextInitializer : Initializer<Context> {

    /**
     * Must never throw: this runs during [android.content.ContentProvider] attach, where a
     * thrown exception becomes a `StartupException` and kills the consumer's process startup.
     * [Context.getApplicationContext] returns null only for a [Context] detached from a running
     * [android.app.Application]; the raw [context] is used as a fallback since App Startup
     * already normalizes it before this method runs.
     */
    override fun create(context: Context): Context {
        val appContext = context.applicationContext ?: context
        MMKVContextHolder.applicationContext = appContext
        return appContext
    }

    override fun dependencies(): List<Class<out Initializer<*>>> = emptyList()
}

internal object MMKVContextHolder {
    @Volatile
    var applicationContext: Context? = null
}

internal fun requireCapturedContext(context: Context?): Context = checkNotNull(context) {
    "MMKV application Context was not captured: androidx.startup did not run in this process. " +
        "Call MMKV.initialize(context, ...) instead, or declare an androidx.startup <provider> " +
        "for this process."
}
