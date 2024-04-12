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

export class MMKV {
    public static readonly SINGLE_PROCESS_MODE: number = 1 << 0
    public static readonly MULTI_PROCESS_MODE: number = 1 << 1
    private static readonly ASHMEM_MODE: number = 1 << 3
    private static readonly BACKUP_MODE: number = 1 << 4
    private static g_rootDir: string

    private static initData() {
        // MMKV.checkedHandleSet = new Set<string>()
    }

    public static initialize(context: common.ApplicationContext, logLevel?: MMKVLogLevel): string {
        let rootDir = context.filesDir + "/mmkv";
        let cacheDir = context.cacheDir;
        // hilog.info(0x0000, 'mmkv', '%{public}s, %{public}s, %{public}d', rootDir, cacheDir, logLevel);
        return MMKV.initializeWithPath(rootDir, cacheDir, logLevel);
    }

    public static initializeWithPath(rootDir: string, cacheDir: string, logLevel?: MMKVLogLevel): string {
        MMKV.initData();
        // hilog.info(0x0000, 'mmkv', '%{public}s, %{public}s, %{public}d', rootDir, cacheDir, logLevel);
        MMKV.g_rootDir = native.initialize(rootDir, cacheDir, logLevel);
        return MMKV.g_rootDir;
    }

    public static get version(): string {
        return native.version();
    }

    public static get pageSize(): number {
        return native.pageSize();
    }

    public static get rootDir(): string {
        return MMKV.g_rootDir;
    }

    private nativeHandle: bigint;

    private constructor(handle: bigint) {
        this.nativeHandle = handle;
    }

    public static defaultMMKV(mode?: number, cryptKey?: string): MMKV {
        if (!MMKV.g_rootDir) {
            throw new Error('You should Call MMKV.initialize() first.')
        }
        let handle = native.getDefaultMMKV(mode ?? MMKV.SINGLE_PROCESS_MODE, cryptKey);
        if (!handle) {
            throw new Error('Fail to create an MMKV instance [' + mmapID + '] in NAPI.');
        }
        return new MMKV(handle);
    }

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

    public get mmapID(): string {
        return native.mmapID(this.nativeHandle);
    }

    public encodeBool(key: string, value: boolean, expiration?: number): boolean {
        return native.encodeBool(this.nativeHandle, key, value, expiration);
    }

    public decodeBool(key: string, defaultValue?: boolean): boolean | undefined {
        return native.decodeBool(this.nativeHandle, key, defaultValue ?? false);
    }

    public encodeInt32(key: string, value: number, expiration?: number): boolean {
        return native.encodeInt32(this.nativeHandle, key, value, expiration);
    }

    public decodeInt32(key: string, defaultValue?: number): number | undefined {
        return native.decodeInt32(this.nativeHandle, key, defaultValue ?? 0);
    }

    public encodeUInt32(key: string, value: number, expiration?: number): boolean {
        return native.encodeUInt32(this.nativeHandle, key, value, expiration);
    }

    public decodeUInt32(key: string, defaultValue?: number): number | undefined {
        return native.decodeUInt32(this.nativeHandle, key, defaultValue ?? 0);
    }

    public encodeInt64(key: string, value: bigint, expiration?: number): boolean {
        return native.encodeInt64(this.nativeHandle, key, value, expiration);
    }

    public decodeInt64(key: string, defaultValue?: bigint): bigint | undefined {
        return native.decodeInt64(this.nativeHandle, key, defaultValue ?? BigInt(0));
    }

    public encodeUInt64(key: string, value: bigint, expiration?: number): boolean {
        return native.encodeUInt64(this.nativeHandle, key, value, expiration);
    }

    public decodeUInt64(key: string, defaultValue?: bigint): bigint | undefined {
        return native.decodeUInt64(this.nativeHandle, key, defaultValue ?? BigInt(0));
    }

