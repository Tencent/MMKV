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

import cocoapods.MMKV.MMKV as DarwinMMKV
import cocoapods.MMKV.MMKVConfig as DarwinMMKVConfig
import cocoapods.MMKV.MMKVHandlerProtocol
import cocoapods.MMKV.MMKVLogDebug
import cocoapods.MMKV.MMKVLogError
import cocoapods.MMKV.MMKVLogInfo
import cocoapods.MMKV.MMKVLogLevel as DarwinMMKVLogLevel
import cocoapods.MMKV.MMKVLogNone
import cocoapods.MMKV.MMKVLogWarning
import cocoapods.MMKV.MMKVMultiProcess
import cocoapods.MMKV.MMKVOnErrorDiscard
import cocoapods.MMKV.MMKVOnErrorNotSet
import cocoapods.MMKV.MMKVOnErrorRecover
import cocoapods.MMKV.MMKVReadOnly
import cocoapods.MMKV.MMKVRecoverStrategic as DarwinMMKVRecoverStrategic
import cocoapods.MMKV.MMKVSingleProcess
import kotlinx.cinterop.CValue
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.cValue
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.toKString
import kotlinx.cinterop.usePinned
import platform.Foundation.NSData
import platform.Foundation.NSNumber
import platform.Foundation.NSString
import platform.Foundation.create
import platform.Foundation.dataUsingEncoding
import platform.Foundation.NSUTF8StringEncoding
import platform.darwin.NSObject
import platform.posix.memcpy

// region Platform-specific initialization (Darwin doesn't need Context)

/**
 * Initialize MMKV with customize settings.
 * Call this in main thread, before calling any other MMKV methods.
 *
 * @param rootDir The root dir of MMKV, passing null defaults to {NSDocumentDirectory}/mmkv.
 * @param logLevel MMKVLogInfo by default, MMKVLogNone to disable all logging.
 * @param handler The unified callback handler for MMKV.
 * @return Root dir of MMKV.
 */
@OptIn(ExperimentalForeignApi::class)
fun MMKV.Companion.initialize(
    rootDir: String? = null,
    logLevel: MMKVLogLevel = MMKVLogLevel.Info,
    handler: MMKVHandler? = null,
): String {
    val darwinHandler = handler?.let { DarwinMMKVHandlerAdapter(it) }
    return DarwinMMKV.initializeMMKV(rootDir, logLevel = logLevel.toDarwin(), handler = darwinHandler) ?: ""
}

/**
 * Initialize MMKV with a group directory for multi-process access.
 *
 * @param rootDir The root dir of MMKV, passing null defaults to {NSDocumentDirectory}/mmkv.
 * @param groupDir The root dir of multi-process MMKV.
 * @param logLevel MMKVLogInfo by default.
 * @param handler The unified callback handler for MMKV.
 * @return Root dir of MMKV.
 */
@OptIn(ExperimentalForeignApi::class)
fun MMKV.Companion.initialize(
    rootDir: String? = null,
    groupDir: String,
    logLevel: MMKVLogLevel = MMKVLogLevel.Info,
    handler: MMKVHandler? = null,
): String {
    val darwinHandler = handler?.let { DarwinMMKVHandlerAdapter(it) }
    return DarwinMMKV.initializeMMKV(rootDir, groupDir = groupDir, logLevel = logLevel.toDarwin(), handler = darwinHandler) ?: ""
}

// endregion

