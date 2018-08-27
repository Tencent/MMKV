package com.tencent.mmkvdemo

import com.tencent.mmkv.MMKV
import java.util.*

fun kotlinFunctionalTest() {
    var mmkv = MMKV.mmkvWithID("testKotlin")
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

        val bytes = byteArrayOf('m'.toByte(), 'm'.toByte(), 'k'.toByte(), 'v'.toByte())
    mmkv.encode("bytes", bytes)
    println("bytes: " + String(mmkv.decodeBytes("bytes")))

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