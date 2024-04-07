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

export class MMKV {
    static SINGLE_PROCESS_MODE: number = 1 << 0
    static MULTI_PROCESS_MODE: number = 1 << 1
    private static ASHMEM_MODE: number = 1 << 3
    private static BACKUP_MODE: number = 1 << 4

    private  static g_rootDir: string

    private static initData() {
        // MMKV.checkedHandleSet = new Set<string>()
    }

    static initialize(rootDir:string, cacheDir:string, logLevel?:MMKVLogLevel) {
        MMKV.initData();
        // hilog.info(0x0000, 'mmkv', '%{public}s, %{public}s, %{public}d', rootDir, cacheDir, logLevel);
        MMKV.g_rootDir = native.initialize(rootDir, cacheDir, logLevel);
        return MMKV.g_rootDir;
    }

    static get version() {
        return native.version();
    }

    static get rootDir() {
        return MMKV.g_rootDir;
    }

    private nativeHandle: bigint;
    private constructor(handle: bigint) {
        this.nativeHandle = handle;
    }

    static defaultMMKV() {
        let handle = native.getDefaultMMKV(MMKV.SINGLE_PROCESS_MODE);
        // hilog.info(0x0000, 'mmkv', 'default mmkv: %{public}i', handle);
        return new MMKV(handle);
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

    removeValueForKey(key: string) {
        native.removeValueForKey(this.nativeHandle, key);
    }

    removeValuesForKeys(keys: string[]) {
        native.removeValuesForKeys(this.nativeHandle, keys);
    }

    count(filterExpire?: boolean) {
        return native.count(this.nativeHandle, filterExpire ?? false);
    }
}
