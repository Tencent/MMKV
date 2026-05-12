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
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class MMKVDarwinLifecycleTest {
    @Test
    fun namespaceInstanceCloseInvalidatesWrapper() {
        if (!MMKVTestEnv.initializeIfPossible()) return

        val namespace = MMKVNameSpace.of(MMKVTestEnv.uniquePath("darwin-lifecycle"))
        val mmapID = "ns_test-${MMKVTestEnv.uniqueID("close")}"
        val kv = namespace.mmkvWithID(mmapID)
        assertTrue(kv.encodeString("key", "value"))

        kv.close()
        kv.close()
        assertFailsWith<IllegalStateException> {
            kv.clearMemoryCache()
        }

        namespace.removeStorage(mmapID)
        namespace.close()
    }
}
