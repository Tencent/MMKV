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

import android.content.Context
import com.tencent.mmkv.MMKV as AndroidMMKV
import com.tencent.mmkv.MMKVConfig as AndroidMMKVConfig
import com.tencent.mmkv.MMKVHandler as AndroidMMKVHandler
import com.tencent.mmkv.MMKVLogLevel as AndroidMMKVLogLevel
import com.tencent.mmkv.MMKVRecoverStrategic as AndroidMMKVRecoverStrategic

// region Platform-specific initialization (Android needs Context)

/**
 * Initialize MMKV with default configuration.
 * You must call one of the initialize() methods on App startup process before using MMKV.
 *
 * @param context The context of Android App, usually from Application.
 * @param logLevel The log level of MMKV, defaults to [MMKVLogLevel.Info].
 * @return The root folder of MMKV, defaults to $(FilesDir)/mmkv.
 */
fun MMKV.Companion.initialize(
    context: Context,
    logLevel: MMKVLogLevel = MMKVLogLevel.Info,
): String {
    return AndroidMMKV.initialize(context, logLevel.toAndroid())
}

/**
 * Initialize MMKV with customize root folder.
 *
 * @param context The context of Android App, usually from Application.
 * @param rootDir The root folder of MMKV, defaults to $(FilesDir)/mmkv.
 * @param logLevel The log level of MMKV, defaults to [MMKVLogLevel.Info].
 * @param handler The unified callback handler for MMKV.
 * @return The root folder of MMKV.
 */
fun MMKV.Companion.initialize(
    context: Context,
    rootDir: String,
    logLevel: MMKVLogLevel = MMKVLogLevel.Info,
    handler: MMKVHandler? = null,
): String {
    val androidHandler = handler?.let { AndroidMMKVHandlerAdapter(it) }
    return AndroidMMKV.initialize(context, rootDir, null, logLevel.toAndroid(), androidHandler)
}

// endregion

/**
 * The Android actual implementation of [MMKV] that delegates to the Java `com.tencent.mmkv.MMKV` library.
 *
 * Since the KMP class is now in `com.tencent.mmkv.kmp`, we can reference the Java
 * `com.tencent.mmkv.MMKV` directly via import alias without any naming collision.
 */
