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

import kotlinx.cinterop.*
import mmkv.*

// region Platform-specific initialization (no Context needed, same as Darwin)

/**
 * Initialize MMKV with customize settings.
 * Call this in main thread, before calling any other MMKV methods.
 *
 * @param rootDir The root dir of MMKV.
 * @param logLevel MMKVLogInfo by default, MMKVLogNone to disable all logging.
 * @param handler The unified callback handler for MMKV.
 * @return Root dir of MMKV.
 */
@OptIn(ExperimentalForeignApi::class)
fun MMKV.Companion.initialize(
    rootDir: String,
    logLevel: MMKVLogLevel = MMKVLogLevel.Info,
    handler: MMKVHandler? = null,
): String {
    if (handler != null) {
        NativeDesktopMMKVHandlerHolder.handler = handler
        val callbacks = cValue<MMKVHandler_t> {
            log = NativeDesktopMMKVHandlerHolder.logCallback
            error = NativeDesktopMMKVHandlerHolder.errorCallback
            contentChange = NativeDesktopMMKVHandlerHolder.contentChangeCallback
            contentLoad = NativeDesktopMMKVHandlerHolder.contentLoadCallback
        }
        mmkv_initialize_with_handler(rootDir, logLevel.toNativeLevel(), callbacks)
    } else {
        mmkv_initialize(rootDir, logLevel.toNativeLevel())
    }
    return mmkv_root_dir()?.toKString() ?: ""
}

// endregion

