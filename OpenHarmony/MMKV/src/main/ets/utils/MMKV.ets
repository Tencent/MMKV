/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
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

import native from 'libmmkv.so';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { MMKVLogLevel } from './MMKVLogLevel';
import { getObjKeys } from './Util';
import { NativeBuffer } from './NativeBuffer';
import common from '@ohos.app.ability.common';
import dataPreferences from '@ohos.data.preferences';

/**
 * An highly efficient, reliable, multi-process key-value storage framework.
 * THE PERFECT drop-in replacement for Preferences.
 */
export class MMKV {
    /**
     * Single-process mode. The default mode on an MMKV instance.
     */
    public static readonly SINGLE_PROCESS_MODE: number = 1 << 0
    /**
     * Multi-process mode.
     * To enable multi-process accessing of an MMKV instance, you must set this mode whenever you getting that instance.
     */
    public static readonly MULTI_PROCESS_MODE: number = 1 << 1
    private static readonly ASHMEM_MODE: number = 1 << 3
    private static readonly BACKUP_MODE: number = 1 << 4
    private static g_rootDir: string

    private static initData() {
        // MMKV.checkedHandleSet = new Set<string>()
    }

    /**
     * Initialize MMKV with default configuration.
     * You must call one of the initialize() methods on App startup process before using MMKV.
     *
     * @param context The ApplicationContext of the App, usually from UIAbility.
     * @param logLevel The log level of MMKV, defaults to {@link MMKVLogLevel.LevelInfo}.
     * @returns The root folder of MMKV, defaults to $(FilesDir)/mmkv.
     */
    public static initialize(context: common.ApplicationContext, logLevel?: MMKVLogLevel): string {
        let rootDir = context.filesDir + "/mmkv";
        let cacheDir = context.cacheDir;
        // hilog.info(0x0000, 'mmkv', '%{public}s, %{public}s, %{public}d', rootDir, cacheDir, logLevel);
        return MMKV.initializeWithPath(rootDir, cacheDir, logLevel);
    }

    /**
     * Initialize MMKV with customize settings.
     * You must call one of the initialize() methods on App startup process before using MMKV.
     * @param rootDir The root folder of MMKV, defaults to $(FilesDir)/mmkv.
     * @param cacheDir The cache folder of MMKV, defaults to $(CacheDir).
     * @param logLevel The log level of MMKV, defaults to {@link MMKVLogLevel.LevelInfo}.
     * @returns The root folder of MMKV, defaults to $(FilesDir)/mmkv.
     */
    public static initializeWithPath(rootDir: string, cacheDir: string, logLevel?: MMKVLogLevel): string {
        MMKV.initData();
        let level = logLevel ?? MMKVLogLevel.LevelInfo;
        // hilog.info(0x0000, 'mmkv', '%{public}s, %{public}s, %{public}d', rootDir, cacheDir, level);
        MMKV.g_rootDir = native.initialize(rootDir, cacheDir, level);
        return MMKV.g_rootDir;
    }

    /**
     * @returns The version of MMKV.
     */
    public static get version(): string {
        return native.version();
    }

    /**
     * @returns The device's memory page size.
     */
    public static get pageSize(): number {
        return native.pageSize();
    }

    /**
     * @returns The root folder of MMKV, defaults to $(FilesDir)/mmkv.
     */
    public static get rootDir(): string {
        return MMKV.g_rootDir;
    }

    private nativeHandle: bigint;

    private constructor(handle: bigint) {
        this.nativeHandle = handle;
    }

    /**
     * Create the default MMKV instance in customize process mode, with an encryption key.
     * @param mode The process mode of the MMKV instance, defaults to {@link MMKV.SINGLE_PROCESS_MODE}.
     * @param cryptKey The encryption key of the MMKV instance (no more than 16 bytes).
     * @returns The default MMKV instance.
     */
    public static defaultMMKV(mode?: number, cryptKey?: string): MMKV {
        if (!MMKV.g_rootDir) {
            throw new Error('You should Call MMKV.initialize() first.')
        }
        let handle = native.getDefaultMMKV(mode ?? MMKV.SINGLE_PROCESS_MODE, cryptKey);
        if (!handle) {
            throw new Error('Fail to create the default MMKV instance in NAPI.');
        }
        return new MMKV(handle);
    }