actual class MMKV internal constructor(private val impl: AndroidMMKV) {

    actual companion object {
        actual fun onExit() = AndroidMMKV.onExit()

        actual fun setLogLevel(level: MMKVLogLevel) = AndroidMMKV.setLogLevel(level.toAndroid())

        actual fun defaultMMKV(): MMKV = MMKV(AndroidMMKV.defaultMMKV())

        actual fun defaultMMKV(config: MMKVConfig): MMKV = MMKV(AndroidMMKV.defaultMMKV(config.toAndroid()))

        actual fun mmkvWithID(mmapID: String): MMKV = MMKV(AndroidMMKV.mmkvWithID(mmapID))

        actual fun mmkvWithID(mmapID: String, config: MMKVConfig): MMKV = MMKV(AndroidMMKV.mmkvWithID(mmapID, config.toAndroid()))

        actual fun pageSize(): Int = AndroidMMKV.pageSize()

        actual fun version(): String = AndroidMMKV.version()

        actual fun rootDir(): String = AndroidMMKV.getRootDir() ?: ""

        actual fun backupOneToDirectory(mmapID: String, dstDir: String, rootPath: String?): Boolean =
            AndroidMMKV.backupOneToDirectory(mmapID, dstDir, rootPath)

        actual fun restoreOneFromDirectory(mmapID: String, srcDir: String, rootPath: String?): Boolean =
            AndroidMMKV.restoreOneMMKVFromDirectory(mmapID, srcDir, rootPath)

        actual fun backupAllToDirectory(dstDir: String): Long = AndroidMMKV.backupAllToDirectory(dstDir)

        actual fun restoreAllFromDirectory(srcDir: String): Long = AndroidMMKV.restoreAllFromDirectory(srcDir)

        actual fun isFileValid(mmapID: String, rootPath: String?): Boolean = AndroidMMKV.isFileValid(mmapID, rootPath)

        actual fun removeStorage(mmapID: String, rootPath: String?): Boolean = AndroidMMKV.removeStorage(mmapID, rootPath)

        actual fun checkExist(mmapID: String, rootPath: String?): Boolean = AndroidMMKV.checkExist(mmapID, rootPath)

        actual fun registerHandler(handler: MMKVHandler) = AndroidMMKV.registerHandler(AndroidMMKVHandlerAdapter(handler))

        actual fun unRegisterHandler() = AndroidMMKV.unregisterHandler()
    }

    // region Properties

    actual val mmapID: String get() = impl.mmapID()
    actual val isMultiProcess: Boolean get() = impl.isMultiProcess
    actual val isReadOnly: Boolean get() = impl.isReadOnly
    actual val totalSize: Long get() = impl.totalSize()
    actual val actualSize: Long get() = impl.actualSize()
    actual val count: Long get() = impl.count()
    actual val allKeys: List<String> get() = impl.allKeys()?.toList() ?: emptyList()
    actual val cryptKey: String? get() = impl.cryptKey()

    // endregion

    // region Encode

    actual fun encodeBool(key: String, value: Boolean): Boolean = impl.encode(key, value)
    actual fun encodeBool(key: String, value: Boolean, expireDuration: UInt): Boolean = impl.encode(key, value, expireDuration.toInt())
    actual fun encodeInt(key: String, value: Int): Boolean = impl.encode(key, value)
    actual fun encodeInt(key: String, value: Int, expireDuration: UInt): Boolean = impl.encode(key, value, expireDuration.toInt())
    actual fun encodeLong(key: String, value: Long): Boolean = impl.encode(key, value)
    actual fun encodeLong(key: String, value: Long, expireDuration: UInt): Boolean = impl.encode(key, value, expireDuration.toInt())
    actual fun encodeFloat(key: String, value: Float): Boolean = impl.encode(key, value)
    actual fun encodeFloat(key: String, value: Float, expireDuration: UInt): Boolean = impl.encode(key, value, expireDuration.toInt())
    actual fun encodeDouble(key: String, value: Double): Boolean = impl.encode(key, value)
    actual fun encodeDouble(key: String, value: Double, expireDuration: UInt): Boolean = impl.encode(key, value, expireDuration.toInt())
    actual fun encodeString(key: String, value: String): Boolean = impl.encode(key, value)
    actual fun encodeString(key: String, value: String, expireDuration: UInt): Boolean = impl.encode(key, value, expireDuration.toInt())
    actual fun encodeBytes(key: String, value: ByteArray): Boolean = impl.encode(key, value)
    actual fun encodeBytes(key: String, value: ByteArray, expireDuration: UInt): Boolean = impl.encode(key, value, expireDuration.toInt())

    // endregion

    // region Decode

    actual fun decodeBool(key: String, defaultValue: Boolean): Boolean = impl.decodeBool(key, defaultValue)
    actual fun decodeInt(key: String, defaultValue: Int): Int = impl.decodeInt(key, defaultValue)
    actual fun decodeLong(key: String, defaultValue: Long): Long = impl.decodeLong(key, defaultValue)
    actual fun decodeFloat(key: String, defaultValue: Float): Float = impl.decodeFloat(key, defaultValue)
    actual fun decodeDouble(key: String, defaultValue: Double): Double = impl.decodeDouble(key, defaultValue)
    actual fun decodeString(key: String, defaultValue: String?): String? = impl.decodeString(key, defaultValue)
    actual fun decodeBytes(key: String): ByteArray? = impl.decodeBytes(key)

    // endregion

    // region Key management

    actual fun containsKey(key: String): Boolean = impl.containsKey(key)
    actual fun countNonExpiredKeys(): Long = impl.countNonExpiredKeys()
    actual fun allNonExpiredKeys(): List<String> = impl.allNonExpireKeys()?.toList() ?: emptyList()
    actual fun removeValueForKey(key: String) = impl.removeValueForKey(key)
    actual fun removeValuesForKeys(keys: List<String>) = impl.removeValuesForKeys(keys.toTypedArray())
    actual fun clearAll() = impl.clearAll()
    actual fun clearAllKeepSpace() = impl.clearAllWithKeepingSpace()

    // endregion

    // region Encryption

    actual fun reKey(newKey: String?, aes256: Boolean): Boolean = impl.reKey(newKey, aes256)
    actual fun checkReSetCryptKey(cryptKey: String?, aes256: Boolean) = impl.checkReSetCryptKey(cryptKey, aes256)

    // endregion

    // region Utility

    actual fun sync() = impl.sync()
    actual fun async() = impl.async()
    actual fun trim() = impl.trim()
    actual fun close() = impl.close()
    actual fun clearMemoryCache() = impl.clearMemoryCache()

    actual fun importFrom(source: MMKV): Long = impl.importFrom(source.impl)

    actual fun enableAutoKeyExpire(expiredInSeconds: UInt): Boolean = impl.enableAutoKeyExpire(expiredInSeconds.toInt())
    actual fun disableAutoKeyExpire(): Boolean = impl.disableAutoKeyExpire()

    actual fun enableCompareBeforeSet(): Boolean {
        impl.enableCompareBeforeSet()
        return true
    }

    actual fun disableCompareBeforeSet(): Boolean {
        impl.disableCompareBeforeSet()
        return true
    }

    actual fun checkContentChanged() = impl.checkContentChangedByOuterProcess()

    // endregion
}

