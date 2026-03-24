package com.tencent.mmkv.sample

import androidx.compose.ui.window.Window
import androidx.compose.ui.window.application
import com.tencent.mmkv.kmp.MMKV
import com.tencent.mmkv.kmp.initialize

fun main() = application {
    // Initialize MMKV with a directory under user home
    val home = System.getProperty("user.home")
    val rootDir = "$home/.mmkv_kmp_sample"
    MMKV.initialize(rootDir)

    Window(
        onCloseRequest = {
            MMKV.onExit()
            exitApplication()
        },
        title = "MMKV KMP Sample"
    ) {
        App()
    }
}