@OptIn(ExperimentalForeignApi::class)
actual class MMKV internal constructor(private val handle: COpaquePointer) {

    actual companion object {
        actual fun onExit() {
            mmkv_on_exit()
        }

        actual fun setLogLevel(level: MMKVLogLevel) {
            // C bridge doesn't have a direct setLogLevel function;
            // log level is set during initialization.
            // Re-initialize is not ideal, so this is a no-op on native desktop.
            // Users should set log level during initialize().
        }

        actual fun defaultMMKV(): MMKV {
            val config = buildDefaultNativeConfig()
            return MMKV(mmkv_default(config)!!)
        }

        actual fun defaultMMKV(config: MMKVConfig): MMKV {
            return withNativeConfig(config) { cfg ->
                MMKV(mmkv_default(cfg)!!)
            }
        }

        actual fun mmkvWithID(mmapID: String): MMKV {
            val config = buildDefaultNativeConfig()
            return MMKV(mmkv_with_id(mmapID, config)!!)
        }

        actual fun mmkvWithID(mmapID: String, config: MMKVConfig): MMKV {
            return withNativeConfig(config) { cfg ->
                MMKV(mmkv_with_id(mmapID, cfg)!!)
            }
        }

        actual fun pageSize(): Int {
            return mmkv_page_size()
        }

        actual fun version(): String {
            return mmkv_version()?.toKString() ?: ""
        }

        actual fun rootDir(): String {
            return mmkv_root_dir()?.toKString() ?: ""
        }

        actual fun backupOneToDirectory(mmapID: String, dstDir: String, rootPath: String?): Boolean {
            return mmkv_backup_one(mmapID, dstDir, rootPath)
        }

        actual fun restoreOneFromDirectory(mmapID: String, srcDir: String, rootPath: String?): Boolean {
            return mmkv_restore_one(mmapID, srcDir, rootPath)
        }

        actual fun backupAllToDirectory(dstDir: String): Long {
            return mmkv_backup_all(dstDir, null).toLong()
        }

        actual fun restoreAllFromDirectory(srcDir: String): Long {
            return mmkv_restore_all(srcDir, null).toLong()
        }

        actual fun isFileValid(mmapID: String, rootPath: String?): Boolean {
            return mmkv_is_file_valid(mmapID, rootPath)
        }

        actual fun removeStorage(mmapID: String, rootPath: String?): Boolean {
            return mmkv_remove_storage(mmapID, rootPath)
        }

        actual fun checkExist(mmapID: String, rootPath: String?): Boolean {
            return mmkv_check_exist(mmapID, rootPath)
        }

        actual fun registerHandler(handler: MMKVHandler) {
            NativeDesktopMMKVHandlerHolder.handler = handler
            // Note: handler must be set during initialization via initialize() on native desktop.
            // This is provided for API compatibility.
        }

        actual fun unRegisterHandler() {
            NativeDesktopMMKVHandlerHolder.handler = null
        }
    }

    // region Properties

    actual val mmapID: String
        get() = mmkv_mmap_id(handle)?.toKString() ?: ""

    actual val isMultiProcess: Boolean
        get() = mmkv_is_multi_process(handle)

    actual val isReadOnly: Boolean
        get() = mmkv_is_read_only(handle)

    actual val totalSize: Long
        get() = mmkv_total_size(handle).toLong()

    actual val actualSize: Long
        get() = mmkv_actual_size(handle).toLong()

    actual val count: Long
        get() = mmkv_count(handle, false).toLong()

    actual val allKeys: List<String>
        get() = memScoped {
            val lengthPtr = alloc<ULongVar>()
            val keys = mmkv_all_keys(handle, lengthPtr.ptr, false) ?: return@memScoped emptyList()
            val count = lengthPtr.value.toInt()
            val result = (0 until count).map { i ->
                keys[i]?.toKString() ?: ""
            }
            // Free each string and the array
            for (i in 0 until count) {
                mmkv_free(keys[i])
            }
            mmkv_free(keys)
            result
        }

    actual val cryptKey: String?
        get() {
            memScoped {
                val lengthPtr = alloc<UIntVar>()
                val ptr = mmkv_crypt_key(handle, lengthPtr.ptr) ?: return null
                val length = lengthPtr.value.toInt()
                if (length == 0) {
                    mmkv_free(ptr)
                    return null
                }
                val bytes = ByteArray(length)
                bytes.usePinned { pinned ->
                    platform.posix.memcpy(pinned.addressOf(0), ptr, length.toULong())
                }
                mmkv_free(ptr)
                return bytes.decodeToString()
            }
        }

    // endregion

    // region Encode

    actual fun encodeBool(key: String, value: Boolean): Boolean = mmkv_encode_bool(handle, key, value)
    actual fun encodeBool(key: String, value: Boolean, expireDuration: UInt): Boolean = mmkv_encode_bool_v2(handle, key, value, expireDuration)
    actual fun encodeInt(key: String, value: Int): Boolean = mmkv_encode_int32(handle, key, value)
    actual fun encodeInt(key: String, value: Int, expireDuration: UInt): Boolean = mmkv_encode_int32_v2(handle, key, value, expireDuration)
    actual fun encodeLong(key: String, value: Long): Boolean = mmkv_encode_int64(handle, key, value)
    actual fun encodeLong(key: String, value: Long, expireDuration: UInt): Boolean = mmkv_encode_int64_v2(handle, key, value, expireDuration)
    actual fun encodeFloat(key: String, value: Float): Boolean = mmkv_encode_float(handle, key, value)
    actual fun encodeFloat(key: String, value: Float, expireDuration: UInt): Boolean = mmkv_encode_float_v2(handle, key, value, expireDuration)
    actual fun encodeDouble(key: String, value: Double): Boolean = mmkv_encode_double(handle, key, value)
    actual fun encodeDouble(key: String, value: Double, expireDuration: UInt): Boolean = mmkv_encode_double_v2(handle, key, value, expireDuration)
    actual fun encodeString(key: String, value: String): Boolean = mmkv_encode_string(handle, key, value)
    actual fun encodeString(key: String, value: String, expireDuration: UInt): Boolean = mmkv_encode_string_v2(handle, key, value, expireDuration)

    @OptIn(ExperimentalForeignApi::class)
    actual fun encodeBytes(key: String, value: ByteArray): Boolean {
        if (value.isEmpty()) return mmkv_encode_bytes(handle, key, null, 0)
        return value.usePinned { pinned ->
            mmkv_encode_bytes(handle, key, pinned.addressOf(0), value.size.toLong())
        }
    }

    @OptIn(ExperimentalForeignApi::class)
    actual fun encodeBytes(key: String, value: ByteArray, expireDuration: UInt): Boolean {
        if (value.isEmpty()) return mmkv_encode_bytes_v2(handle, key, null, 0, expireDuration)
        return value.usePinned { pinned ->
            mmkv_encode_bytes_v2(handle, key, pinned.addressOf(0), value.size.toLong(), expireDuration)
        }
    }

    // endregion

    // region Decode

    actual fun decodeBool(key: String, defaultValue: Boolean): Boolean = mmkv_decode_bool(handle, key, defaultValue)
    actual fun decodeInt(key: String, defaultValue: Int): Int = mmkv_decode_int32(handle, key, defaultValue)
    actual fun decodeLong(key: String, defaultValue: Long): Long = mmkv_decode_int64(handle, key, defaultValue)
    actual fun decodeFloat(key: String, defaultValue: Float): Float = mmkv_decode_float(handle, key, defaultValue)
    actual fun decodeDouble(key: String, defaultValue: Double): Double = mmkv_decode_double(handle, key, defaultValue)
    actual fun decodeString(key: String, defaultValue: String?): String? {
        val ptr = mmkv_decode_string(handle, key) ?: return defaultValue
        val result = ptr.toKString()
        mmkv_free(ptr)
        return result
    }

    @OptIn(ExperimentalForeignApi::class)
    actual fun decodeBytes(key: String): ByteArray? {
        memScoped {
            val lengthPtr = alloc<ULongVar>()
            val ptr = mmkv_decode_bytes(handle, key, lengthPtr.ptr) ?: return null
            val length = lengthPtr.value.toInt()
            if (length == 0) {
                mmkv_free(ptr)
                return null
            }
            val bytes = ByteArray(length)
            bytes.usePinned { pinned ->
                platform.posix.memcpy(pinned.addressOf(0), ptr, length.toULong())
            }
            mmkv_free(ptr)
            return bytes
        }
    }

    // endregion

    // region Key management

    actual fun containsKey(key: String): Boolean = mmkv_contains_key(handle, key)

    actual fun countNonExpiredKeys(): Long = mmkv_count(handle, true).toLong()

    actual fun allNonExpiredKeys(): List<String> {
        memScoped {
            val lengthPtr = alloc<ULongVar>()
            val keys = mmkv_all_keys(handle, lengthPtr.ptr, true) ?: return emptyList()
            val count = lengthPtr.value.toInt()
            val result = (0 until count).map { i ->
                keys[i]?.toKString() ?: ""
            }
            for (i in 0 until count) {
                mmkv_free(keys[i])
            }
            mmkv_free(keys)
            return result
        }
    }

    actual fun removeValueForKey(key: String) = mmkv_remove_value(handle, key)

    @OptIn(ExperimentalForeignApi::class)
    actual fun removeValuesForKeys(keys: List<String>) {
        if (keys.isEmpty()) return
        memScoped {
            val cArray = allocArray<CPointerVar<ByteVar>>(keys.size)
            keys.forEachIndexed { index, key ->
                cArray[index] = key.cstr.ptr
            }
            mmkv_remove_values(handle, cArray, keys.size.toULong())
        }
    }

    actual fun clearAll() = mmkv_clear_all(handle, false)

    actual fun clearAllKeepSpace() = mmkv_clear_all(handle, true)

    // endregion

    // region Encryption

    actual fun reKey(newKey: String?, aes256: Boolean): Boolean = mmkv_rekey(handle, newKey, aes256)

    actual fun checkReSetCryptKey(cryptKey: String?, aes256: Boolean) = mmkv_check_reset_crypt_key(handle, cryptKey, aes256)

    // endregion

    // region Utility

    actual fun sync() = mmkv_sync(handle, true)
    actual fun async() = mmkv_sync(handle, false)
    actual fun trim() = mmkv_trim(handle)
    actual fun close() = mmkv_close(handle)
    actual fun clearMemoryCache() = mmkv_clear_memory_cache(handle)

    actual fun importFrom(source: MMKV): Long = mmkv_import_from(handle, source.handle).toLong()

    actual fun enableAutoKeyExpire(expiredInSeconds: UInt): Boolean = mmkv_enable_auto_expire(handle, expiredInSeconds)
    actual fun disableAutoKeyExpire(): Boolean = mmkv_disable_auto_expire(handle)

    actual fun enableCompareBeforeSet(): Boolean = mmkv_enable_compare_before_set(handle)
    actual fun disableCompareBeforeSet(): Boolean = mmkv_disable_compare_before_set(handle)

    actual fun checkContentChanged() = mmkv_check_content_changed(handle)

    // endregion
}