    /**
     * Create an MMKV instance.
     * @param mmapID The unique ID of the MMKV instance.
     * @param mode The process mode of the MMKV instance, defaults to {@link MMKV.SINGLE_PROCESS_MODE}.
     * @param cryptKey The encryption key of the MMKV instance (no more than 16 bytes).
     * @param rootPath The folder of the MMKV instance, defaults to $(FilesDir)/mmkv.
     * @param expectedCapacity The file size you expected when opening or creating file.
     * @returns The MMKV instance.
     */
    public static mmkvWithID(mmapID: string, mode?: number, cryptKey?: string, rootPath?: string, expectedCapacity?: bigint): MMKV {
        if (!MMKV.g_rootDir) {
            throw new Error('You should Call MMKV.initialize() first.')
        }
        let handle = native.mmkvWithID(mmapID, mode ?? MMKV.SINGLE_PROCESS_MODE, cryptKey, rootPath, expectedCapacity);
        if (!handle) {
            throw new Error('Fail to create an MMKV instance [' + mmapID + '] in NAPI.');
        }
        return new MMKV(handle);
    }

    /**
     * @returns The unique ID of the MMKV instance.
     */
    public get mmapID(): string {
        return native.mmapID(this.nativeHandle);
    }

    /**
     * Set boolean value with customize expiration in seconds.
     *
     * @param expiration override the default duration, {@link MMKV.ExpireNever} (0) means never expire.
     * @returns true if success, false if failure.
     */
    public encodeBool(key: string, value: boolean, expiration?: number): boolean {
        return native.encodeBool(this.nativeHandle, key, value, expiration);
    }

    /**
     * Get boolean value.
     * @returns { boolean | undefined }
     */
    public decodeBool(key: string, defaultValue?: boolean): boolean | undefined {
        return native.decodeBool(this.nativeHandle, key, defaultValue ?? false);
    }

    /**
     * Set int32 value with customize expiration in seconds.
     * @param value { number }
     * @param expiration override the default duration, {@link MMKV.ExpireNever} (0) means never expire.
     * @returns true if success, false if failure.
     */
    public encodeInt32(key: string, value: number, expiration?: number): boolean {
        return native.encodeInt32(this.nativeHandle, key, value, expiration);
    }

    /**
     * Get int32 value.
     * @returns { number | undefined }
     */
    public decodeInt32(key: string, defaultValue?: number): number | undefined {
        return native.decodeInt32(this.nativeHandle, key, defaultValue ?? 0);
    }

    /**
     * Set uint32 value with customize expiration in seconds.
     * @param value { number }
     * @param expiration override the default duration, {@link MMKV.ExpireNever} (0) means never expire.
     * @returns true if success, false if failure.
     */
    public encodeUInt32(key: string, value: number, expiration?: number): boolean {
        return native.encodeUInt32(this.nativeHandle, key, value, expiration);
    }

    /**
     * Get uint32 value.
     * @returns { number | undefined }
     */
    public decodeUInt32(key: string, defaultValue?: number): number | undefined {
        return native.decodeUInt32(this.nativeHandle, key, defaultValue ?? 0);
    }

    /**
     * Set int64 value with customize expiration in seconds.
     * @param value { bigint }
     * @param expiration override the default duration, {@link MMKV.ExpireNever} (0) means never expire.
     * @returns true if success, false if failure.
     */
    public encodeInt64(key: string, value: bigint, expiration?: number): boolean {
        return native.encodeInt64(this.nativeHandle, key, value, expiration);
    }

    /**
     * Get int64 value.
     * @returns { bigint | undefined }
     */
    public decodeInt64(key: string, defaultValue?: bigint): bigint | undefined {
        return native.decodeInt64(this.nativeHandle, key, defaultValue ?? BigInt(0));
    }

    /**
     * Set uint64 value with customize expiration in seconds.
     * @param value { bigint }
     * @param expiration override the default duration, {@link MMKV.ExpireNever} (0) means never expire.
     * @returns true if success, false if failure.
     */
    public encodeUInt64(key: string, value: bigint, expiration?: number): boolean {
        return native.encodeUInt64(this.nativeHandle, key, value, expiration);
    }

