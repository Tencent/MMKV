package com.tencent.mmkv.sample

import com.tencent.mmkv.kmp.MMKV
import com.tencent.mmkv.kmp.MMKVConfig
import com.tencent.mmkv.kmp.MMKVExpireDuration
import com.tencent.mmkv.kmp.MMKVMode
import com.tencent.mmkv.kmp.MMKVNameSpace

/**
 * Platform-agnostic MMKV demo logic, ported from the Flutter example app.
 * Platform-specific initialization is handled in androidMain/iosMain before calling this.
 */
class MMKVDemo {
    private val log = mutableListOf<String>()

    private fun print(msg: String) {
        log += msg
    }

    // region Functional Test

    fun functionalTest(): List<String> {
        log.clear()

        // val mmkv = MMKV.defaultMMKV()
        val mmkv = MMKV.mmkvWithID("functionalTest", MMKVConfig(MMKVMode.MULTI_PROCESS))
        print("MMKV version: ${MMKV.version()}")
        print("MMKV root dir: ${MMKV.rootDir()}")
        print("Page size: ${MMKV.pageSize()}")
        print("")

        mmkv.encodeBool("bool", true)
        print("bool = ${mmkv.decodeBool("bool")}")

        mmkv.encodeInt("int32", Int.MAX_VALUE)
        print("max int32 = ${mmkv.decodeInt("int32")}")

        mmkv.encodeInt("int32", Int.MIN_VALUE)
        print("min int32 = ${mmkv.decodeInt("int32")}")

        mmkv.encodeLong("int", Long.MAX_VALUE)
        print("max int = ${mmkv.decodeLong("int")}")

        mmkv.encodeLong("int", Long.MIN_VALUE)
        print("min int = ${mmkv.decodeLong("int")}")

        mmkv.encodeDouble("double", Double.MAX_VALUE)
        print("max double = ${mmkv.decodeDouble("double")}")

        mmkv.encodeDouble("double", Double.MIN_VALUE)
        print("min positive double = ${mmkv.decodeDouble("double")}")

        var str = "Hello dart from MMKV"
        mmkv.encodeString("string", str)
        print("string = ${mmkv.decodeString("string")}")

        mmkv.encodeString("string", "")
        print("empty string = ${mmkv.decodeString("string")}")
        print("contains \"string\": ${mmkv.containsKey("string")}")

        str += " with bytes"
        val bytes = str.encodeToByteArray()
        mmkv.encodeBytes("bytes", bytes)
        val decodedBytes = mmkv.decodeBytes("bytes")
        print("bytes = ${decodedBytes?.decodeToString()}")

        // test empty bytes
        run {
            val emptyBytes = ByteArray(0)
            mmkv.encodeBytes("empty_bytes", emptyBytes)
            val result = mmkv.decodeBytes("empty_bytes")
            print("empty_bytes = ${result?.toList()}")
        }

        print("contains \"bool\": ${mmkv.containsKey("bool")}")
        mmkv.removeValueForKey("bool")
        print("after remove, contains \"bool\": ${mmkv.containsKey("bool")}")
        mmkv.removeValuesForKeys(listOf("int32", "int"))
        print("all keys: ${mmkv.allKeys}")

        mmkv.trim()
        mmkv.clearMemoryCache()
        print("all keys: ${mmkv.allKeys}")
        mmkv.clearAll()
        print("all keys: ${mmkv.allKeys}")
        mmkv.checkContentChanged()
        print("is file valid: ${MMKV.isFileValid(mmkv.mmapID)}")

        // Import test
        testImport()

        return log.toList()
    }