// region Helper functions

@OptIn(ExperimentalForeignApi::class)
private fun MMKVLogLevel.toNativeLevel(): Int = when (this) {
    MMKVLogLevel.Debug -> 0
    MMKVLogLevel.Info -> 1
    MMKVLogLevel.Warning -> 2
    MMKVLogLevel.Error -> 3
    MMKVLogLevel.None -> 4
}

@OptIn(ExperimentalForeignApi::class)
private fun nativeLogLevelToCommon(level: Int): MMKVLogLevel = when (level) {
    0 -> MMKVLogLevel.Debug
    1 -> MMKVLogLevel.Info
    2 -> MMKVLogLevel.Warning
    3 -> MMKVLogLevel.Error
    4 -> MMKVLogLevel.None
    else -> MMKVLogLevel.Info
}

@OptIn(ExperimentalForeignApi::class)
private fun buildDefaultNativeConfig(): CValue<MMKVConfig_t> = cValue {
    mode = MMKVMode.SINGLE_PROCESS
    cryptKey = null
    aes256 = false
    rootPath = null
    expectedCapacity = 0u
    enableKeyExpire = -1  // leave default
    expiredInSeconds = 0u
    enableCompareBeforeSet = false
    recover = -1  // leave default
    itemSizeLimit = 0u
}

