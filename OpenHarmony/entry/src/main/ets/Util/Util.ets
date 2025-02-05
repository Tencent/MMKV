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
import { hilog } from "@kit.PerformanceAnalysisKit";
import { MMKV } from '@tencent/mmkv';
import { util } from "@kit.ArkTS";

export class GlobalContainer {
  private static container = new Map<string, Object>();

  static getObject(value: string) {
    return GlobalContainer.container.get(value);
  }

  static setObject(key: string, objectClass: Object): void {
    GlobalContainer.container.set(key, objectClass);
  }
}

export function testOneMMKV(kv: MMKV, decodeOnly: boolean) {
  hilog.info(0, 'mmkvdemo', 'mmapID = %{public}s, cryptKey = %{public}s', kv.mmapID, kv.cryptKey);

  if (!decodeOnly) {
    kv.encodeBool('bool', true);
  }
  hilog.info(0, 'mmkvdemo', 'decode bool = %{public}s', kv.decodeBool('bool'));

  let pow31 = Math.pow(2, 31);
  if (!decodeOnly) {
    kv.encodeInt32('int32', pow31 - 1);
  }
  hilog.info(0, 'mmkvdemo', 'decode int32 max = %{public}d', kv.decodeInt32('int32'));

  if (!decodeOnly) {
    kv.encodeInt32('int32', -pow31);
  }
  hilog.info(0, 'mmkvdemo', 'decode int32 min = %{public}d', kv.decodeInt32('int32'));

  let pow32 = Math.pow(2, 32);
  if (!decodeOnly) {
    kv.encodeUInt32('uint32', pow32 - 1);
  }
  hilog.info(0, 'mmkvdemo', 'decode uint32 max = %{public}d', kv.decodeUInt32('uint32'));

  if (!decodeOnly) {
    kv.encodeUInt32('uint32', 0);
  }
  hilog.info(0, 'mmkvdemo', 'decode uint32 min = %{public}d', kv.decodeUInt32('uint32'));

  let pow63 = BigInt(2**63);
  if (!decodeOnly) {
    kv.encodeInt64('int64', pow63 - BigInt(1));
  }
  hilog.info(0, 'mmkvdemo', 'decode int64 max = %{public}i', kv.decodeInt64('int64'));

  if (!decodeOnly) {
    kv.encodeInt64('int64', -pow63);
  }
  hilog.info(0, 'mmkvdemo', 'decode int64 min = %{public}i', kv.decodeInt64('int64'));

  let pow64 = BigInt(2**64);
  if (!decodeOnly) {
    kv.encodeUInt64('uint64', pow64 - BigInt(1));
  }
  hilog.info(0, 'mmkvdemo', 'decode uint64 max = %{public}i', kv.decodeUInt64('uint64'));

  if (!decodeOnly) {
    kv.encodeUInt64('uint64', BigInt(0));
  }
  hilog.info(0, 'mmkvdemo', 'decode uint64 min = %{public}i', kv.decodeUInt64('uint64'));

  if (!decodeOnly) {
    kv.encodeFloat('float', Number.MAX_VALUE);
  }
  hilog.info(0, 'mmkvdemo', 'decode float max = %{public}d', kv.decodeFloat('float'));

  if (!decodeOnly) {
    kv.encodeFloat('float', Number.MIN_VALUE);
  }
  hilog.info(0, 'mmkvdemo', 'decode float min = %{public}d', kv.decodeFloat('float'));

  if (!decodeOnly) {
    kv.encodeDouble('double', Number.MAX_VALUE);
  }
  hilog.info(0, 'mmkvdemo', 'decode double max = %{public}d', kv.decodeDouble('double'));

  if (!decodeOnly) {
    kv.encodeDouble('double', Number.MIN_VALUE);
  }
  hilog.info(0, 'mmkvdemo', 'decode double min = %{public}d', kv.decodeDouble('double'));

  if (!decodeOnly) {
    kv.encodeString('string', 'Hello world to OpenHarmony!');
  }
  hilog.info(0, 'mmkvdemo', 'decode string = %{public}s', kv.decodeString('string'));

  hilog.info(0, 'mmkvdemo', 'decode non-exist string = %{public}s', kv.decodeString('non-exist'));
  hilog.info(0, 'mmkvdemo', 'decode non-exist string = %{public}s', kv.decodeString('non-exist', 'default_value'));

  if (!decodeOnly) {
    let arrayBuffer = StringToArrayBuffer('This is a string 转换成 a Uint8Array 💯');
    kv.encodeBytes('bytes', arrayBuffer);
  }
  let bytes = kv.decodeBytes('bytes');
  hilog.info(0, 'mmkvdemo', 'decode bytes = %{public}s', ArrayBufferToString(bytes));

  hilog.info(0, 'mmkvdemo', 'bytes value size = %{public}d, actual value size = %{public}d',
    kv.getValueSize('bytes'), kv.getValueSize('bytes', true));

  let sizeNeeded = kv.getValueSize('bytes', true);
  let nativeBuffer = MMKV.createNativeBuffer(sizeNeeded);
  if (nativeBuffer) {
    let size = kv.writeValueToNativeBuffer('bytes', nativeBuffer);
    hilog.info(0, 'mmkvdemo', 'NativeBuffer: size Needed = %{public}d,  written size = %{public}d', sizeNeeded, size);
    MMKV.destroyNativeBuffer(nativeBuffer);
  }

  if (!decodeOnly) {
    let strArr: string[] = ['abc', 'defg', 'hijk'];
    kv.encodeStringSet('string-set', strArr);
  }
  let newStrArr = kv.decodeStringSet('string-set');
  hilog.info(0, 'mmkvdemo', 'string-set = %{public}s', ArrayToString(newStrArr));

  if (!decodeOnly) {
    kv.encodeStringSet('empty-string-set', []);
  }
  let emptyStrArr = kv.decodeStringSet('empty-string-set');
  hilog.info(0, 'mmkvdemo', 'empty-string-set = %{public}s', ArrayToString(emptyStrArr));
  let nonStrArr = kv.decodeStringSet('non-exist-string-set');
  hilog.info(0, 'mmkvdemo', 'non-exist-string-set = %{public}s', ArrayToString(nonStrArr));

  hilog.info(0, 'mmkvdemo', 'contains bool = %{public}s, contains bool_not_exit = %{public}s',
    kv.containsKey('bool'), kv.containsKey('bool_not_exit'));

  kv.removeValueForKey('bool');
  hilog.info(0, 'mmkvdemo', 'after remove, contains bool = %{public}s', kv.containsKey('bool'));

  hilog.info(0, 'mmkvdemo', 'total count = %{public}i, total size = %{public}i, actual size = %{public}i',
    kv.count(), kv.totalSize(), kv.actualSize());

  // kv.lock();
  // kv.trim();
  // kv.sync(true);
  // kv.unlock();
  // hilog.info(0, 'mmkvdemo', 'tryLock = %{public}s', kv.tryLock());

  kv.clearMemoryCache();
  hilog.info(0, 'mmkvdemo', 'allKeys = %{public}s', ArrayToString(kv.allKeys()));

  kv.removeValuesForKeys(['int32', 'uint32']);
  hilog.info(0, 'mmkvdemo', 'remove "int32" & "uint32", allKeys = %{public}s', ArrayToString(kv.allKeys()));

  // kv.clearAll();
  // hilog.info(0, 'mmkvdemo', 'clearAll(), allKeys = %{public}s', StringArrayToString(kv.allKeys()));

  // kv.close();
}

export function StringToArrayBuffer(str: string | undefined): ArrayBuffer {
  if (str == undefined) {
    return new ArrayBuffer(0);
  }
  let enc = new util.TextEncoder(); // always utf-8
  let i8Arr = enc.encodeInto(str);
  return i8Arr.buffer;
}

export function ArrayBufferToString(arr: ArrayBuffer | undefined): string {
  if (arr == undefined) {
    return 'undefined';
  }
  let out8Arr = new Uint8Array(arr);
  let dnc = util.TextDecoder.create('utf-8');
  return dnc.decodeWithStream(out8Arr);
}

export interface Joinable {
  join(separator?: string): string;
}

export function ArrayToString<T extends Joinable>(arr: T | undefined): string {
  if (arr == undefined) {
    return 'undefined';
  }
  return '[' + arr.join(', ') + ']';
}

export function ArraysEqual(arr1: Uint8Array, arr2: Uint8Array) {
  // Check if the lengths are different
  if (arr1.length !== arr2.length) {
    return false;
  }

  // Compare each element
  for (let i = 0; i < arr1.length; i++) {
    if (arr1[i] !== arr2[i]) {
      return false;
    }
  }

  // If all elements are equal, return true
  return true;
}