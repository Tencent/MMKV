/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2026 THL A29 Limited, a Tencent company.
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

package com.tencent.mmkv.kmp

import com.sun.jna.*
import com.sun.jna.ptr.IntByReference
import com.sun.jna.ptr.LongByReference

// region JNA interface mapping the C bridge

@Suppress("FunctionName")
private interface MMKVLib : Library {
    fun mmkv_initialize(rootDir: String, logLevel: Int)
    fun mmkv_on_exit()

    fun mmkv_default(config: JnaMMKVConfig.ByValue): Pointer?
    fun mmkv_with_id(mmapID: String, config: JnaMMKVConfig.ByValue): Pointer?
    fun mmkv_mmap_id(handle: Pointer): String?
    fun mmkv_close(handle: Pointer)

    fun mmkv_encode_bool(handle: Pointer, key: String, value: Boolean): Boolean
    fun mmkv_encode_bool_v2(handle: Pointer, key: String, value: Boolean, expireDuration: Int): Boolean
    fun mmkv_encode_int32(handle: Pointer, key: String, value: Int): Boolean
    fun mmkv_encode_int32_v2(handle: Pointer, key: String, value: Int, expireDuration: Int): Boolean
    fun mmkv_encode_int64(handle: Pointer, key: String, value: Long): Boolean
    fun mmkv_encode_int64_v2(handle: Pointer, key: String, value: Long, expireDuration: Int): Boolean
    fun mmkv_encode_float(handle: Pointer, key: String, value: Float): Boolean
    fun mmkv_encode_float_v2(handle: Pointer, key: String, value: Float, expireDuration: Int): Boolean
    fun mmkv_encode_double(handle: Pointer, key: String, value: Double): Boolean
    fun mmkv_encode_double_v2(handle: Pointer, key: String, value: Double, expireDuration: Int): Boolean
    fun mmkv_encode_string(handle: Pointer, key: String, value: String?): Boolean
    fun mmkv_encode_string_v2(handle: Pointer, key: String, value: String?, expireDuration: Int): Boolean
    fun mmkv_encode_bytes(handle: Pointer, key: String, value: Pointer?, length: Long): Boolean
    fun mmkv_encode_bytes_v2(handle: Pointer, key: String, value: Pointer?, length: Long, expireDuration: Int): Boolean

    fun mmkv_decode_bool(handle: Pointer, key: String, defaultValue: Boolean): Boolean
    fun mmkv_decode_int32(handle: Pointer, key: String, defaultValue: Int): Int
    fun mmkv_decode_int64(handle: Pointer, key: String, defaultValue: Long): Long
    fun mmkv_decode_float(handle: Pointer, key: String, defaultValue: Float): Float
    fun mmkv_decode_double(handle: Pointer, key: String, defaultValue: Double): Double
    fun mmkv_decode_string(handle: Pointer, key: String): Pointer?
    fun mmkv_decode_bytes(handle: Pointer, key: String, lengthPtr: LongByReference): Pointer?

    fun mmkv_rekey(handle: Pointer, cryptKey: String?, aes256: Boolean): Boolean
    fun mmkv_crypt_key(handle: Pointer, lengthPtr: IntByReference): Pointer?
    fun mmkv_check_reset_crypt_key(handle: Pointer, cryptKey: String?, aes256: Boolean)

    fun mmkv_all_keys(handle: Pointer, lengthPtr: LongByReference, filterExpire: Boolean): Pointer?
    fun mmkv_contains_key(handle: Pointer, key: String): Boolean
    fun mmkv_count(handle: Pointer, filterExpire: Boolean): Long
    fun mmkv_total_size(handle: Pointer): Long
    fun mmkv_actual_size(handle: Pointer): Long

    fun mmkv_remove_value(handle: Pointer, key: String)
    fun mmkv_remove_values(handle: Pointer, keyArray: Array<String>, count: Long)
    fun mmkv_clear_all(handle: Pointer, keepSpace: Boolean)
    fun mmkv_import_from(handle: Pointer, srcHandle: Pointer): Long

    fun mmkv_sync(handle: Pointer, sync: Boolean)
    fun mmkv_clear_memory_cache(handle: Pointer)
    fun mmkv_trim(handle: Pointer)

    fun mmkv_backup_one(mmapID: String, dstDir: String, srcDir: String?): Boolean
    fun mmkv_restore_one(mmapID: String, srcDir: String, dstDir: String?): Boolean
    fun mmkv_backup_all(dstDir: String, srcDir: String?): Long
    fun mmkv_restore_all(srcDir: String, dstDir: String?): Long

    fun mmkv_enable_auto_expire(handle: Pointer, expireDuration: Int): Boolean
    fun mmkv_disable_auto_expire(handle: Pointer): Boolean
    fun mmkv_enable_compare_before_set(handle: Pointer): Boolean
    fun mmkv_disable_compare_before_set(handle: Pointer): Boolean

