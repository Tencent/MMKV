package com.tencent.mmkv.sample

import com.tencent.mmkv.kmp.MMKV
import com.tencent.mmkv.kmp.MMKVNameSpace
import kotlin.random.Random

fun verifyMMKVConsumer() {
    check(MMKV.version() == "v2.4.2")

    val id = "published-consumer-${Random.nextLong()}"
    val kv = MMKV.mmkvWithID(id)
    check(kv.encodeString("smoke", "passed"))
    check(kv.decodeString("smoke") == "passed")
    kv.clearAll()
    kv.close()
    kv.close()

    val namespace = MMKVNameSpace.of("${MMKV.rootDir()}/published-consumer-namespace")
    val namespaced = namespace.mmkvWithID(id)
    check(namespaced.encodeInt("smoke", 42))
    check(namespaced.decodeInt("smoke") == 42)
    namespaced.clearAll()
    namespaced.close()
    namespace.close()
    namespace.close()
}