    public encodeDouble(key: string, value: number, expiration?: number): boolean {
        return native.encodeDouble(this.nativeHandle, key, value, expiration);
    }

    public decodeDouble(key: string, defaultValue?: number): number | undefined {
        return native.decodeDouble(this.nativeHandle, key, defaultValue ?? 0);
    }

    public encodeString(key: string, value: string, expiration?: number): boolean {
        return native.encodeString(this.nativeHandle, key, value, expiration);
    }

    public decodeString(key: string, defaultValue?: string): string | undefined {
        return native.decodeString(this.nativeHandle, key, defaultValue);
    }

    public encodeStringSet(key: string, value: string[], expiration?: number): boolean {
        return native.encodeStringSet(this.nativeHandle, key, value, expiration);
    }

    public decodeStringSet(key: string, defaultValue?: string[]): string[] | undefined {
        return native.decodeStringSet(this.nativeHandle, key, defaultValue);
    }

    public encodeNumberSet(key: string, value: number[], expiration?: number): boolean {
        return native.encodeNumberSet(this.nativeHandle, key, value, expiration);
    }

    public decodeNumberSet(key: string, defaultValue?: number[]) : number[] | undefined {
        return native.decodeNumberSet(this.nativeHandle, key, defaultValue);
    }

    public encodeBoolSet(key: string, value: boolean[], expiration?: number): boolean {
        return native.encodeBoolSet(this.nativeHandle, key, value, expiration);
    }

    public decodeBoolSet(key: string, defaultValue?: boolean[]): boolean[] | undefined {
        return native.decodeBoolSet(this.nativeHandle, key, defaultValue);
    }

    public encodeBytes(key: string, value: ArrayBuffer, expiration?: number): boolean {
        return native.encodeBytes(this.nativeHandle, key, value, expiration);
    }

    public decodeBytes(key: string, defaultValue?: ArrayBuffer): ArrayBuffer | undefined {
        return native.decodeBytes(this.nativeHandle, key, defaultValue);
    }

    public containsKey(key: string): boolean {
        return native.containsKey(this.nativeHandle, key);
    }

    public allKeys(filterExpire?: boolean): string[] {
        return native.allKeys(this.nativeHandle, filterExpire ?? false);
    }

    public count(filterExpire?: boolean): bigint {
        return native.count(this.nativeHandle, filterExpire ?? false);
    }

    public removeValueForKey(key: string): void {
        native.removeValueForKey(this.nativeHandle, key);
    }

    public removeValuesForKeys(keys: string[]): void {
        native.removeValuesForKeys(this.nativeHandle, keys);
    }

    public clearAll(keepSpace?: boolean): void {
        return native.clearAll(this.nativeHandle, keepSpace ?? false);
    }

    public totalSize(): bigint {
        return native.totalSize(this.nativeHandle);
    }

    public actualSize(): bigint {
        return native.actualSize(this.nativeHandle);
    }

    public sync(sync?: boolean): void {
        return native.sync(this.nativeHandle, sync ?? false);
    }

    public clearMemoryCache(): void {
        return native.clearMemoryCache(this.nativeHandle);
    }

    public lock(): void {
        return native.lock(this.nativeHandle);
    }

    public unlock(): void {
        return native.unlock(this.nativeHandle);
    }

    public tryLock(): boolean {
        return native.tryLock(this.nativeHandle);
    }

    public trim(): void {
        return native.trim(this.nativeHandle);
    }

    public close(): void {
        return native.close(this.nativeHandle);
    }

    public getValueSize(key: string, actualSize?: boolean): number {
        return native.getValueSize(this.nativeHandle, key, actualSize);
    }

    public static removeStorage(mmapID: string, rootPath?: string): boolean {
        return native.removeStorage(mmapID, rootPath);
    }

    public static isFileValid(mmapID: string, rootPath?: string): boolean {
        return native.isFileValid(mmapID, rootPath);
    }

