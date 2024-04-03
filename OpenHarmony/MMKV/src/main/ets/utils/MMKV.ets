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

    static version() {
        return native.version();
    }

    static rootDir() {
        return MMKV.g_rootDir;
    }

    private nativeHandle: BigInt;
    private constructor(handle: BigInt) {
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
}
