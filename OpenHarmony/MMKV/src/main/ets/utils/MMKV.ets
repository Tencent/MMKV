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
import common from '@ohos.app.ability.common';
import dataPreferences from '@ohos.data.preferences';

export class MMKV {
    static readonly SINGLE_PROCESS_MODE: number = 1 << 0
    static readonly MULTI_PROCESS_MODE: number = 1 << 1
    private static readonly ASHMEM_MODE: number = 1 << 3
    private static readonly BACKUP_MODE: number = 1 << 4

    private  static g_rootDir: string

    private static initData() {
        // MMKV.checkedHandleSet = new Set<string>()
    }

    static initialize(context: common.ApplicationContext, logLevel?:MMKVLogLevel): string {
        let rootDir = context.filesDir + "/mmkv";
        let cacheDir = context.cacheDir;
        // hilog.info(0x0000, 'mmkv', '%{public}s, %{public}s, %{public}d', rootDir, cacheDir, logLevel);
        return MMKV.initializeWithPath(rootDir, cacheDir, logLevel);
    }

    static initializeWithPath(rootDir:string, cacheDir:string, logLevel?:MMKVLogLevel): string {
        MMKV.initData();
        // hilog.info(0x0000, 'mmkv', '%{public}s, %{public}s, %{public}d', rootDir, cacheDir, logLevel);
        MMKV.g_rootDir = native.initialize(rootDir, cacheDir, logLevel);
        return MMKV.g_rootDir;
    }

    static get version() {
        return native.version();
    }

    static get pageSize() {
        return native.pageSize();
    }

    static get rootDir() {
        return MMKV.g_rootDir;
    }

    private nativeHandle: bigint;
    private constructor(handle: bigint) {
        this.nativeHandle = handle;
    }

    static defaultMMKV(mode?: number, cryptKey?: string) {
        let handle = native.getDefaultMMKV(mode ?? MMKV.SINGLE_PROCESS_MODE, cryptKey);
        return new MMKV(handle);
    }

    static mmkvWithID(mmapID: string, mode?: number, cryptKey?: string, rootPath?: string, expectedCapacity?: bigint) {
        let handle = native.mmkvWithID(mmapID, mode ?? MMKV.SINGLE_PROCESS_MODE, cryptKey, rootPath, expectedCapacity);
        return new MMKV(handle);
    }

    get mmapID () {
        return native.mmapID(this.nativeHandle);
    }

    encodeBool(key: string, value: boolean, expiration?: number) {
        return native.encodeBool(this.nativeHandle, key, value, expiration);
    }

    decodeBool(key: string, defaultValue?: boolean) {
        return native.decodeBool(this.nativeHandle, key, defaultValue ?? false);
    }

    encodeInt32(key: string, value: number, expiration?: number) {
        return native.encodeInt32(this.nativeHandle, key, value, expiration);
    }

    decodeInt32(key: string, defaultValue?: number) {
        return native.decodeInt32(this.nativeHandle, key, defaultValue ?? 0);
    }

    encodeUInt32(key: string, value: number, expiration?: number) {
        return native.encodeUInt32(this.nativeHandle, key, value, expiration);
    }

    decodeUInt32(key: string, defaultValue?: number) {
        return native.decodeUInt32(this.nativeHandle, key, defaultValue ?? 0);
    }

    encodeInt64(key: string, value: bigint, expiration?: number) {
        return native.encodeInt64(this.nativeHandle, key, value, expiration);
    }

    decodeInt64(key: string, defaultValue?: bigint) {
        return native.decodeInt64(this.nativeHandle, key, defaultValue ?? BigInt(0));
    }

    encodeUInt64(key: string, value: bigint, expiration?: number) {
        return native.encodeUInt64(this.nativeHandle, key, value, expiration);
    }

    decodeUInt64(key: string, defaultValue?: bigint) {
        return native.decodeUInt64(this.nativeHandle, key, defaultValue ?? BigInt(0));
    }

    encodeDouble(key: string, value: number, expiration?: number) {
        return native.encodeDouble(this.nativeHandle, key, value, expiration);
    }

    decodeDouble(key: string, defaultValue?: number) {
        return native.decodeDouble(this.nativeHandle, key, defaultValue ?? 0);
    }

    encodeString(key: string, value: string, expiration?: number) {
        return native.encodeString(this.nativeHandle, key, value, expiration);
    }

    decodeString(key: string, defaultValue?: string) {
        return native.decodeString(this.nativeHandle, key, defaultValue);
    }

    encodeStringSet(key: string, value: string[], expiration?: number) {
        return native.encodeStringSet(this.nativeHandle, key, value, expiration);
    }

    decodeStringSet(key: string, defaultValue?: string[]) {
        return native.decodeStringSet(this.nativeHandle, key, defaultValue);
    }

    encodeNumberSet(key: string, value: number[], expiration?: number) {
        return native.encodeNumberSet(this.nativeHandle, key, value, expiration);
    }

