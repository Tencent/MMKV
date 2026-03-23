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

/**
 * An highly efficient, reliable, multi-process key-value storage framework.
 * Kotlin Multiplatform wrapper for MMKV.
 *
 * Note: Platform-specific initialization methods are provided as extension functions
 * on [MMKV.Companion] in each platform source set, since Android requires [Context]
 * while other platforms do not.
 */
expect class MMKV {

    companion object {
        /**
         * Notify MMKV that App is about to exit. It's totally fine not calling it at all.
         */
        fun onExit()

        /**
         * Set the log level of MMKV.
         * @param level Defaults to [MMKVLogLevel.Info].
         */
        fun setLogLevel(level: MMKVLogLevel)

        /**
         * Create the default MMKV instance in single-process mode.
         */
        fun defaultMMKV(): MMKV

        /**
         * Create the default MMKV instance with the given configuration.
         * @param config The all-in-one configuration for the MMKV instance.
         */
        fun defaultMMKV(config: MMKVConfig): MMKV

        /**
         * Create an MMKV instance with an unique ID (in single-process mode).
         * @param mmapID The unique ID of the MMKV instance.
         */
        fun mmkvWithID(mmapID: String): MMKV

        /**
         * Create an MMKV instance with an unique ID and the given configuration.
         * @param mmapID The unique ID of the MMKV instance.
         * @param config The all-in-one configuration for the MMKV instance.
         */
        fun mmkvWithID(mmapID: String, config: MMKVConfig): MMKV

        /**
         * @return The device's memory page size.
         */
        fun pageSize(): Int

        /**
         * @return The version of MMKV.
         */
        fun version(): String

        /**
         * @return The root folder of MMKV.
         */
        fun rootDir(): String

        /**
         * Backup one MMKV instance to dstDir.
         * @param mmapID The MMKV ID to backup.
         * @param dstDir The backup destination directory.
         * @param rootPath The customize root path of the MMKV, if null then backup from the root dir of MMKV.
         */
        fun backupOneToDirectory(mmapID: String, dstDir: String, rootPath: String? = null): Boolean

        /**
         * Restore one MMKV instance from srcDir.
         * @param mmapID The MMKV ID to restore.
         * @param srcDir The restore source directory.
         * @param rootPath The customize root path of the MMKV, if null then restore to the root dir of MMKV.
         */
        fun restoreOneFromDirectory(mmapID: String, srcDir: String, rootPath: String? = null): Boolean

        /**
         * Backup all MMKV instances to dstDir.
         * @param dstDir The backup destination directory.
         * @return Count of MMKV successfully backed up.
         */
        fun backupAllToDirectory(dstDir: String): Long

        /**
         * Restore all MMKV instances from srcDir.
         * @param srcDir The restore source directory.
         * @return Count of MMKV successfully restored.
         */
        fun restoreAllFromDirectory(srcDir: String): Long

        /**
         * Check whether the MMKV file is valid or not.
         * Note: Don't use this to check the existence of the instance, the result is undefined on nonexistent files.
         */
        fun isFileValid(mmapID: String, rootPath: String? = null): Boolean

        /**
         * Remove the storage of the MMKV, including the data file & meta file (.crc).
         * Note: the existing instance (if any) will be closed & destroyed.
         */
        fun removeStorage(mmapID: String, rootPath: String? = null): Boolean

        /**
         * Check existence of the MMKV file.
         */
        fun checkExist(mmapID: String, rootPath: String? = null): Boolean

        /**
         * Register a unified callback handler for MMKV.
         */
        fun registerHandler(handler: MMKVHandler)

        /**
         * Unregister the callback handler.
         */
        fun unRegisterHandler()
    }

    /** The unique ID of the MMKV instance. */
    val mmapID: String

    /** Whether the MMKV instance is in multi-process mode. */
    val isMultiProcess: Boolean

    /** Whether the MMKV instance is in read-only mode. */
    val isReadOnly: Boolean

    /**
     * Get the size of the underlying file.
     * Align to the disk block size, typically 4K for an Android device.
     */
    val totalSize: Long

    /**
     * Get the actual used size of the MMKV instance.
     */
    val actualSize: Long

    /**
     * The total count of all the keys.
     */
    val count: Long

    /**
     * All keys.
     */
    val allKeys: List<String>

    /**
     * The encryption key (no more than 16 bytes, or 32 bytes for AES-256).
     */
    val cryptKey: String?

    // region Encode

    fun encodeBool(key: String, value: Boolean): Boolean
    fun encodeBool(key: String, value: Boolean, expireDuration: UInt): Boolean
    fun encodeInt(key: String, value: Int): Boolean
    fun encodeInt(key: String, value: Int, expireDuration: UInt): Boolean
    fun encodeLong(key: String, value: Long): Boolean
    fun encodeLong(key: String, value: Long, expireDuration: UInt): Boolean
    fun encodeFloat(key: String, value: Float): Boolean
    fun encodeFloat(key: String, value: Float, expireDuration: UInt): Boolean
    fun encodeDouble(key: String, value: Double): Boolean
    fun encodeDouble(key: String, value: Double, expireDuration: UInt): Boolean
    fun encodeString(key: String, value: String): Boolean
    fun encodeString(key: String, value: String, expireDuration: UInt): Boolean
    fun encodeBytes(key: String, value: ByteArray): Boolean
    fun encodeBytes(key: String, value: ByteArray, expireDuration: UInt): Boolean

    // endregion

    // region Decode

    fun decodeBool(key: String, defaultValue: Boolean = false): Boolean
    fun decodeInt(key: String, defaultValue: Int = 0): Int
    fun decodeLong(key: String, defaultValue: Long = 0L): Long
    fun decodeFloat(key: String, defaultValue: Float = 0f): Float
    fun decodeDouble(key: String, defaultValue: Double = 0.0): Double
    fun decodeString(key: String, defaultValue: String? = null): String?
    fun decodeBytes(key: String): ByteArray?

    // endregion

    // region Key management

    /**
     * Check whether or not MMKV contains the key.
     */
    fun containsKey(key: String): Boolean

    /**
     * @return Count of non-expired keys. Note that this call has costs.
     */
    fun countNonExpiredKeys(): Long

    /**
     * @return All non-expired keys. Note that this call has costs.
     */
    fun allNonExpiredKeys(): List<String>

    fun removeValueForKey(key: String)
    fun removeValuesForKeys(keys: List<String>)

    /**
     * Clear all the key-values inside the MMKV instance.
     */
    fun clearAll()

    /**
     * Faster [clearAll] implementation. The file size is kept as previous for later use.
     */
    fun clearAllKeepSpace()

    // endregion

    // region Encryption

    /**
     * Transform plain text into encrypted text, or vice versa by passing a null encryption key.
     * You can also change existing crypt key with a different cryptKey.
     * @param newKey The new encryption key, or null to remove encryption.
     * @param aes256 Use AES 256 key length.
     */
    fun reKey(newKey: String?, aes256: Boolean = false): Boolean

    /**
     * Just reset the encryption key (will not encrypt or decrypt anything).
     * Usually you should call this method after another process has [reKey] the multi-process MMKV instance.
     */
    fun checkReSetCryptKey(cryptKey: String?, aes256: Boolean = false)

    // endregion

    // region Utility

    /**
     * Save all mmap memory to file synchronously.
     */
    fun sync()

    /**
     * Save all mmap memory to file asynchronously.
     */
    fun async()

    /**
     * The [totalSize] of an MMKV instance won't reduce after deleting key-values,
     * call this method after lots of deleting if you care about disk usage.
     */
    fun trim()

    /**
     * Call this method if the MMKV instance is no longer needed in the near future.
     * Any subsequent call to the instance is undefined behavior.
     */
    fun close()

    /**
     * Clear memory cache of the MMKV instance.
     * You can call it on memory warning.
     */
    fun clearMemoryCache()

    /**
     * Import all key-value items from source.
     * @return Count of items imported.
     */
    fun importFrom(source: MMKV): Long

    /**
     * Enable auto key expiration.
     * @param expiredInSeconds 0 means no common expiration duration, aka each key will have its own expire date.
     */
    fun enableAutoKeyExpire(expiredInSeconds: UInt = 0u): Boolean

    /**
     * Disable auto key expiration.
     */
    fun disableAutoKeyExpire(): Boolean

    /**
     * Enable data compare before set, for better performance.
     */
    fun enableCompareBeforeSet(): Boolean

    /**
     * Disable data compare before set.
     */
    fun disableCompareBeforeSet(): Boolean

    /**
     * Check if content changed by other process.
     */
    fun checkContentChanged()

    // endregion
}