/**
 * Helper that creates a native config inside a memScoped block so that
 * String fields survive as C pointers until the MMKV C call completes.
 */
@OptIn(ExperimentalForeignApi::class)
private inline fun <R> withNativeConfig(config: MMKVConfig, block: (CValue<MMKVConfig_t>) -> R): R = memScoped {
    val cfg = alloc<MMKVConfig_t>()
    cfg.mode = config.mode
    cfg.cryptKey = config.cryptKey?.cstr?.getPointer(this)
    cfg.aes256 = config.aes256
    cfg.rootPath = config.rootPath?.cstr?.getPointer(this)
    cfg.expectedCapacity = config.expectedCapacity.toULong()
    cfg.enableKeyExpire = when (config.enableKeyExpire) {
        null -> -1
        false -> 0
        true -> 1
    }
    cfg.expiredInSeconds = config.expiredInSeconds
    cfg.enableCompareBeforeSet = config.enableCompareBeforeSet
    cfg.recover = when (config.recover) {
        null -> -1
        MMKVRecoverStrategic.OnErrorDiscard -> 0
        MMKVRecoverStrategic.OnErrorRecover -> 1
    }
    cfg.itemSizeLimit = config.itemSizeLimit
    block(cfg.readValue())
}

// endregion

// region Handler holder with static C function callbacks

@OptIn(ExperimentalForeignApi::class)
private object NativeDesktopMMKVHandlerHolder {
    var handler: MMKVHandler? = null

    val logCallback: mmkv_log_callback_t = staticCFunction { level, file, line, function, message ->
        NativeDesktopMMKVHandlerHolder.handler?.mmkvLog(
            nativeLogLevelToCommon(level),
            file?.toKString() ?: "",
            line,
            function?.toKString() ?: "",
            message?.toKString() ?: ""
        )
    }

    val errorCallback: mmkv_error_callback_t = staticCFunction { mmapID, error ->
        val result = when (error) {
            0 -> NativeDesktopMMKVHandlerHolder.handler?.onMMKVCRCCheckFail(mmapID?.toKString() ?: "")
            else -> NativeDesktopMMKVHandlerHolder.handler?.onMMKVFileLengthError(mmapID?.toKString() ?: "")
        }
        when (result) {
            MMKVRecoverStrategic.OnErrorRecover -> 1
            else -> 0
        }
    }

    val contentChangeCallback: mmkv_content_change_callback_t = staticCFunction { mmapID ->
        NativeDesktopMMKVHandlerHolder.handler?.onContentChangedByOuterProcess(mmapID?.toKString() ?: "")
    }

    val contentLoadCallback: mmkv_content_load_callback_t = staticCFunction { mmapID ->
        NativeDesktopMMKVHandlerHolder.handler?.onMMKVContentLoadSuccessfully(mmapID?.toKString() ?: "")
    }
}

// endregion