    /**
     * Get uint64 value.
     * @returns { bigint | undefined }
     */
    public decodeUInt64(key: string, defaultValue?: bigint): bigint | undefined {
        return native.decodeUInt64(this.nativeHandle, key, defaultValue ?? BigInt(0));
    }

    /**
     * Set double value with customize expiration in seconds.
     * @param value { number }
     * @param expiration override the default duration, {@link MMKV.ExpireNever} (0) means never expire.
     * @returns true if success, false if failure.
     */
    public encodeDouble(key: string, value: number, expiration?: number): boolean {
        return native.encodeDouble(this.nativeHandle, key, value, expiration);
    }

    /**
     * Get double value.
     * @returns { number | undefined }
     */
    public decodeDouble(key: string, defaultValue?: number): number | undefined {
        return native.decodeDouble(this.nativeHandle, key, defaultValue ?? 0);
    }

    /**
     * Set string value with customize expiration in seconds.
     * @param value { string }
     * @param expiration override the default duration, {@link MMKV.ExpireNever} (0) means never expire.
     * @returns true if success, false if failure.
     */
    public encodeString(key: string, value: string, expiration?: number): boolean {
        return native.encodeString(this.nativeHandle, key, value, expiration);
    }

    /**
     * Get string value.
     * @returns { string | undefined }
     */
    public decodeString(key: string, defaultValue?: string): string | undefined {
        return native.decodeString(this.nativeHandle, key, defaultValue);
    }

    /**
     * Set string array value with customize expiration in seconds.
     * @param value { string[] }
     * @param expiration override the default duration, {@link MMKV.ExpireNever} (0) means never expire.
     * @returns true if success, false if failure.
     */
    public encodeStringSet(key: string, value: string[], expiration?: number): boolean {
        return native.encodeStringSet(this.nativeHandle, key, value, expiration);
    }

    /**
     * Get string array value.
     * @returns { string[] | undefined }
     */
    public decodeStringSet(key: string, defaultValue?: string[]): string[] | undefined {
        return native.decodeStringSet(this.nativeHandle, key, defaultValue);
    }

    /**
     * Set number array value with customize expiration in seconds.
     * @param value { number[] }
     * @param expiration override the default duration, {@link MMKV.ExpireNever} (0) means never expire.
     * @returns true if success, false if failure.
     */
    public encodeNumberSet(key: string, value: number[], expiration?: number): boolean {
        return native.encodeNumberSet(this.nativeHandle, key, value, expiration);
    }

    /**
     * Get number array value.
     * @returns { number[] | undefined }
     */
    public decodeNumberSet(key: string, defaultValue?: number[]) : number[] | undefined {
        return native.decodeNumberSet(this.nativeHandle, key, defaultValue);
    }

    /**
     * Set boolean array value with customize expiration in seconds.
     * @param value { boolean[] }
     * @param expiration override the default duration, {@link MMKV.ExpireNever} (0) means never expire.
     * @returns true if success, false if failure.
     */
    public encodeBoolSet(key: string, value: boolean[], expiration?: number): boolean {
        return native.encodeBoolSet(this.nativeHandle, key, value, expiration);
    }

    /**
     * Get boolean array value.
     * @returns { boolean[] | undefined }
     */
    public decodeBoolSet(key: string, defaultValue?: boolean[]): boolean[] | undefined {
        return native.decodeBoolSet(this.nativeHandle, key, defaultValue);
    }

    /**
     * Set ArrayBuffer value with customize expiration in seconds.
     * @param value { ArrayBuffer }
     * @param expiration override the default duration, {@link MMKV.ExpireNever} (0) means never expire.
     * @returns true if success, false if failure.
     */
    public encodeBytes(key: string, value: ArrayBuffer, expiration?: number): boolean {
        return native.encodeBytes(this.nativeHandle, key, value, expiration);
    }

