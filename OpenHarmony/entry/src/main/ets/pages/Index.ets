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

import { hilog } from '@kit.PerformanceAnalysisKit';
import { MMKV, MMKVLogLevel } from 'mmkv';
import util from '@ohos.util';

@Entry
@Component
struct Index {
  @State message: string = 'Functionality Test';

  build() {
    Row() {
      Column({ space: 20 }) {
        Button(this.message)
          .type(ButtonType.Normal)
          .buttonStyle(ButtonStyleMode.TEXTUAL)
          .fontSize(18)
          .onClick(() => {
            this.functionalTest();
          })
        Text('mmkv version: ' + MMKV.version + ' pagesize: ' + MMKV.pageSize.toString())
          .fontSize(16)
          .fontWeight(FontWeight.Medium)
        Text(MMKV.rootDir)
          .fontSize(16)
          .fontWeight(FontWeight.Medium)
      }
      .width('100%')
    }
    .height('100%')
  }

  functionalTest() {
    let kv = MMKV.defaultMMKV(MMKV.MULTI_PROCESS_MODE);
    kv.encodeBool('bool', true);
    hilog.info(0, "mmkvdemo", 'decode bool = %{public}s', kv.decodeBool('bool'));

    let pow31 = Math.pow(2, 31);
    kv.encodeInt32('int32', pow31 - 1);
    hilog.info(0, "mmkvdemo", 'decode int32 max = %{public}d', kv.decodeInt32('int32'));

    kv.encodeInt32('int32', -pow31);
    hilog.info(0, "mmkvdemo", 'decode int32 min = %{public}d', kv.decodeInt32('int32'));

    let pow32 = Math.pow(2, 32);
    kv.encodeUInt32('uint32', pow32 - 1);
    hilog.info(0, "mmkvdemo", 'decode uint32 max = %{public}d', kv.decodeUInt32('uint32'));

    kv.encodeUInt32('uint32', 0);
    hilog.info(0, "mmkvdemo", 'decode uint32 min = %{public}d', kv.decodeUInt32('uint32'));

    let pow63 = BigInt(2**63);
    kv.encodeInt64('int64', pow63 - BigInt(1));
    hilog.info(0, "mmkvdemo", 'decode int64 max = %{public}i', kv.decodeInt64('int64'));

    kv.encodeInt64('int64', -pow63);
    hilog.info(0, "mmkvdemo", 'decode int64 min = %{public}i', kv.decodeInt64('int64'));

    let pow64 = BigInt(2**64);
    kv.encodeUInt64('uint64', pow64 - BigInt(1));
    hilog.info(0, "mmkvdemo", 'decode uint64 max = %{public}i', kv.decodeUInt64('uint64'));

    kv.encodeUInt64('uint64', BigInt(0));
    hilog.info(0, "mmkvdemo", 'decode uint64 min = %{public}i', kv.decodeUInt64('uint64'));

    kv.encodeDouble('double', Number.MAX_VALUE);
    hilog.info(0, "mmkvdemo", 'decode double max = %{public}d', kv.decodeDouble('double'));

    kv.encodeDouble('double', Number.MIN_VALUE);
    hilog.info(0, "mmkvdemo", 'decode double min = %{public}d', kv.decodeDouble('double'));

    kv.encodeString('string', 'Hello world to OpenHarmony!');
    hilog.info(0, "mmkvdemo", 'decode string = %{public}s', kv.decodeString('string'));

    hilog.info(0, "mmkvdemo", 'decode non-exist string = %{public}s', kv.decodeString('non-exist'));
    hilog.info(0, "mmkvdemo", 'decode non-exist string = %{public}s', kv.decodeString('non-exist', 'default_value'));

    let arrayBuffer = StringToArrayBuffer("This is a string 转换成 a Uint8Array 💯");
    kv.encodeBytes('bytes', arrayBuffer);
    let bytes = kv.decodeBytes('bytes');
    hilog.info(0, "mmkvdemo", 'decode bytes = %{public}s', ArrayBufferToString(bytes));

    hilog.info(0, "mmkvdemo", 'bytes value size = %{public}d, actual value size =  %{public}d',
      kv.getValueSize('bytes'), kv.getValueSize('bytes', true));

    let strArr: string[] = ['abc', 'defg', 'hijk'];
    kv.encodeStringSet('string-set', strArr);
    let newStrArr = kv.decodeStringSet('string-set');
    hilog.info(0, "mmkvdemo", 'string-set = %{public}s', StringArrayToString(newStrArr));

    kv.encodeStringSet('empty-string-set', []);
    let emptyStrArr = kv.decodeStringSet('empty-string-set');
    hilog.info(0, "mmkvdemo", 'empty-string-set = %{public}s', StringArrayToString(emptyStrArr));
    let nonStrArr = kv.decodeStringSet('non-exist-string-set');
    hilog.info(0, "mmkvdemo", 'non-exist-string-set = %{public}s', StringArrayToString(nonStrArr));

    hilog.info(0, "mmkvdemo", 'contains bytes = %{public}s, contains bytes_not_exit = %{public}s',
      kv.containsKey('bytes'), kv.containsKey('bytes_not_exit'));

    kv.removeValueForKey('bytes');
    hilog.info(0, "mmkvdemo", 'after remove, contains bytes = %{public}s', kv.containsKey('bytes'));

    hilog.info(0, "mmkvdemo", 'mmapID = %{public}s, total count = %{public}i, total size = %{public}i, actual size = %{public}i',
      kv.mmapID, kv.count(), kv.totalSize(), kv.actualSize());

    kv.lock();
    kv.trim();
    kv.sync(true);
    kv.unlock();
    hilog.info(0, "mmkvdemo", 'tryLock = %{public}s', kv.tryLock());

    kv.clearMemoryCache();
    hilog.info(0, "mmkvdemo", 'allKeys = %{public}s', StringArrayToString(kv.allKeys()));

    kv.removeValuesForKeys(['bool', 'int32']);
    hilog.info(0, "mmkvdemo", 'remove "bool" & "int32", allKeys = %{public}s', StringArrayToString(kv.allKeys()));

    kv.clearAll();
    hilog.info(0, "mmkvdemo", 'clearAll(), allKeys = %{public}s', StringArrayToString(kv.allKeys()));

    kv.close();
    hilog.info(0, "mmkvdemo", 'isFileValue %{public}s', MMKV.isFileValid("mmkv.default"));
    hilog.info(0, "mmkvdemo", 'remove storage %{public}s', MMKV.removeStorage("mmkv.default"));
  }
}

function StringToArrayBuffer(str: string): ArrayBuffer {
  let enc = new util.TextEncoder(); // always utf-8
  let i8Arr = enc.encodeInto(str);
  return i8Arr.buffer;
}

function ArrayBufferToString(arr: ArrayBuffer): string {
  let out8Arr = new Uint8Array(arr);
  let dnc = util.TextDecoder.create("utf-8");
  return dnc.decodeWithStream(out8Arr);
}

function StringArrayToString(strArr: string[]): string {
  if (strArr == undefined) {
    return 'undefined';
  }
  return '[' + strArr.join(', ') + ']';
}