@OptIn(ExperimentalForeignApi::class)
actual class MMKV internal constructor(private val impl: DarwinMMKV) {

    actual companion object {
        actual fun onExit() {
            DarwinMMKV.onAppTerminate()
        }

        actual fun setLogLevel(level: MMKVLogLevel) {
            @Suppress("DEPRECATION")
            DarwinMMKV.setLogLevel(level.toDarwin())
        }

        actual fun defaultMMKV(): MMKV {
            return MMKV(DarwinMMKV.defaultMMKV()!!)
        }

        actual fun defaultMMKV(config: MMKVConfig): MMKV {
            return MMKV(DarwinMMKV.defaultMMKVWithConfig(config.toDarwinCValue())!!)
        }

        actual fun mmkvWithID(mmapID: String): MMKV {
            return MMKV(DarwinMMKV.mmkvWithID(mmapID)!!)
        }

        actual fun mmkvWithID(mmapID: String, config: MMKVConfig): MMKV {
            return MMKV(DarwinMMKV.mmkvWithID(mmapID, config = config.toDarwinCValue())!!)
        }

        actual fun pageSize(): Int {
            return DarwinMMKV.pageSize().toInt()
        }

        actual fun version(): String {
            return DarwinMMKV.version()
        }

        actual fun rootDir(): String {
            return DarwinMMKV.mmkvBasePath()
        }

        actual fun backupOneToDirectory(mmapID: String, dstDir: String, rootPath: String?): Boolean {
            return DarwinMMKV.backupOneMMKV(mmapID, rootPath = rootPath, toDirectory = dstDir)
        }

        actual fun restoreOneFromDirectory(mmapID: String, srcDir: String, rootPath: String?): Boolean {
            return DarwinMMKV.restoreOneMMKV(mmapID, rootPath = rootPath, fromDirectory = srcDir)
        }

        actual fun backupAllToDirectory(dstDir: String): Long {
            return DarwinMMKV.backupAll(null, toDirectory = dstDir).toLong()
        }

        actual fun restoreAllFromDirectory(srcDir: String): Long {
            return DarwinMMKV.restoreAll(null, fromDirectory = srcDir).toLong()
        }

        actual fun isFileValid(mmapID: String, rootPath: String?): Boolean {
            return DarwinMMKV.isFileValid(mmapID, rootPath = rootPath)
        }

        actual fun removeStorage(mmapID: String, rootPath: String?): Boolean {
            return DarwinMMKV.removeStorage(mmapID, rootPath = rootPath)
        }

        actual fun checkExist(mmapID: String, rootPath: String?): Boolean {
            return DarwinMMKV.checkExist(mmapID, rootPath = rootPath)
        }

        private var currentHandler: DarwinMMKVHandlerAdapter? = null

        actual fun registerHandler(handler: MMKVHandler) {
            val adapter = DarwinMMKVHandlerAdapter(handler)
            currentHandler = adapter
            @Suppress("DEPRECATION")
            DarwinMMKV.registerHandler(adapter)
        }

        actual fun unRegisterHandler() {
            currentHandler = null
            DarwinMMKV.unregiserHandler()
        }
    }

    // region Properties

    actual val mmapID: String get() = impl.mmapID()

    actual val isMultiProcess: Boolean get() = impl.isMultiProcess()

    actual val isReadOnly: Boolean get() = impl.isReadOnly()

    actual val totalSize: Long get() = impl.totalSize().toLong()

    actual val actualSize: Long get() = impl.actualSize().toLong()

    actual val count: Long get() = impl.count().toLong()

    @Suppress("UNCHECKED_CAST")
    actual val allKeys: List<String> get() = (impl.allKeys() as? List<String>) ?: emptyList()

    actual val cryptKey: String?
        get() {
            val data = impl.cryptKey() ?: return null
            return NSString.create(data = data, encoding = NSUTF8StringEncoding) as? String
        }

    // endregion

    // region Encode

    actual fun encodeBool(key: String, value: Boolean): Boolean = impl.setBool(value, forKey = key)
    actual fun encodeBool(key: String, value: Boolean, expireDuration: UInt): Boolean = impl.setBool(value, forKey = key, expireDuration = expireDuration)
    actual fun encodeInt(key: String, value: Int): Boolean = impl.setInt32(value, forKey = key)
    actual fun encodeInt(key: String, value: Int, expireDuration: UInt): Boolean = impl.setInt32(value, forKey = key, expireDuration = expireDuration)
    actual fun encodeLong(key: String, value: Long): Boolean = impl.setInt64(value, forKey = key)
    actual fun encodeLong(key: String, value: Long, expireDuration: UInt): Boolean = impl.setInt64(value, forKey = key, expireDuration = expireDuration)
    actual fun encodeFloat(key: String, value: Float): Boolean = impl.setFloat(value, forKey = key)
    actual fun encodeFloat(key: String, value: Float, expireDuration: UInt): Boolean = impl.setFloat(value, forKey = key, expireDuration = expireDuration)
    actual fun encodeDouble(key: String, value: Double): Boolean = impl.setDouble(value, forKey = key)
    actual fun encodeDouble(key: String, value: Double, expireDuration: UInt): Boolean = impl.setDouble(value, forKey = key, expireDuration = expireDuration)
    actual fun encodeString(key: String, value: String): Boolean = impl.setString(value, forKey = key)
    actual fun encodeString(key: String, value: String, expireDuration: UInt): Boolean = impl.setString(value, forKey = key, expireDuration = expireDuration)

    actual fun encodeBytes(key: String, value: ByteArray): Boolean {
        val data = value.toNSData()
        return impl.setData(data, forKey = key)
    }

    actual fun encodeBytes(key: String, value: ByteArray, expireDuration: UInt): Boolean {
        val data = value.toNSData()
        return impl.setData(data, forKey = key, expireDuration = expireDuration)
    }

    // endregion

    // region Decode

    actual fun decodeBool(key: String, defaultValue: Boolean): Boolean = impl.getBoolForKey(key, defaultValue = defaultValue)
    actual fun decodeInt(key: String, defaultValue: Int): Int = impl.getInt32ForKey(key, defaultValue = defaultValue)
    actual fun decodeLong(key: String, defaultValue: Long): Long = impl.getInt64ForKey(key, defaultValue = defaultValue)
    actual fun decodeFloat(key: String, defaultValue: Float): Float = impl.getFloatForKey(key, defaultValue = defaultValue)
    actual fun decodeDouble(key: String, defaultValue: Double): Double = impl.getDoubleForKey(key, defaultValue = defaultValue)
    actual fun decodeString(key: String, defaultValue: String?): String? = impl.getStringForKey(key, defaultValue = defaultValue)

    actual fun decodeBytes(key: String): ByteArray? {
        val data = impl.getDataForKey(key) ?: return null
        return data.toByteArray()
    }

    // endregion

    // region Key management

    actual fun containsKey(key: String): Boolean = impl.containsKey(key)

    actual fun countNonExpiredKeys(): Long = impl.countNonExpiredKeys().toLong()

    @Suppress("UNCHECKED_CAST")
    actual fun allNonExpiredKeys(): List<String> = (impl.allNonExpiredKeys() as? List<String>) ?: emptyList()

    actual fun removeValueForKey(key: String) = impl.removeValueForKey(key)

    actual fun removeValuesForKeys(keys: List<String>) = impl.removeValuesForKeys(keys)

    actual fun clearAll() = impl.clearAll()

    actual fun clearAllKeepSpace() = impl.clearAllWithKeepingSpace()

    // endregion

    // region Encryption

    actual fun reKey(newKey: String?, aes256: Boolean): Boolean {
        val keyData = newKey?.toNSData()
        return impl.reKey(keyData, aes256 = aes256)
    }

    actual fun checkReSetCryptKey(cryptKey: String?, aes256: Boolean) {
        val keyData = cryptKey?.toNSData()
        impl.checkReSetCryptKey(keyData, aes256 = aes256)
    }

    // endregion

    // region Utility

    actual fun sync() = impl.sync()
    actual fun async() = impl.async()
    actual fun trim() = impl.trim()
    actual fun close() = impl.close()
    actual fun clearMemoryCache() = impl.clearMemoryCache()

    actual fun importFrom(source: MMKV): Long = impl.importFrom(source.impl).toLong()

    actual fun enableAutoKeyExpire(expiredInSeconds: UInt): Boolean = impl.enableAutoKeyExpire(expiredInSeconds)
    actual fun disableAutoKeyExpire(): Boolean = impl.disableAutoKeyExpire()

    actual fun enableCompareBeforeSet(): Boolean = impl.enableCompareBeforeSet()
    actual fun disableCompareBeforeSet(): Boolean = impl.disableCompareBeforeSet()

    actual fun checkContentChanged() = impl.checkContentChanged()

    actual fun getValueSize(key: String, actualSize: Boolean): Long = impl.getValueSizeForKey(key, actualSize = actualSize).toLong()

    @OptIn(ExperimentalForeignApi::class)
    actual fun writeValueToBuffer(key: String, buffer: ByteArray): Int {
        if (buffer.isEmpty()) return -1
        val nsData = platform.Foundation.NSMutableData.create(length = buffer.size.toULong())!!
        val written = impl.writeValueForKey(key, toBuffer = nsData)
        if (written > 0) {
            val len = minOf(written, buffer.size)
            buffer.usePinned { pinned ->
                memcpy(pinned.addressOf(0), nsData.bytes, len.toULong())
            }
        }
        return written
    }

    actual fun lock() {
        // lock() is not exposed in the ObjC MMKV API.
        // Inter-process lock is handled automatically on Darwin.
    }
    actual fun unlock() {
        // unlock() is not exposed in the ObjC MMKV API.
    }
    actual fun tryLock(): Boolean {
        // tryLock() is not exposed in the ObjC MMKV API.
        return false
    }

    actual val isExpirationEnabled: Boolean get() = false // not exposed in ObjC API
    actual val isEncryptionEnabled: Boolean get() = (cryptKey != null)
    actual val isCompareBeforeSetEnabled: Boolean get() = false // not exposed in ObjC API

    // endregion
}