    /**
     * Get ArrayBuffer value.
     * @returns { ArrayBuffer | undefined }
     */
    public decodeBytes(key: string, defaultValue?: ArrayBuffer): ArrayBuffer | undefined {
        return native.decodeBytes(this.nativeHandle, key, defaultValue);
    }

    /**
     * Check whether or not MMKV contains the key.
     *
     * @param key The key of the value.
     */
    public containsKey(key: string): boolean {
        return native.containsKey(this.nativeHandle, key);
    }

    /**
     * Get all the key.
     * @param filterExpire true to filter all non-expired keys. Note that this has costs.
     * @return { string[] }
     */
    public allKeys(filterExpire?: boolean): string[] {
        return native.allKeys(this.nativeHandle, filterExpire ?? false);
    }

    /**
     * @param filterExpire  true to filter all non-expired keys. Note that this has costs.
     * @return The total count of all the keys.
     */
    public count(filterExpire?: boolean): bigint {
        return native.count(this.nativeHandle, filterExpire ?? false);
    }

    /**
     * Remove the value for one key.
     */
    public removeValueForKey(key: string): void {
        native.removeValueForKey(this.nativeHandle, key);
    }

    /**
     * Batch remove some keys from the MMKV instance.
     *
     * @param arrKeys { string[] } The keys to be removed.
     */
    public removeValuesForKeys(keys: string[]): void {
        native.removeValuesForKeys(this.nativeHandle, keys);
    }

    /**
     * Clear all the key-values inside the MMKV instance.
     * The data file will be trimmed down to `pageSize`, and some sync operations will be called
     * @param keepSpace If you do not want to trim the file, pass true for better performance
     */
    public clearAll(keepSpace?: boolean): void {
        return native.clearAll(this.nativeHandle, keepSpace ?? false);
    }

    /**
     * Get the size of the underlying file. Align to the disk block size, typically 4K for an Android device.
     */
    public totalSize(): bigint {
        return native.totalSize(this.nativeHandle);
    }

    /**
     * Get the actual used size of the MMKV instance.
     * This size might increase and decrease as MMKV doing insertion and full write back.
     */
    public actualSize(): bigint {
        return native.actualSize(this.nativeHandle);
    }

    /**
     * Save all mmap memory to file.
     * You don't need to call this, really, I mean it.
     * Unless you worry about the device running out of battery.
     * @param sync true for synchronously sync, false for asynchronously sync
     */
    public sync(sync?: boolean): void {
        return native.sync(this.nativeHandle, sync ?? false);
    }

    /**
     * Exclusively inter-process lock the MMKV instance.
     * It will block and wait until it successfully locks the file.
     * It will make no effect if the MMKV instance is created with {@link #SINGLE_PROCESS_MODE}.
     */
    public lock(): void {
        return native.lock(this.nativeHandle);
    }

    /**
     * Exclusively inter-process unlock the MMKV instance.
     * It will make no effect if the MMKV instance is created with {@link #SINGLE_PROCESS_MODE}.
     */
    public unlock(): void {
        return native.unlock(this.nativeHandle);
    }

    /**
     * Try exclusively inter-process lock the MMKV instance.
     * It will not block if the file has already been locked by another process.
     * It will make no effect if the MMKV instance is created with {@link #SINGLE_PROCESS_MODE}.
     *
     * @return true if successfully locked, otherwise return immediately with false.
     */
    public tryLock(): boolean {
        return native.tryLock(this.nativeHandle);
    }

    /**
     * The {@link MMKV.totalSize()} of an MMKV instance won't reduce after deleting key-values,
     * call this method after lots of deleting if you care about disk usage.
     * Note that {@link MMKV.clearAll()}  has a similar effect.
     */
    public trim(): void {
        return native.trim(this.nativeHandle);
    }

    /**
     * Call this method if the MMKV instance is no longer needed in the near future.
     * Any subsequent call to any MMKV instances with the same ID is undefined behavior.
     */
    public close(): void {
        return native.close(this.nativeHandle);
    }

    /**
     * Clear memory cache of the MMKV instance.
     * You can call it on memory warning.
     * Any subsequent call to the MMKV instance will trigger all key-values loading from the file again.
     */
    public clearMemoryCache(): void {
        return native.clearMemoryCache(this.nativeHandle);
    }