    private fun testMMKVImp(mmkv: MMKV, decodeOnly: Boolean) {
        print("MMKV mmapID = ${mmkv.mmapID}")
        print("string = ${mmkv.decodeString("string")}")

        val str = "Hello dart from MMKV"
        if (!decodeOnly) {
            mmkv.encodeString("string", str)
        }

        if (!decodeOnly) {
            mmkv.encodeBool("bool", true)
        }
        print("bool = ${mmkv.decodeBool("bool")}")

        if (!decodeOnly) {
            mmkv.encodeInt("int32", Int.MAX_VALUE)
        }
        print("max int32 = ${mmkv.decodeInt("int32")}")

        if (!decodeOnly) {
            mmkv.encodeInt("int32", Int.MIN_VALUE)
        }
        print("min int32 = ${mmkv.decodeInt("int32")}")

        if (!decodeOnly) {
            mmkv.encodeLong("int", Long.MAX_VALUE)
        }
        print("max int = ${mmkv.decodeLong("int")}")

        if (!decodeOnly) {
            mmkv.encodeLong("int", Long.MIN_VALUE)
        }
        print("min int = ${mmkv.decodeLong("int")}")

        if (!decodeOnly) {
            mmkv.encodeDouble("double", Double.MAX_VALUE)
        }
        print("max double = ${mmkv.decodeDouble("double")}")

        if (!decodeOnly) {
            mmkv.encodeDouble("double", Double.MIN_VALUE)
        }
        print("min positive double = ${mmkv.decodeDouble("double")}")

        val bytesStr = "$str with bytes"
        if (!decodeOnly) {
            mmkv.encodeBytes("bytes", bytesStr.encodeToByteArray())
        }
        val decoded = mmkv.decodeBytes("bytes")
        print("bytes = ${decoded?.decodeToString()}")

        print("contains \"bool\": ${mmkv.containsKey("bool")}")
        mmkv.removeValueForKey("bool")
        print("after remove, contains \"bool\": ${mmkv.containsKey("bool")}")
        mmkv.removeValuesForKeys(listOf("int32", "int"))
        print("all keys: ${mmkv.allKeys}")
    }

    private fun testMMKV(mmapID: String, cryptKey: String?, aes256: Boolean, decodeOnly: Boolean, rootPath: String?): MMKV {
        val config = MMKVConfig(cryptKey = cryptKey, aes256 = aes256, rootPath = rootPath)
        val mmkv = MMKV.mmkvWithID(mmapID, config)
        testMMKVImp(mmkv, decodeOnly)
        return mmkv
    }

    private fun testImport() {
        print("")
        print("=== Import Test ===")
        val src = MMKV.mmkvWithID("testImportSrc")
        src.encodeBool("bool", true)
        src.encodeInt("int", Int.MIN_VALUE)
        src.encodeLong("long", Long.MAX_VALUE)
        src.encodeString("string", "test import")

        val dst = MMKV.mmkvWithID("testImportDst")
        dst.clearAll()
        dst.enableAutoKeyExpire(1u)
        dst.encodeBool("bool", false)
        dst.encodeInt("int", -1)
        dst.encodeLong("long", 0)
        dst.encodeString("string", "testImportSrc")

        val count = dst.importFrom(src)
        if (count != 4L || dst.count != 4L) {
            print("MMKV: import check count fail (count=$count, dst.count=${dst.count})")
        } else {
            print("import count: $count")
        }
        if (!dst.decodeBool("bool")) print("MMKV: import check bool fail")
        if (dst.decodeInt("int") != Int.MIN_VALUE) print("MMKV: import check int fail")
        if (dst.decodeLong("long") != Long.MAX_VALUE) print("MMKV: import check long fail")
        if (dst.decodeString("string") != "test import") print("MMKV: import check string fail")
        print("import test passed")
    }

    // endregion

    // region Encryption Test

    fun testReKey(): List<String> {
        log.clear()
        print("=== Encryption Test ===")

        val mmapID = "testAES_reKey1"
        val kv = testMMKV(mmapID, null, false, false, null)

        kv.reKey("Key_seq_1")
        kv.clearMemoryCache()
        print("")
        print("--- after reKey to Key_seq_1 ---")
        testMMKV(mmapID, "Key_seq_1", false, true, null)

        kv.reKey("Key_Seq_Very_Looooooooong", aes256 = true)
        kv.clearMemoryCache()
        print("")
        print("--- after reKey to AES256 ---")
        testMMKV(mmapID, "Key_Seq_Very_Looooooooong", true, true, null)

        kv.reKey(null)
        kv.clearMemoryCache()
        print("")
        print("--- after reKey to null (remove encryption) ---")
        testMMKV(mmapID, null, false, true, null)

        print("")
        print("encryption test passed")
        return log.toList()
    }

    // endregion

    // region Auto Expiration Test