// region Conversion helpers

@OptIn(ExperimentalForeignApi::class)
private fun MMKVLogLevel.toDarwin(): DarwinMMKVLogLevel = when (this) {
    MMKVLogLevel.Debug -> MMKVLogDebug
    MMKVLogLevel.Info -> MMKVLogInfo
    MMKVLogLevel.Warning -> MMKVLogWarning
    MMKVLogLevel.Error -> MMKVLogError
    MMKVLogLevel.None -> MMKVLogNone
}

@OptIn(ExperimentalForeignApi::class)
private fun darwinLogLevelToCommon(level: DarwinMMKVLogLevel): MMKVLogLevel = when (level) {
    MMKVLogDebug -> MMKVLogLevel.Debug
    MMKVLogInfo -> MMKVLogLevel.Info
    MMKVLogWarning -> MMKVLogLevel.Warning
    MMKVLogError -> MMKVLogLevel.Error
    MMKVLogNone -> MMKVLogLevel.None
    else -> MMKVLogLevel.Info
}

@OptIn(ExperimentalForeignApi::class)
private fun MMKVRecoverStrategic.toDarwin(): DarwinMMKVRecoverStrategic = when (this) {
    MMKVRecoverStrategic.OnErrorDiscard -> MMKVOnErrorDiscard
    MMKVRecoverStrategic.OnErrorRecover -> MMKVOnErrorRecover
}