// region Enum conversion helpers

private fun MMKVLogLevel.toAndroid(): AndroidMMKVLogLevel = when (this) {
    MMKVLogLevel.Debug -> AndroidMMKVLogLevel.LevelDebug
    MMKVLogLevel.Info -> AndroidMMKVLogLevel.LevelInfo
    MMKVLogLevel.Warning -> AndroidMMKVLogLevel.LevelWarning
    MMKVLogLevel.Error -> AndroidMMKVLogLevel.LevelError
    MMKVLogLevel.None -> AndroidMMKVLogLevel.LevelNone
}

private fun AndroidMMKVLogLevel.toKmp(): MMKVLogLevel = when (this) {
    AndroidMMKVLogLevel.LevelDebug -> MMKVLogLevel.Debug
    AndroidMMKVLogLevel.LevelInfo -> MMKVLogLevel.Info
    AndroidMMKVLogLevel.LevelWarning -> MMKVLogLevel.Warning
    AndroidMMKVLogLevel.LevelError -> MMKVLogLevel.Error
    AndroidMMKVLogLevel.LevelNone -> MMKVLogLevel.None
}

private fun MMKVRecoverStrategic.toAndroid(): AndroidMMKVRecoverStrategic = when (this) {
    MMKVRecoverStrategic.OnErrorDiscard -> AndroidMMKVRecoverStrategic.OnErrorDiscard
    MMKVRecoverStrategic.OnErrorRecover -> AndroidMMKVRecoverStrategic.OnErrorRecover
}

// endregion

// region Config conversion

private fun MMKVConfig.toAndroid(): AndroidMMKVConfig {
    val config = AndroidMMKVConfig()
    config.mode = mode
    config.cryptKey = cryptKey
    config.aes256 = aes256
    config.rootPath = rootPath
    config.expectedCapacity = expectedCapacity
    config.enableKeyExpire = enableKeyExpire
    config.expiredInSeconds = expiredInSeconds.toInt()
    config.enableCompareBeforeSet = enableCompareBeforeSet
    config.recover = recover?.toAndroid()
    config.itemSizeLimit = itemSizeLimit.toInt()
    return config
}

// endregion

// region Handler adapter

/**
 * Adapts the KMP [MMKVHandler] to the Java [AndroidMMKVHandler] interface.
 */
private class AndroidMMKVHandlerAdapter(
    private val handler: MMKVHandler,
) : AndroidMMKVHandler {

    override fun onMMKVCRCCheckFail(mmapID: String): AndroidMMKVRecoverStrategic {
        return handler.onMMKVCRCCheckFail(mmapID).toAndroid()
    }

    override fun onMMKVFileLengthError(mmapID: String): AndroidMMKVRecoverStrategic {
        return handler.onMMKVFileLengthError(mmapID).toAndroid()
    }

    override fun wantLogRedirecting(): Boolean = handler.wantLogRedirect()

    override fun mmkvLog(level: AndroidMMKVLogLevel, file: String, line: Int, function: String, message: String) {
        handler.mmkvLog(level.toKmp(), file, line, function, message)
    }

    override fun wantContentChangeNotification(): Boolean = handler.wantContentChangeNotification()

    override fun onContentChangedByOuterProcess(mmapID: String) = handler.onContentChangedByOuterProcess(mmapID)

    override fun onMMKVContentLoadSuccessfully(mmapID: String) = handler.onMMKVContentLoadSuccessfully(mmapID)
}

// endregion