    public get cryptKey(): string | undefined {
        return native.cryptKey(this.nativeHandle);
    }

    public reKey(newKey?: string): boolean {
        return native.reKey(this.nativeHandle, newKey);
    }

    public checkReSetCryptKey(newKey?: string): void {
        return native.checkReSetCryptKey(this.nativeHandle, newKey);
    }

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

    public static backedUpMMKVWithID(mmapID: string, rootPath: string, mode?: number, cryptKey?: string): MMKV {
        if (!MMKV.g_rootDir) {
            throw new Error('You should Call MMKV.initialize() first.')
        }
        let realMode = (mode ?? MMKV.SINGLE_PROCESS_MODE) | MMKV.BACKUP_MODE
        // hilog.info(0x0000, 'mmkv', 'realMode %{public}d for mmapID %{public}s', realMode, mmapID);
        return MMKV.mmkvWithID(mmapID, realMode, cryptKey, rootPath);
    }

    public static backupOneToDirectory(mmapID: string, dstDir: string, rootPath?: string): boolean {
        if (!MMKV.g_rootDir) {
            throw new Error('You should Call MMKV.initialize() first.')
        }
        return native.backupOneToDirectory(mmapID, dstDir, rootPath);
    }

    public static restoreOneFromDirectory(mmapID: string, srcDir: string, rootPath?: string): boolean {
        if (!MMKV.g_rootDir) {
            throw new Error('You should Call MMKV.initialize() first.')
        }
        return native.restoreOneFromDirectory(mmapID, srcDir, rootPath);
    }

    public static backupAllToDirectory(dstDir: string): bigint {
        if (!MMKV.g_rootDir) {
            throw new Error('You should Call MMKV.initialize() first.')
        }
        return native.backupAllToDirectory(dstDir);
    }

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

    public enableAutoKeyExpire(expireDurationInSecond: number): boolean {
        return native.enableAutoKeyExpire(this.nativeHandle, expireDurationInSecond);
    }

    public disableAutoKeyExpire(): boolean {
        return native.disableAutoKeyExpire(this.nativeHandle);
    }

    public enableCompareBeforeSet(): void {
        return native.enableCompareBeforeSet(this.nativeHandle);
    }

    public disableCompareBeforeSet(): void {
        return native.disableCompareBeforeSet(this.nativeHandle);
    }

    public static mmkvWithAshmemID(mmapID: string, size: number, mode: number, cryptKey?: string): MMKV {
        let handle = native.mmkvWithIDAndSize(mmapID, size, (mode | MMKV.ASHMEM_MODE), cryptKey);
        if (!handle) {
            throw new Error('Fail to create an MMKV instance [' + mmapID + '] in NAPI.');
        }
        return new MMKV(handle);
    }

    public static mmkvWithAshmemFD(mmapID: string, fd: number, metaFD: number, cryptKey?: string): MMKV {
        let handle = native.mmkvWithAshmemFD(mmapID, fd, metaFD, cryptKey);
        if (!handle) {
            throw new Error('Fail to create an MMKV instance [' + mmapID + '] in NAPI.');
        }
        return new MMKV(handle);
    }

    public get ashmemFD(): number {
        return native.ashmemFD(this.nativeHandle);
    }

    public get ashmemMetaFD(): number {
        return native.ashmemMetaFD(this.nativeHandle);
    }

    public static createNativeBuffer(size: number): NativeBuffer | null {
        let pointer: bigint = native.createNativeBuffer(size);
        if (pointer <= BigInt(0)) {
            return null;
        }
        return new NativeBuffer(pointer, size);
    }

    public static destroyNativeBuffer(buffer: NativeBuffer): void {
        native.destroyNativeBuffer(buffer.pointer, buffer.size);
    }

    public writeValueToNativeBuffer(key: string, buffer: NativeBuffer): number {
        return native.writeValueToNativeBuffer(this.nativeHandle, key, buffer.pointer, buffer.size);
    }
}