@OptIn(ExperimentalForeignApi::class)
private fun darwinRecoverToCommon(value: DarwinMMKVRecoverStrategic): MMKVRecoverStrategic = when (value) {
    MMKVOnErrorRecover -> MMKVRecoverStrategic.OnErrorRecover
    else -> MMKVRecoverStrategic.OnErrorDiscard
}

@OptIn(ExperimentalForeignApi::class)
internal fun MMKVConfig.toDarwinCValue(): CValue<DarwinMMKVConfig> = cValue {
    mode = when {
        this@toDarwinCValue.mode and MMKVMode.READ_ONLY != 0 -> MMKVReadOnly
        this@toDarwinCValue.mode and MMKVMode.MULTI_PROCESS != 0 -> MMKVMultiProcess
        else -> MMKVSingleProcess
    }
    aes256 = this@toDarwinCValue.aes256
    cryptKey = this@toDarwinCValue.cryptKey?.toNSData()
    rootPath = this@toDarwinCValue.rootPath
    expectedCapacity = this@toDarwinCValue.expectedCapacity.toULong()
    enableKeyExpire = this@toDarwinCValue.enableKeyExpire?.let { NSNumber(bool = it) }
    expiredInSeconds = this@toDarwinCValue.expiredInSeconds
    enableCompareBeforeSet = this@toDarwinCValue.enableCompareBeforeSet
    recover = this@toDarwinCValue.recover?.toDarwin() ?: MMKVOnErrorNotSet
    itemSizeLimit = this@toDarwinCValue.itemSizeLimit
}

// endregion

// region NSData <-> ByteArray helpers

@OptIn(ExperimentalForeignApi::class)
private fun ByteArray.toNSData(): NSData {
    if (isEmpty()) return NSData()
    return usePinned { pinned ->
        NSData.create(bytes = pinned.addressOf(0), length = size.toULong())
    }
}

@OptIn(ExperimentalForeignApi::class)
private fun NSData.toByteArray(): ByteArray {
    val length = this.length.toInt()
    if (length == 0) return ByteArray(0)
    val bytes = ByteArray(length)
    bytes.usePinned { pinned ->
        memcpy(pinned.addressOf(0), this@toByteArray.bytes, this@toByteArray.length)
    }
    return bytes
}

@OptIn(ExperimentalForeignApi::class)
private fun String.toNSData(): NSData {
    @Suppress("CAST_NEVER_SUCCEEDS")
    return (this as NSString).dataUsingEncoding(NSUTF8StringEncoding) ?: NSData()
}

// endregion

// region Handler adapter

/**
 * Adapts the common [MMKVHandler] abstract class to the Darwin MMKVHandler ObjC protocol.
 */
@OptIn(ExperimentalForeignApi::class)
private class DarwinMMKVHandlerAdapter(
    private val handler: MMKVHandler,
) : NSObject(), MMKVHandlerProtocol {

    override fun onMMKVCRCCheckFail(mmapID: String?): DarwinMMKVRecoverStrategic {
        return handler.onMMKVCRCCheckFail(mmapID ?: "").toDarwin()
    }

    override fun onMMKVFileLengthError(mmapID: String?): DarwinMMKVRecoverStrategic {
        return handler.onMMKVFileLengthError(mmapID ?: "").toDarwin()
    }

    override fun mmkvLogWithLevel(level: DarwinMMKVLogLevel, file: kotlinx.cinterop.CPointer<kotlinx.cinterop.ByteVar>?, line: Int, func: kotlinx.cinterop.CPointer<kotlinx.cinterop.ByteVar>?, message: String?) {
        val fileName = file?.toKString() ?: ""
        val funcName = func?.toKString() ?: ""
        handler.mmkvLog(darwinLogLevelToCommon(level), fileName, line, funcName, message ?: "")
    }

    override fun onMMKVContentChange(mmapID: String?) {
        handler.onContentChangedByOuterProcess(mmapID ?: "")
    }

    override fun onMMKVContentLoadSuccessfully(mmapID: String?) {
        handler.onMMKVContentLoadSuccessfully(mmapID ?: "")
    }
}

// endregion