    fun testAutoExpire(): List<String> {
        log.clear()
        print("=== Auto Expiration Test ===")

        // test enable auto expire by config
        val config = MMKVConfig(enableKeyExpire = true, expiredInSeconds = 1u)
        val mmkv = MMKV.mmkvWithID("test_auto_expire", config)
        mmkv.clearAllKeepSpace()

        mmkv.encodeBool("auto_expire_key_1", true)
        mmkv.encodeInt("auto_expire_key_2", 1, 1u)
        mmkv.encodeLong("auto_expire_key_3", 2L, 1u)
        mmkv.encodeDouble("auto_expire_key_4", 3.0, 1u)
        mmkv.encodeString("auto_expire_key_5", "hello auto expire", 1u)
        mmkv.encodeBytes("auto_expire_key_6", "hello auto expire".encodeToByteArray(), 1u)
        mmkv.encodeBool("never_expire_key_1", true, MMKVExpireDuration.Never)

        print("before sleep: count = ${mmkv.count}")
        print("sleeping 2 seconds for expiration...")

        // sleep 2 seconds to let keys expire
        platformSleep(2000)

        print("auto_expire_key_1: ${mmkv.containsKey("auto_expire_key_1")}")
        print("auto_expire_key_2: ${mmkv.containsKey("auto_expire_key_2")}")
        print("auto_expire_key_3: ${mmkv.containsKey("auto_expire_key_3")}")
        print("auto_expire_key_4: ${mmkv.containsKey("auto_expire_key_4")}")
        print("auto_expire_key_5: ${mmkv.containsKey("auto_expire_key_5")}")
        print("auto_expire_key_6: ${mmkv.containsKey("auto_expire_key_6")}")
        print("never_expire_key_1: ${mmkv.containsKey("never_expire_key_1")}")
        print("")
        print("count non-expired keys: ${mmkv.countNonExpiredKeys()}")
        print("all non-expired keys: ${mmkv.allNonExpiredKeys()}")

        return log.toList()
    }

    // endregion

    // region Compare Before Set Test

    fun testCompareBeforeSet(): List<String> {
        log.clear()
        print("=== Compare Before Set Test ===")

        val config = MMKVConfig(expectedCapacity = 4096 * 2)
        val mmkv = MMKV.mmkvWithID("testCompareBeforeSet", config)
        mmkv.enableCompareBeforeSet()

        mmkv.encodeString("key", "extra")

        // int test
        run {
            val key = "int"
            val v = 12345
            mmkv.encodeInt(key, v)
            val actualSize = mmkv.actualSize
            print("actualSize = $actualSize")
            print("v = ${mmkv.decodeInt(key)}")

            mmkv.encodeInt(key, v) // same value
            val actualSize2 = mmkv.actualSize
            print("actualSize after same write = $actualSize2")
            if (actualSize2 != actualSize) {
                print("FAIL: size changed for same value!")
            } else {
                print("OK: size unchanged for same value")
            }

            mmkv.encodeInt(key, v * 23) // different value
            print("actualSize after different write = ${mmkv.actualSize}")
            print("v = ${mmkv.decodeInt(key)}")
        }

        // string test
        print("")
        run {
            val key = "string"
            val v = "w012A\uD83C\uDFCA\uD83C\uDFFBgood"
            mmkv.encodeString(key, v)
            val actualSize = mmkv.actualSize
            print("actualSize = $actualSize")
            print("v = ${mmkv.decodeString(key)}")

            mmkv.encodeString(key, v) // same value
            val actualSize2 = mmkv.actualSize
            print("actualSize after same write = $actualSize2")
            if (actualSize2 != actualSize) {
                print("FAIL: size changed for same value!")
            } else {
                print("OK: size unchanged for same value")
            }

            mmkv.encodeString(key, "temp data \uD83D\uDC69\uD83C\uDFFB\u200D\uD83C\uDFEB")
            print("actualSize after different write = ${mmkv.actualSize}")
            print("v = ${mmkv.decodeString(key)}")
        }

        return log.toList()
    }

    // endregion

    // region Backup & Restore Test

    fun testBackupAndRestore(): List<String> {
        log.clear()
        print("=== Backup & Restore Test ===")

        val rootDir = MMKV.rootDir()
        val parentDir = rootDir.substringBeforeLast('/')
        val backupRootDir = "$parentDir/mmkv_backup_3"
        val mmapID = "test/AES"
        val cryptKey = "Tencent MMKV"
        val otherDir = "$parentDir/mmkv_3"

        testMMKV(mmapID, cryptKey, false, false, otherDir)

        val ret = MMKV.backupOneToDirectory(mmapID, backupRootDir, otherDir)
        print("")
        print("backup one [$mmapID] return: $ret")

        val backupAllDir = "$parentDir/mmkv_backup"
        val count = MMKV.backupAllToDirectory(backupAllDir)
        print("backup all count: $count")

        // Restore
        print("")
        print("--- Restore ---")
        val kv = MMKV.mmkvWithID(mmapID, MMKVConfig(cryptKey = cryptKey, rootPath = otherDir))
        kv.encodeString("test_restore", "value before restore")
        print("before restore [${kv.mmapID}] allKeys: ${kv.allKeys}")

        val restoreRet = MMKV.restoreOneFromDirectory(mmapID, backupRootDir, otherDir)
        print("restore one [${kv.mmapID}] ret = $restoreRet")
        if (restoreRet) {
            print("after restore [${kv.mmapID}] allKeys: ${kv.allKeys}")
        }

        val restoreAllDir = "$parentDir/mmkv_backup"
        val restoreCount = MMKV.restoreAllFromDirectory(restoreAllDir)
        print("restore all count $restoreCount")
        if (restoreCount > 0) {
            val defaultKV = MMKV.defaultMMKV()
            print("check restore [${defaultKV.mmapID}] allKeys: ${defaultKV.allKeys}")
        }

        return log.toList()
    }

