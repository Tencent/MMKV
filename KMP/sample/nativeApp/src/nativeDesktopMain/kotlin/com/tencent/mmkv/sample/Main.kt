package com.tencent.mmkv.sample

import com.tencent.mmkv.kmp.MMKV
import com.tencent.mmkv.kmp.initialize
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.toKString
import platform.posix.getenv

@OptIn(ExperimentalForeignApi::class)
fun main() {
    // Initialize MMKV with a directory under HOME
    val home = getenv("HOME")?.toKString() ?: getenv("USERPROFILE")?.toKString() ?: "/tmp"
    val rootDir = "$home/.mmkv_kmp_sample"
    println("Initializing MMKV at: $rootDir")
    val root = MMKV.initialize(rootDir)
    println("MMKV root: $root")
    println()

    runDemo()
}