    /**
     * Get the actual size consumption of the key's value.
     * Note: might be a little bigger than value's length.
     * @param key The key of the value.
     * @param actualSize true to get the actual size of the key's value. String's length or byte[]'s length, etc.
     */
    public getValueSize(key: string, actualSize?: boolean): number {
        return native.getValueSize(this.nativeHandle, key, actualSize);
    }

    /**
     * remove the storage of the MMKV, including the data file & meta file (.crc)
     * Note: the existing instance (if any) will be closed & destroyed
     */
    public static removeStorage(mmapID: string, rootPath?: string): boolean {
        return native.removeStorage(mmapID, rootPath);
    }

    /**
     * Check whether the MMKV file is valid or not on customize folder.
     * @param mmapID   The unique ID of the MMKV instance.
     * @param rootPath The folder of the MMKV instance, defaults to $(FilesDir)/mmkv.
     */
    public static isFileValid(mmapID: string, rootPath?: string): boolean {
        return native.isFileValid(mmapID, rootPath);
    }

    /**
     * @return The encryption key (no more than 16 bytes).
     */
    public get cryptKey(): string | undefined {
        return native.cryptKey(this.nativeHandle);
    }

    /**
     * Transform plain text into encrypted text, or vice versa by passing a null encryption key.
     * You can also change existing crypt key with a different cryptKey.
     *
     * @param cryptKey The new encryption key (no more than 16 bytes).
     * @return true if success, otherwise false.
     */
    public reKey(newKey?: string): boolean {
        return native.reKey(this.nativeHandle, newKey);
    }

    /**
     * Just reset the encryption key (will not encrypt or decrypt anything).
     * Usually you should call this method after another process has {@link MMKV.reKey(String)} the multi-process MMKV instance.
     *
     * @param cryptKey The new encryption key (no more than 16 bytes).
     */
    public checkReSetCryptKey(newKey?: string): void {
        return native.checkReSetCryptKey(this.nativeHandle, newKey);
    }

    /**
     * Atomically migrate all key-values from an existent Preferences to the MMKV instance.
     * @param preferences The SharedPreferences to import from.
     * @return The total count of key-values imported.
     */
    public importFromPreferences(preferences: dataPreferences.Preferences): bigint {
        hilog.info(0x0000, 'mmkv', 'import from Preferences to %{public}s', this.mmapID);
        let all = preferences.getAllSync();
        let allKeys = getObjKeys(all);
        let allKV = all as Record<string, Object>;
        let count = BigInt(0);
        for (let index = 0; index < allKeys.length; index++) {
            const key = allKeys[index];
            const value = allKV[key];
            if (typeof value == 'string') {
                this.encodeString(key, value as string);
            } else if (typeof value == 'boolean') {
                this.encodeBool(key, value as boolean);
            } else if (typeof value == 'number') {
                // there's no way to tell whether a number is integer or float/double
                this.encodeDouble(key, value as number);
            } else if (typeof value == 'object') {
                if (Array.isArray(value)) {
                    if (value.length == 0) {
                        // assume it's Array<string>, the type doesn't really matter for an empty Array
                        this.encodeStringSet(key, value as string[]);
                    } else {
                        // Array<number>、Array<string>、Array<boolean>、Uint8Array
                        let first: Object = value[0];
                        if (typeof first == 'string') {
                            let arrStr = value as string[];
                            this.encodeStringSet(key, arrStr);
                        } else if (typeof first == 'boolean') {
                            let arrBool = value as boolean[];
                            this.encodeBoolSet(key, arrBool);
                        } else if (typeof first == 'number') {
                            let arrNumber = value as number[];
                            this.encodeNumberSet(key, arrNumber);
                        } else {
                            hilog.warn(0x0000, 'mmkv', 'unsupported Array<%{public}s> type on key %{public}s',
                                typeof first, key);
                            continue;
                        }
                    }
                } else if (value instanceof Uint8Array) {
                    let arr = value as Uint8Array;
                    this.encodeBytes(key, arr.buffer as ArrayBuffer);
                } else {
                    hilog.warn(0x0000, 'mmkv', 'unsupported Object type on key %{public}s', key);
                    continue;
                }
            } else {
                hilog.warn(0x0000, 'mmkv', 'unsupported %{public}s type on key %{public}s', typeof value, key);
                continue;
            }
            count++;
        }
        return count;
    }

