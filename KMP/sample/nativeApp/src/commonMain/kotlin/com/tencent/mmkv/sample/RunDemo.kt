package com.tencent.mmkv.sample

import com.tencent.mmkv.kmp.MMKV

fun runDemo() {
    val demo = MMKVDemo()

    println("===== Functional Test =====")
    demo.functionalTest().forEach(::println)
    println()

    println("===== Encryption Test =====")
    demo.testReKey().forEach(::println)
    println()

    println("===== Auto Expiration Test =====")
    demo.testAutoExpire().forEach(::println)
    println()

    println("===== Compare Before Set Test =====")
    demo.testCompareBeforeSet().forEach(::println)
    println()

    println("===== Backup & Restore Test =====")
    demo.testBackupAndRestore().forEach(::println)
    println()

    println("===== Remove Storage & Check Exist Test =====")
    demo.testRemoveStorageAndCheckExist().forEach(::println)
    println()

    println("All tests completed!")
    MMKV.onExit()
}
