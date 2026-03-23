package com.tencent.mmkv.sample

import platform.posix.usleep

actual fun platformSleep(millis: Long) {
    usleep((millis * 1000).toUInt())
}