    fun mmkv_page_size(): Int
    fun mmkv_version(): String?
    fun mmkv_root_dir(): String?

    fun mmkv_remove_storage(mmapID: String, rootPath: String?): Boolean
    fun mmkv_check_exist(mmapID: String, rootPath: String?): Boolean
    fun mmkv_is_multi_process(handle: Pointer): Boolean
    fun mmkv_is_read_only(handle: Pointer): Boolean
    fun mmkv_is_file_valid(mmapID: String, rootPath: String?): Boolean
    fun mmkv_check_content_changed(handle: Pointer)

    fun mmkv_free(ptr: Pointer)

    companion object {
        val INSTANCE: MMKVLib = Native.load("mmkv-kmp", MMKVLib::class.java)
    }
}

// endregion

// region JNA struct for MMKVConfig_t

@Suppress("unused")
@Structure.FieldOrder(
    "mode", "cryptKey", "aes256", "rootPath", "expectedCapacity",
    "enableKeyExpire", "expiredInSeconds", "enableCompareBeforeSet",
    "recover", "itemSizeLimit"
)
internal open class JnaMMKVConfig : Structure() {
    @JvmField var mode: Int = MMKVMode.SINGLE_PROCESS
    @JvmField var cryptKey: String? = null
    @JvmField var aes256: Boolean = false
    @JvmField var rootPath: String? = null
    @JvmField var expectedCapacity: Long = 0
    @JvmField var enableKeyExpire: Int = -1
    @JvmField var expiredInSeconds: Int = 0
    @JvmField var enableCompareBeforeSet: Boolean = false
    @JvmField var recover: Int = -1
    @JvmField var itemSizeLimit: Int = 0

    class ByValue : JnaMMKVConfig(), Structure.ByValue
}

// endregion

// region Platform-specific initialization (JVM desktop, no Context needed)

/**
 * Initialize MMKV with customize settings.
 * Call this before calling any other MMKV methods.
 *
 * @param rootDir The root dir of MMKV.
 * @param logLevel MMKVLogInfo by default, MMKVLogNone to disable all logging.
 * @return Root dir of MMKV.
 */
fun MMKV.Companion.initialize(
    rootDir: String,
    logLevel: MMKVLogLevel = MMKVLogLevel.Info,
): String {
    MMKVLib.INSTANCE.mmkv_initialize(rootDir, logLevel.toNativeLevel())
    return MMKVLib.INSTANCE.mmkv_root_dir() ?: ""
}

// endregion

