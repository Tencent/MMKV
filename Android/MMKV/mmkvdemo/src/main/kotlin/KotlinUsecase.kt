/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2018 THL A29 Limited, a Tencent company.
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

package com.tencent.mmkvdemo

import com.tencent.mmkv.MMKV
import java.util.*

fun kotlinFunctionalTest() {
    val mmkv = MMKV.mmkvWithID("testKotlin")
    mmkv.encode("bool", true)
    println("bool = " + mmkv.decodeBool("bool"))

    mmkv.encode("int", Integer.MIN_VALUE)
    println("int: " + mmkv.decodeInt("int"))

    mmkv.encode("long", java.lang.Long.MAX_VALUE)
    println("long: " + mmkv.decodeLong("long"))

    mmkv.encode("float", -3.14f)
    println("float: " + mmkv.decodeFloat("float"))

    mmkv.encode("double", java.lang.Double.MIN_VALUE)
    println("double: " + mmkv.decodeDouble("double"))

    mmkv.encode("string", "Hello from mmkv")
    println("string: " + mmkv.decodeString("string"))

    val bytes = byteArrayOf('m'.code.toByte(), 'm'.code.toByte(), 'k'.code.toByte(), 'v'.code.toByte())
    mmkv.encode("bytes", bytes)
    println("bytes: " + mmkv.decodeBytes("bytes")?.let { String(it) })

    mmkv.encode("empty_bytes", "".toByteArray());
    println("empty_bytes: " + mmkv.decodeBytes("empty_bytes")?.let { String(it) })

    mmkv.encode("stringSet", HashSet<String>())
    println("empty string set: " + mmkv.decodeStringSet("stringSet"))

    println("allKeys: " + Arrays.toString(mmkv.allKeys()))
    println("count = " + mmkv.count() + ", totalSize = " + mmkv.totalSize())
    println("containsKey[string]: " + mmkv.containsKey("string"))

    mmkv.removeValueForKey("bool")
    println("bool: " + mmkv.decodeBool("bool"))
    mmkv.removeValuesForKeys(arrayOf("int", "long"))
    //mmkv.clearAll();
    mmkv.clearMemoryCache()
    println("allKeys: " + Arrays.toString(mmkv.allKeys()))
    println("isFileValid[" + mmkv.mmapID() + "]: " + MMKV.isFileValid(mmkv.mmapID()))
}