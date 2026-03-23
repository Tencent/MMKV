package com.tencent.mmkv.sample

actual fun platformSleep(millis: Long) {
    Thread.sleep(millis)
}