    // endregion

    // region Remove Storage & Check Exist Test

    fun testRemoveStorageAndCheckExist(): List<String> {
        log.clear()
        print("=== Remove Storage & Check Exist Test ===")

        var mmapID = "test_remove"
        run {
            val mmkv = MMKV.mmkvWithID(mmapID, MMKVConfig(mode = MMKVMode.MULTI_PROCESS))
            mmkv.encodeBool("bool", true)
        }
        print("check exist = ${MMKV.checkExist(mmapID)}")
        MMKV.removeStorage(mmapID)
        print("after remove, check exist = ${MMKV.checkExist(mmapID)}")
        run {
            val mmkv = MMKV.mmkvWithID(mmapID, MMKVConfig(mode = MMKVMode.MULTI_PROCESS))
            if (mmkv.count != 0L) {
                print("storage not successfully removed")
            } else {
                print("storage successfully removed")
            }
        }

        print("")
        mmapID = "test_remove/sg"
        val rootDir = "${MMKV.rootDir()}_sg"
        var mmkv = MMKV.mmkvWithID(mmapID, MMKVConfig(rootPath = rootDir))
        mmkv.encodeBool("bool", true)
        print("check exist = ${MMKV.checkExist(mmapID, rootDir)}")
        MMKV.removeStorage(mmapID, rootDir)
        print("after remove, check exist = ${MMKV.checkExist(mmapID, rootDir)}")
        mmkv = MMKV.mmkvWithID(mmapID, MMKVConfig(rootPath = rootDir))
        if (mmkv.count != 0L) {
            print("storage not successfully removed")
        } else {
            print("storage successfully removed")
        }

        return log.toList()
    }

    // endregion

    // region NameSpace Test

    fun testNameSpace(): List<String> {
        log.clear()
        print("=== NameSpace Test ===")

        val rootDir = "${MMKV.rootDir()}_ns"
        val ns = MMKVNameSpace.of(rootDir)
        print("namespace rootDir: ${ns.rootDir}")

        // Create an instance within the namespace
        val mmapID = "ns_test"
        val kv = ns.mmkvWithID(mmapID)
        kv.encodeString("ns_key", "namespace value")
        kv.encodeInt("ns_int", 42)
        kv.encodeBool("ns_bool", true)
        print("ns_key = ${kv.decodeString("ns_key")}")
        print("ns_int = ${kv.decodeInt("ns_int")}")
        print("ns_bool = ${kv.decodeBool("ns_bool")}")
        print("count = ${kv.count}")

        // checkExist / isFileValid within namespace
        print("checkExist(ns_test) = ${ns.checkExist(mmapID)}")

        // backup within namespace
        val backupDir = "${MMKV.rootDir()}_ns_backup"
        val backupRet = ns.backupOneToDirectory(mmapID, backupDir)
        print("backup one = $backupRet")

        // clear, then restore
        kv.clearAll()
        print("count after clearAll = ${kv.count}")

        val restoreRet = ns.restoreOneFromDirectory(mmapID, backupDir)
        print("restore one = $restoreRet")
        print("count after restore = ${kv.count}")
        print("ns_key after restore = ${kv.decodeString("ns_key")}")

        // removeStorage within namespace
        kv.close()
        val removeRet = ns.removeStorage(mmapID)
        print("removeStorage = $removeRet")
        print("checkExist after remove = ${ns.checkExist(mmapID)}")

        // default namespace
        val defaultNs = MMKVNameSpace.default()
        print("default namespace rootDir: ${defaultNs.rootDir}")
        defaultNs.close()

        ns.close()
        print("")
        print("namespace test passed")
        return log.toList()
    }

    // endregion
}