actual class MMKV internal constructor(private val handle: Pointer) {

    actual companion object {
        private val lib = MMKVLib.INSTANCE

        actual fun onExit() = lib.mmkv_on_exit()

        actual fun setLogLevel(level: MMKVLogLevel) {
            // Log level is set during initialization on desktop.
        }

        actual fun defaultMMKV(): MMKV = MMKV(lib.mmkv_default(buildDefaultConfig())!!)

        actual fun defaultMMKV(config: MMKVConfig): MMKV = MMKV(lib.mmkv_default(config.toJna())!!)

        actual fun mmkvWithID(mmapID: String): MMKV = MMKV(lib.mmkv_with_id(mmapID, buildDefaultConfig())!!)

        actual fun mmkvWithID(mmapID: String, config: MMKVConfig): MMKV = MMKV(lib.mmkv_with_id(mmapID, config.toJna())!!)

        actual fun pageSize(): Int = lib.mmkv_page_size()

        actual fun version(): String = lib.mmkv_version() ?: ""

        actual fun rootDir(): String = lib.mmkv_root_dir() ?: ""

        actual fun backupOneToDirectory(mmapID: String, dstDir: String, rootPath: String?): Boolean =
            lib.mmkv_backup_one(mmapID, dstDir, rootPath)

        actual fun restoreOneFromDirectory(mmapID: String, srcDir: String, rootPath: String?): Boolean =
            lib.mmkv_restore_one(mmapID, srcDir, rootPath)

        actual fun backupAllToDirectory(dstDir: String): Long = lib.mmkv_backup_all(dstDir, null)

        actual fun restoreAllFromDirectory(srcDir: String): Long = lib.mmkv_restore_all(srcDir, null)

        actual fun isFileValid(mmapID: String, rootPath: String?): Boolean = lib.mmkv_is_file_valid(mmapID, rootPath)

        actual fun removeStorage(mmapID: String, rootPath: String?): Boolean = lib.mmkv_remove_storage(mmapID, rootPath)

        actual fun checkExist(mmapID: String, rootPath: String?): Boolean = lib.mmkv_check_exist(mmapID, rootPath)

        actual fun registerHandler(handler: MMKVHandler) {
            // JNA callback registration not implemented yet.
        }

        actual fun unRegisterHandler() {
            // JNA callback registration not implemented yet.
        }
    }

    // region Properties

    actual val mmapID: String get() = lib.mmkv_mmap_id(handle) ?: ""
    actual val isMultiProcess: Boolean get() = lib.mmkv_is_multi_process(handle)
    actual val isReadOnly: Boolean get() = lib.mmkv_is_read_only(handle)
    actual val totalSize: Long get() = lib.mmkv_total_size(handle)
    actual val actualSize: Long get() = lib.mmkv_actual_size(handle)
    actual val count: Long get() = lib.mmkv_count(handle, false)

    actual val allKeys: List<String>
        get() {
            val lengthPtr = LongByReference()
            val ptr = lib.mmkv_all_keys(handle, lengthPtr, false) ?: return emptyList()
            return readAndFreeStringArray(ptr, lengthPtr.value.toInt())
        }

    actual val cryptKey: String?
        get() {
            val lengthPtr = IntByReference()
            val ptr = lib.mmkv_crypt_key(handle, lengthPtr) ?: return null
            val length = lengthPtr.value
            if (length == 0) { lib.mmkv_free(ptr); return null }
            val bytes = ptr.getByteArray(0, length)
            lib.mmkv_free(ptr)
            return String(bytes)
        }

    // endregion

    // region Encode

    actual fun encodeBool(key: String, value: Boolean): Boolean = lib.mmkv_encode_bool(handle, key, value)
    actual fun encodeBool(key: String, value: Boolean, expireDuration: UInt): Boolean = lib.mmkv_encode_bool_v2(handle, key, value, expireDuration.toInt())
    actual fun encodeInt(key: String, value: Int): Boolean = lib.mmkv_encode_int32(handle, key, value)
    actual fun encodeInt(key: String, value: Int, expireDuration: UInt): Boolean = lib.mmkv_encode_int32_v2(handle, key, value, expireDuration.toInt())
    actual fun encodeLong(key: String, value: Long): Boolean = lib.mmkv_encode_int64(handle, key, value)
    actual fun encodeLong(key: String, value: Long, expireDuration: UInt): Boolean = lib.mmkv_encode_int64_v2(handle, key, value, expireDuration.toInt())
    actual fun encodeFloat(key: String, value: Float): Boolean = lib.mmkv_encode_float(handle, key, value)
    actual fun encodeFloat(key: String, value: Float, expireDuration: UInt): Boolean = lib.mmkv_encode_float_v2(handle, key, value, expireDuration.toInt())
    actual fun encodeDouble(key: String, value: Double): Boolean = lib.mmkv_encode_double(handle, key, value)
    actual fun encodeDouble(key: String, value: Double, expireDuration: UInt): Boolean = lib.mmkv_encode_double_v2(handle, key, value, expireDuration.toInt())
    actual fun encodeString(key: String, value: String): Boolean = lib.mmkv_encode_string(handle, key, value)
    actual fun encodeString(key: String, value: String, expireDuration: UInt): Boolean = lib.mmkv_encode_string_v2(handle, key, value, expireDuration.toInt())

    actual fun encodeBytes(key: String, value: ByteArray): Boolean {
        if (value.isEmpty()) return lib.mmkv_encode_bytes(handle, key, null, 0)
        val mem = Memory(value.size.toLong())
        mem.write(0, value, 0, value.size)
        return lib.mmkv_encode_bytes(handle, key, mem, value.size.toLong())
    }

    actual fun encodeBytes(key: String, value: ByteArray, expireDuration: UInt): Boolean {
        if (value.isEmpty()) return lib.mmkv_encode_bytes_v2(handle, key, null, 0, expireDuration.toInt())
        val mem = Memory(value.size.toLong())
        mem.write(0, value, 0, value.size)
        return lib.mmkv_encode_bytes_v2(handle, key, mem, value.size.toLong(), expireDuration.toInt())
    }

    // endregion

    // region Decode

    actual fun decodeBool(key: String, defaultValue: Boolean): Boolean = lib.mmkv_decode_bool(handle, key, defaultValue)
    actual fun decodeInt(key: String, defaultValue: Int): Int = lib.mmkv_decode_int32(handle, key, defaultValue)
    actual fun decodeLong(key: String, defaultValue: Long): Long = lib.mmkv_decode_int64(handle, key, defaultValue)
    actual fun decodeFloat(key: String, defaultValue: Float): Float = lib.mmkv_decode_float(handle, key, defaultValue)
    actual fun decodeDouble(key: String, defaultValue: Double): Double = lib.mmkv_decode_double(handle, key, defaultValue)

    actual fun decodeString(key: String, defaultValue: String?): String? {
        val ptr = lib.mmkv_decode_string(handle, key) ?: return defaultValue
        val result = ptr.getString(0)
        lib.mmkv_free(ptr)
        return result
    }

    actual fun decodeBytes(key: String): ByteArray? {
        val lengthPtr = LongByReference()
        val ptr = lib.mmkv_decode_bytes(handle, key, lengthPtr) ?: return null
        val length = lengthPtr.value.toInt()
        if (length == 0) { lib.mmkv_free(ptr); return null }
        val bytes = ptr.getByteArray(0, length)
        lib.mmkv_free(ptr)
        return bytes
    }

    // endregion

    // region Key management

    actual fun containsKey(key: String): Boolean = lib.mmkv_contains_key(handle, key)
    actual fun countNonExpiredKeys(): Long = lib.mmkv_count(handle, true)

    actual fun allNonExpiredKeys(): List<String> {
        val lengthPtr = LongByReference()
        val ptr = lib.mmkv_all_keys(handle, lengthPtr, true) ?: return emptyList()
        return readAndFreeStringArray(ptr, lengthPtr.value.toInt())
    }

    actual fun removeValueForKey(key: String) = lib.mmkv_remove_value(handle, key)

    actual fun removeValuesForKeys(keys: List<String>) {
        if (keys.isEmpty()) return
        lib.mmkv_remove_values(handle, keys.toTypedArray(), keys.size.toLong())
    }

    actual fun clearAll() = lib.mmkv_clear_all(handle, false)
    actual fun clearAllKeepSpace() = lib.mmkv_clear_all(handle, true)

    // endregion

    // region Encryption

    actual fun reKey(newKey: String?, aes256: Boolean): Boolean = lib.mmkv_rekey(handle, newKey, aes256)
    actual fun checkReSetCryptKey(cryptKey: String?, aes256: Boolean) = lib.mmkv_check_reset_crypt_key(handle, cryptKey, aes256)

    // endregion

    // region Utility

    actual fun sync() = lib.mmkv_sync(handle, true)
    actual fun async() = lib.mmkv_sync(handle, false)
    actual fun trim() = lib.mmkv_trim(handle)
    actual fun close() = lib.mmkv_close(handle)
    actual fun clearMemoryCache() = lib.mmkv_clear_memory_cache(handle)
    actual fun importFrom(source: MMKV): Long = lib.mmkv_import_from(handle, source.handle)
    actual fun enableAutoKeyExpire(expiredInSeconds: UInt): Boolean = lib.mmkv_enable_auto_expire(handle, expiredInSeconds.toInt())
    actual fun disableAutoKeyExpire(): Boolean = lib.mmkv_disable_auto_expire(handle)
    actual fun enableCompareBeforeSet(): Boolean = lib.mmkv_enable_compare_before_set(handle)
    actual fun disableCompareBeforeSet(): Boolean = lib.mmkv_disable_compare_before_set(handle)
    actual fun checkContentChanged() = lib.mmkv_check_content_changed(handle)

    // endregion
}

