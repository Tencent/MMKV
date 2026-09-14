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

import android.content.ContextWrapper
import androidx.test.platform.app.InstrumentationRegistry
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNotNull
import kotlin.test.assertSame
import kotlin.test.assertTrue

class MMKVContextInitializerTest {

    private val targetContext
        get() = InstrumentationRegistry.getInstrumentation().targetContext

    @Test
    fun dependenciesAreEmpty() {
        assertTrue(MMKVContextInitializer().dependencies().isEmpty())
    }

    @Test
    fun createReturnsAndStoresApplicationContext() {
        val applicationContext = targetContext.applicationContext
        val result = MMKVContextInitializer().create(targetContext)
        assertSame(applicationContext, result)
        assertSame(applicationContext, MMKVContextHolder.applicationContext)
    }

    /**
     * D8 regression guard: [MMKVContextInitializer.create] must normalize a wrapper/Activity-like
     * [android.content.Context] to the application Context instead of throwing, since it runs
     * during `ContentProvider` attach.
     */
    @Test
    fun createOnWrapperContextNeverThrowsAndReturnsApplicationContext() {
        val applicationContext = targetContext.applicationContext
        val wrapperContext = ContextWrapper(targetContext)
        val result = MMKVContextInitializer().create(wrapperContext)
        assertSame(applicationContext, result)
    }

    @Test
    fun holderIsPopulatedAutomaticallyByAppStartupWithNoExplicitCall() {
        assertNotNull(MMKVContextHolder.applicationContext)
        assertSame(targetContext.applicationContext, MMKVContextHolder.applicationContext)
    }

    @Test
    fun requireCapturedContextThrowsIllegalStateExceptionWhenNeverCaptured() {
        val exception = assertFailsWith<IllegalStateException> {
            requireCapturedContext(null)
        }
        assertTrue(exception.message.orEmpty().contains("initialize(context"))
    }

    /**
     * The root directory is fixed process-wide by whichever `initialize(...)` overload runs
     * first (see [MMKVTestEnv.initialize]); both overloads must agree on that same value.
     */
    @Test
    fun contextFreeInitializeReturnsSameRootDirAsContextInitialize() {
        MMKVTestEnv.initialize()
        val withContext = MMKV.initialize(targetContext, logLevel = MMKVLogLevel.None)
        val contextFree = MMKV.initialize(MMKVLogLevel.None)
        assertTrue(contextFree.isNotBlank())
        assertEquals(withContext, contextFree)
    }

    /**
     * Design Data Flow contract: [MMKVContextInitializer.create] performs only
     * `MMKVContextHolder.applicationContext = ctx`, never any I/O, rootDir resolution, or call
     * into `AndroidMMKV.initialize(...)` -- even across a second `androidx.startup` attach in
     * the same process.
     *
     * There is no public "was native init called" getter, so this is observed indirectly: MMKV's
     * native `initializeMMKV()` unconditionally logs an Info-level "root dir: ..." message on
     * every invocation (`Core/MMKV.cpp`), filtered only by the currently effective log level. A
     * log-redirecting handler that observes zero log messages across two `create()` calls is
     * proof no native `initialize()` call occurred as a side effect of `create()`.
     */
    @Test
    fun repeatedAttachNeverTriggersNativeInitialize() {
        MMKVTestEnv.initialize()
        val observedLogs = mutableListOf<MMKVLogLevel>()
        val handler = object : MMKVHandler() {
            override fun wantLogRedirect(): Boolean = true
            override fun mmkvLog(level: MMKVLogLevel, file: String, line: Int, function: String, message: String) {
                observedLogs += level
            }
        }
        MMKV.registerHandler(handler)
        try {
            MMKVContextInitializer().create(targetContext)
            MMKVContextInitializer().create(targetContext)
            assertTrue(observedLogs.isEmpty())
        } finally {
            MMKV.unRegisterHandler()
        }
    }

    // Spec scenario "Default log level applies when omitted" (3.2) has no dedicated test: there is
    // no public getter for the applied native log level, and AndroidMMKV.java's
    // initialize(context, logLevel) clears any registered MMKVHandler before its own log line
    // fires, so no callback-based signal is observable either. Pre-existing AndroidMMKV.java
    // behavior, tracked separately as a future contribution.

    /**
     * Regression guard for the "first-call-wins `g_rootDir`" risk flagged in the proposal. The
     * existing `initialize(context, rootDir, ...)` overload takes its own explicit
     * [android.content.Context] and never reads [MMKVContextHolder] (see `MMKV.android.kt`);
     * capture running automatically in this process must not override or interfere with it.
     */
    @Test
    fun customRootDirInitializeIsUnaffectedByCapturedContext() {
        val capturedContextBefore = MMKVContextHolder.applicationContext
        assertNotNull(capturedContextBefore)

        val customRootDir = MMKVTestEnv.uniquePath("custom-root-dir")
        val returnedRootDir = MMKV.initialize(targetContext, customRootDir, logLevel = MMKVLogLevel.None)

        assertEquals(customRootDir, returnedRootDir)
        assertSame(capturedContextBefore, MMKVContextHolder.applicationContext)
    }

    /**
     * Calling the context-free overload twice in a row, with two different log levels, must
     * behave exactly like calling the existing `initialize(context, logLevel)` overload twice in
     * a row with the same two log levels -- neither throws, and both keep returning the same
     * deterministic root directory across the sequence, proving the new overload does not diverge
     * from this already-established repeated-call contract.
     *
     * This intentionally does not assert log-level equality itself: the applied native log level
     * has no observable public signal for either overload (see the "Default log level" comment
     * earlier in this file).
     */
    @Test
    fun repeatedContextFreeInitializeMatchesContextInitializeReInitSemantics() {
        MMKVTestEnv.initialize()

        fun sequence(invoke: (MMKVLogLevel) -> String): List<String> =
            listOf(MMKVLogLevel.Debug, MMKVLogLevel.Error).map(invoke)

        val contextSequence = sequence { level -> MMKV.initialize(targetContext, level) }
        val contextFreeSequence = sequence { level -> MMKV.initialize(level) }

        assertTrue(contextSequence.all { it.isNotBlank() })
        assertEquals(contextSequence, contextFreeSequence)
    }
}