    /**
     * Get an backed-up MMKV instance.
     * @param mmapID   The unique ID of the MMKV instance.
     * @param rootPath The backup folder of the MMKV instance.
     * @param mode     The process mode of the MMKV instance, defaults to {@link #SINGLE_PROCESS_MODE}.
     * @param cryptKey The encryption key of the MMKV instance (no more than 16 bytes).
     */
    public static backedUpMMKVWithID(mmapID: string, rootPath: string, mode?: number, cryptKey?: string): MMKV {
        if (!MMKV.g_rootDir) {
            throw new Error('You should Call MMKV.initialize() first.')
        }
        let realMode = (mode ?? MMKV.SINGLE_PROCESS_MODE) | MMKV.BACKUP_MODE
        // hilog.info(0x0000, 'mmkv', 'realMode %{public}d for mmapID %{public}s', realMode, mmapID);
        return MMKV.mmkvWithID(mmapID, realMode, cryptKey, rootPath);
    }

    /**
     * backup one MMKV instance to dstDir
     * @param mmapID   the MMKV ID to backup
     * @param dstDir   the backup destination directory
     * @param rootPath the customize root path of the MMKV, if null then backup from the root dir of MMKV
     */
    public static backupOneToDirectory(mmapID: string, dstDir: string, rootPath?: string): boolean {
        if (!MMKV.g_rootDir) {
            throw new Error('You should Call MMKV.initialize() first.')
        }
        return native.backupOneToDirectory(mmapID, dstDir, rootPath);
    }

    /**
     * restore one MMKV instance from srcDir
     * @param mmapID   the MMKV ID to restore
     * @param srcDir   the restore source directory
     * @param rootPath the customize root path of the MMKV, if null then restore to the root dir of MMKV
     */
    public static restoreOneFromDirectory(mmapID: string, srcDir: string, rootPath?: string): boolean {
        if (!MMKV.g_rootDir) {
            throw new Error('You should Call MMKV.initialize() first.')
        }
        return native.restoreOneFromDirectory(mmapID, srcDir, rootPath);
    }

    /**
     * backup all MMKV instance to dstDir
     * @param dstDir the backup destination directory
     * @return count of MMKV successfully backup
     */
    public static backupAllToDirectory(dstDir: string): bigint {
        if (!MMKV.g_rootDir) {
            throw new Error('You should Call MMKV.initialize() first.')
        }
        return native.backupAllToDirectory(dstDir);
    }

    /**
     * restore all MMKV instance from srcDir
     * @param srcDir the restore source directory
     * @return count of MMKV successfully restored
     */
    public static restoreAllFromDirectory(srcDir: string): bigint {
        if (!MMKV.g_rootDir) {
            throw new Error('You should Call MMKV.initialize() first.')
        }
        return native.restoreAllFromDirectory(srcDir);
    }

    public static readonly ExpireNever = 0;
    public static readonly ExpireInMinute = 60;
    public static readonly ExpireInHour = 60 * 60;
    public static readonly ExpireInDay = 24 * 60 * 60;
    public static readonly ExpireInMonth = 30 * 24 * 60 * 60;
    public static readonly ExpireInYear = 365 * 30 * 24 * 60 * 60;

    /**
     * Enable auto key expiration. This is a upgrade operation, the file format will change.
     * And the file won't be accessed correctly by older version (v1.2.16) of MMKV.
     * NOTICE: enableCompareBeforeSet will be invalid when Expiration is on
     * @param expireDurationInSecond the expire duration for all keys, {@link MMKV.ExpireNever} (0) means no default duration
     * (aka each key will have it's own expire date)
     */
    public enableAutoKeyExpire(expireDurationInSecond: number): boolean {
        return native.enableAutoKeyExpire(this.nativeHandle, expireDurationInSecond);
    }

    /**
     * Disable auto key expiration. This is a downgrade operation.
     */
    public disableAutoKeyExpire(): boolean {
        return native.disableAutoKeyExpire(this.nativeHandle);
    }