// region Private helpers

private fun MMKVLogLevel.toNativeLevel(): Int = when (this) {
    MMKVLogLevel.Debug -> 0
    MMKVLogLevel.Info -> 1
    MMKVLogLevel.Warning -> 2
    MMKVLogLevel.Error -> 3
    MMKVLogLevel.None -> 4
}

private fun buildDefaultConfig(): JnaMMKVConfig.ByValue = JnaMMKVConfig.ByValue()

private fun MMKVConfig.toJna(): JnaMMKVConfig.ByValue {
    val cfg = JnaMMKVConfig.ByValue()
    cfg.mode = mode
    cfg.cryptKey = cryptKey
    cfg.aes256 = aes256
    cfg.rootPath = rootPath
    cfg.expectedCapacity = expectedCapacity
    cfg.enableKeyExpire = when (enableKeyExpire) {
        null -> -1; false -> 0; true -> 1
    }
    cfg.expiredInSeconds = expiredInSeconds.toInt()
    cfg.enableCompareBeforeSet = enableCompareBeforeSet
    cfg.recover = when (recover) {
        null -> -1
        MMKVRecoverStrategic.OnErrorDiscard -> 0
        MMKVRecoverStrategic.OnErrorRecover -> 1
    }
    cfg.itemSizeLimit = itemSizeLimit.toInt()
    return cfg
}

private fun readAndFreeStringArray(ptr: Pointer, count: Int): List<String> {
    val lib = MMKVLib.INSTANCE
    val pointers = ptr.getPointerArray(0, count)
    val result = pointers.map { it?.getString(0) ?: "" }
    for (p in pointers) {
        if (p != null) lib.mmkv_free(p)
    }
    lib.mmkv_free(ptr)
    return result
}

// endregion
