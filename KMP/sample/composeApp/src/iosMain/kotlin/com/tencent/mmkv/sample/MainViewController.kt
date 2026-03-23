package com.tencent.mmkv.sample

import androidx.compose.ui.window.ComposeUIViewController
import com.tencent.mmkv.kmp.MMKV
import com.tencent.mmkv.kmp.initialize
import platform.Foundation.NSFileManager

fun MainViewController() = ComposeUIViewController {
    App()
}

fun initializeMMKV() {
    // Retrieve the shared App Group container path for multi-process MMKV access.
    // This requires the "App Groups" entitlement configured in Xcode with matching identifier.
    val groupDir = NSFileManager.defaultManager
        .containerURLForSecurityApplicationGroupIdentifier("group.tencent.mmkv")
        ?.path

    if (groupDir != null) {
        MMKV.initialize(groupDir = groupDir)
    } else {
        MMKV.initialize()
    }
}