    decodeNumberSet(key: string, defaultValue?: number[]) {
        return native.decodeNumberSet(this.nativeHandle, key, defaultValue);
    }

    encodeBoolSet(key: string, value: boolean[], expiration?: number) {
        return native.encodeBoolSet(this.nativeHandle, key, value, expiration);
    }

    decodeBoolSet(key: string, defaultValue?: boolean[]) {
        return native.decodeBoolSet(this.nativeHandle, key, defaultValue);
    }

    encodeBytes(key: string, value: ArrayBuffer, expiration?: number) {
        return native.encodeBytes(this.nativeHandle, key, value, expiration);
    }

    decodeBytes(key: string, defaultValue?: ArrayBuffer) {
        return native.decodeBytes(this.nativeHandle, key, defaultValue);
    }

    containsKey(key: string) {
        return native.containsKey(this.nativeHandle, key);
    }

    allKeys(filterExpire?: boolean) {
        return native.allKeys(this.nativeHandle, filterExpire ?? false);
    }

    count(filterExpire?: boolean) {
        return native.count(this.nativeHandle, filterExpire ?? false);
    }

    removeValueForKey(key: string) {
        native.removeValueForKey(this.nativeHandle, key);
    }

    removeValuesForKeys(keys: string[]) {
        native.removeValuesForKeys(this.nativeHandle, keys);
    }

    clearAll(keepSpace?: boolean) {
        return native.clearAll(this.nativeHandle, keepSpace ?? false);
    }

    totalSize() {
        return native.totalSize(this.nativeHandle);
    }

    actualSize() {
        return native.actualSize(this.nativeHandle);
    }

    sync(sync?: boolean) {
        return native.sync(this.nativeHandle, sync ?? false);
    }

    clearMemoryCache() {
        return native.clearMemoryCache(this.nativeHandle);
    }

    lock() {
        return native.lock(this.nativeHandle);
    }

    unlock() {
        return native.unlock(this.nativeHandle);
    }

    tryLock() {
        return native.tryLock(this.nativeHandle);
    }

    trim() {
        return native.trim(this.nativeHandle);
    }

    close() {
        return native.close(this.nativeHandle);
    }

    getValueSize(key: string, actualSize?: boolean) {
        return native.getValueSize(this.nativeHandle, key, actualSize);
    }

    static removeStorage(mmapID: string, rootPath?: string) {
        return native.removeStorage(mmapID, rootPath);
    }

    static isFileValid(mmapID: string, rootPath?: string) {
        return native.isFileValid(mmapID, rootPath);
    }

    get cryptKey() {
        return native.cryptKey(this.nativeHandle);
    }

    reKey(newKey?: string) {
        return native.reKey(this.nativeHandle, newKey);
    }

    checkReSetCryptKey(newKey?: string) {
        return native.checkReSetCryptKey(this.nativeHandle, newKey);
    }

    importFromPreferences(preferences: dataPreferences.Preferences) {
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

    static backedUpMMKVWithID(mmapID: string, rootPath: string, mode?: number, cryptKey?: string): MMKV {
        let realMode = (mode ?? MMKV.SINGLE_PROCESS_MODE) | MMKV.BACKUP_MODE
        // hilog.info(0x0000, 'mmkv', 'realMode %{public}d for mmapID %{public}s', realMode, mmapID);
        return MMKV.mmkvWithID(mmapID, realMode, cryptKey, rootPath);
    }

    static backupOneToDirectory(mmapID: string, dstDir: string, rootPath?: string) {
        return native.backupOneToDirectory(mmapID, dstDir, rootPath);
    }

    static restoreOneFromDirectory(mmapID: string, srcDir: string, rootPath?: string) {
        return native.restoreOneFromDirectory(mmapID, srcDir, rootPath);
    }

    static backupAllToDirectory(dstDir: string) {
        return native.backupAllToDirectory(dstDir);
    }

    static restoreAllFromDirectory(srcDir: string) {
        return native.restoreAllFromDirectory(srcDir);
    }

    static readonly ExpireNever = 0;
    static readonly ExpireInMinute = 60;
    static readonly ExpireInHour = 60 * 60;
    static readonly ExpireInDay = 24 * 60 * 60;
    static readonly ExpireInMonth = 30 * 24 * 60 * 60;
    static readonly ExpireInYear = 365 * 30 * 24 * 60 * 60;

    enableAutoKeyExpire(expireDurationInSecond: number) {
        return native.enableAutoKeyExpire(this.nativeHandle, expireDurationInSecond);
    }

    disableAutoKeyExpire() {
        return native.disableAutoKeyExpire(this.nativeHandle);
    }

    enableCompareBeforeSet() {
        return native.enableCompareBeforeSet(this.nativeHandle);
    }

    disableCompareBeforeSet() {
        return native.disableCompareBeforeSet(this.nativeHandle);
    }
}