    /**
     * Enable data compare before set, for better performance.
     * If data for key seldom changes, use it.
     * When encryption or expiration is on, compare-before-set will be invalid.
     * For encryption, compare operation must decrypt data which is time consuming.
     * For expiration, compare is useless because in most cases the expiration time changes every time.
     */
    public enableCompareBeforeSet(): void {
        return native.enableCompareBeforeSet(this.nativeHandle);
    }

    /**
     * Disable data compare before set
     * disabled at default
     */
    public disableCompareBeforeSet(): void {
        return native.disableCompareBeforeSet(this.nativeHandle);
    }

    /**
     * Create an MMKV instance base on Anonymous Shared Memory, aka not synced to any disk files.
     *
     * @param context  The context of Android App, usually from Application.
     * @param mmapID   The unique ID of the MMKV instance.
     * @param size     The maximum size of the underlying Anonymous Shared Memory.
     *                 Anonymous Shared Memory on Android can't grow dynamically, must set an appropriate size on creation.
     * @param mode     The process mode of the MMKV instance, defaults to {@link #SINGLE_PROCESS_MODE}.
     * @param cryptKey The encryption key of the MMKV instance (no more than 16 bytes).
     */
    public static mmkvWithAshmemID(mmapID: string, size: number, mode: number, cryptKey?: string): MMKV {
        let handle = native.mmkvWithIDAndSize(mmapID, size, (mode | MMKV.ASHMEM_MODE), cryptKey);
        if (!handle) {
            throw new Error('Fail to create an MMKV instance [' + mmapID + '] in NAPI.');
        }
        return new MMKV(handle);
    }

    /**
     * Get an ashmem MMKV instance that has been initiated by another process.
     * Normally you should just call {@link #mmkvWithAshmemID(Context, String, int, int, String)} instead.
     *
     * @param mmapID   The unique ID of the MMKV instance.
     * @param fd       The file descriptor of the ashmem of the MMKV file, transferred from another process by binder.
     * @param metaFD   The file descriptor of the ashmem of the MMKV crc file, transferred from another process by binder.
     * @param cryptKey The encryption key of the MMKV instance (no more than 16 bytes).
     */
    public static mmkvWithAshmemFD(mmapID: string, fd: number, metaFD: number, cryptKey?: string): MMKV {
        let handle = native.mmkvWithAshmemFD(mmapID, fd, metaFD, cryptKey);
        if (!handle) {
            throw new Error('Fail to create an MMKV instance [' + mmapID + '] in NAPI.');
        }
        return new MMKV(handle);
    }

    /**
     * @return The file descriptor of the ashmem of the MMKV file.
     */
    public get ashmemFD(): number {
        return native.ashmemFD(this.nativeHandle);
    }

    /**
     * @return The file descriptor of the ashmem of the MMKV crc file.
     */
    public get ashmemMetaFD(): number {
        return native.ashmemMetaFD(this.nativeHandle);
    }

    /**
     * Create an native buffer, whose underlying memory can be directly transferred to another JNI method.
     * Avoiding unnecessary JNI boxing and unboxing.
     * An NativeBuffer must be manually {@link MMKV.destroyNativeBuffer()} to avoid memory leak.
     *
     * @param size The size of the underlying memory.
     */
    public static createNativeBuffer(size: number): NativeBuffer | null {
        let pointer: bigint = native.createNativeBuffer(size);
        if (pointer <= BigInt(0)) {
            return null;
        }
        return new NativeBuffer(pointer, size);
    }

    /**
     * Destroy the native buffer. An NativeBuffer must be manually destroy to avoid memory leak.
     */
    public static destroyNativeBuffer(buffer: NativeBuffer): void {
        native.destroyNativeBuffer(buffer.pointer, buffer.size);
    }

    /**
     * Write the value of the key to the native buffer.
     *
     * @return The size written. Return -1 on any error.
     */
    public writeValueToNativeBuffer(key: string, buffer: NativeBuffer): number {
        return native.writeValueToNativeBuffer(this.nativeHandle, key, buffer.pointer, buffer.size);
    }
}
