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
import { MMKV, NativeBuffer } from '@tencent/mmkv';
import util from '@ohos.util';
import dataPreferences from '@ohos.data.preferences';
import { GlobalContainer } from './Util'

@Entry
@Component
struct Index {
  @State message: string = 'Hello, world!';

  build() {
    Row() {
      Column({ space: 20 }) {
        Button('Functionality Test')
          .type(ButtonType.Normal)
          .buttonStyle(ButtonStyleMode.TEXTUAL)
          .fontSize(18)
          .onClick(() => {
            this.functionalTest('test', MMKV.MULTI_PROCESS_MODE, 'encrypt_key');
          })
        Button('Encryption Test')
          .type(ButtonType.Normal)
          .buttonStyle(ButtonStyleMode.TEXTUAL)
          .fontSize(18)
          .onClick(() => {
            this.testReKey();
          })
        Button('Import Preferences Test')
          .type(ButtonType.Normal)
          .buttonStyle(ButtonStyleMode.TEXTUAL)
          .fontSize(18)
          .onClick(() => {
            this.testImportPreferences();
          })
        Button('Backup & Restore Test')
          .type(ButtonType.Normal)
          .buttonStyle(ButtonStyleMode.TEXTUAL)
          .fontSize(18)
          .onClick(() => {
            this.testBackup();
            this.testRestore();
          })
        Button('Auto Expiration Test')
          .type(ButtonType.Normal)
          .buttonStyle(ButtonStyleMode.TEXTUAL)
          .fontSize(18)
          .onClick(() => {
            this.testAutoExpire();
          })
        Button('Compare-Before-Set Test')
          .type(ButtonType.Normal)
          .buttonStyle(ButtonStyleMode.TEXTUAL)
          .fontSize(18)
          .onClick(() => {
            this.testCompareBeforeSet()
          })
        Button('Ashmem Test')
          .type(ButtonType.Normal)
          .buttonStyle(ButtonStyleMode.TEXTUAL)
          .fontSize(18)
          .onClick(() => {
            this.testAshmem()
          })
        Text('mmkv version: ' + MMKV.version)
          .fontSize(16)
          .fontWeight(FontWeight.Medium)
        Text('pagesize: ' + MMKV.pageSize.toString())
          .fontSize(16)
          .fontWeight(FontWeight.Medium)
        Text('rootDir: ' + MMKV.rootDir)
          .fontSize(16)
          .fontWeight(FontWeight.Medium)
      }
      .width('100%')
    }
    .height('100%')
  }

  functionalTest(mmapID: string, mode?: number, cryptKey?: string, rootPath?: string, expectedCapacity?: bigint) {
    let kv = MMKV.mmkvWithID(mmapID, mode, cryptKey, rootPath, expectedCapacity);
    hilog.info(0, 'mmkvdemo', 'mmapID = %{public}s, cryptKey = %{public}s', kv.mmapID, kv.cryptKey);

    kv.encodeBool('bool', true);
    hilog.info(0, 'mmkvdemo', 'decode bool = %{public}s', kv.decodeBool('bool'));

    let pow31 = Math.pow(2, 31);
    kv.encodeInt32('int32', pow31 - 1);
    hilog.info(0, 'mmkvdemo', 'decode int32 max = %{public}d', kv.decodeInt32('int32'));

    kv.encodeInt32('int32', -pow31);
    hilog.info(0, 'mmkvdemo', 'decode int32 min = %{public}d', kv.decodeInt32('int32'));

    let pow32 = Math.pow(2, 32);
    kv.encodeUInt32('uint32', pow32 - 1);
    hilog.info(0, 'mmkvdemo', 'decode uint32 max = %{public}d', kv.decodeUInt32('uint32'));

    kv.encodeUInt32('uint32', 0);
    hilog.info(0, 'mmkvdemo', 'decode uint32 min = %{public}d', kv.decodeUInt32('uint32'));

    let pow63 = BigInt(2**63);
    kv.encodeInt64('int64', pow63 - BigInt(1));
    hilog.info(0, 'mmkvdemo', 'decode int64 max = %{public}i', kv.decodeInt64('int64'));

    kv.encodeInt64('int64', -pow63);
    hilog.info(0, 'mmkvdemo', 'decode int64 min = %{public}i', kv.decodeInt64('int64'));

    let pow64 = BigInt(2**64);
    kv.encodeUInt64('uint64', pow64 - BigInt(1));
    hilog.info(0, 'mmkvdemo', 'decode uint64 max = %{public}i', kv.decodeUInt64('uint64'));

    kv.encodeUInt64('uint64', BigInt(0));
    hilog.info(0, 'mmkvdemo', 'decode uint64 min = %{public}i', kv.decodeUInt64('uint64'));

    kv.encodeDouble('double', Number.MAX_VALUE);
    hilog.info(0, 'mmkvdemo', 'decode double max = %{public}d', kv.decodeDouble('double'));

    kv.encodeDouble('double', Number.MIN_VALUE);
    hilog.info(0, 'mmkvdemo', 'decode double min = %{public}d', kv.decodeDouble('double'));

    kv.encodeString('string', 'Hello world to OpenHarmony!');
    hilog.info(0, 'mmkvdemo', 'decode string = %{public}s', kv.decodeString('string'));

    hilog.info(0, 'mmkvdemo', 'decode non-exist string = %{public}s', kv.decodeString('non-exist'));
    hilog.info(0, 'mmkvdemo', 'decode non-exist string = %{public}s', kv.decodeString('non-exist', 'default_value'));

    let arrayBuffer = StringToArrayBuffer('This is a string 转换成 a Uint8Array 💯');
    kv.encodeBytes('bytes', arrayBuffer);
    let bytes = kv.decodeBytes('bytes');
    hilog.info(0, 'mmkvdemo', 'decode bytes = %{public}s', ArrayBufferToString(bytes));

    hilog.info(0, 'mmkvdemo', 'bytes value size = %{public}d, actual value size = %{public}d',
      kv.getValueSize('bytes'), kv.getValueSize('bytes', true));

    let strArr: string[] = ['abc', 'defg', 'hijk'];
    kv.encodeStringSet('string-set', strArr);
    let newStrArr = kv.decodeStringSet('string-set');
    hilog.info(0, 'mmkvdemo', 'string-set = %{public}s', ArrayToString(newStrArr));

    kv.encodeStringSet('empty-string-set', []);
    let emptyStrArr = kv.decodeStringSet('empty-string-set');
    hilog.info(0, 'mmkvdemo', 'empty-string-set = %{public}s', ArrayToString(emptyStrArr));
    let nonStrArr = kv.decodeStringSet('non-exist-string-set');
    hilog.info(0, 'mmkvdemo', 'non-exist-string-set = %{public}s', ArrayToString(nonStrArr));

    hilog.info(0, 'mmkvdemo', 'contains bytes = %{public}s, contains bytes_not_exit = %{public}s',
      kv.containsKey('bytes'), kv.containsKey('bytes_not_exit'));

    kv.removeValueForKey('bytes');
    hilog.info(0, 'mmkvdemo', 'after remove, contains bytes = %{public}s', kv.containsKey('bytes'));

    hilog.info(0, 'mmkvdemo', 'total count = %{public}i, total size = %{public}i, actual size = %{public}i',
      kv.count(), kv.totalSize(), kv.actualSize());

    kv.lock();
    kv.trim();
    kv.sync(true);
    kv.unlock();
    hilog.info(0, 'mmkvdemo', 'tryLock = %{public}s', kv.tryLock());

    kv.clearMemoryCache();
    hilog.info(0, 'mmkvdemo', 'allKeys = %{public}s', ArrayToString(kv.allKeys()));

    kv.removeValuesForKeys(['bool', 'int32']);
    hilog.info(0, 'mmkvdemo', 'remove "bool" & "int32", allKeys = %{public}s', ArrayToString(kv.allKeys()));

    kv.clearAll();
    hilog.info(0, 'mmkvdemo', 'clearAll(), allKeys = %{public}s', ArrayToString(kv.allKeys()));

    kv.close();
    hilog.info(0, 'mmkvdemo', 'isFileValue %{public}s', MMKV.isFileValid(mmapID, rootPath));
    hilog.info(0, 'mmkvdemo', 'remove storage %{public}s', MMKV.removeStorage(mmapID, rootPath));
  }

  testMMKV(mmapID: string, decodeOnly: boolean, cryptKey?: string, rootPath?: string): MMKV {
    let kv = MMKV.mmkvWithID(mmapID, MMKV.SINGLE_PROCESS_MODE, cryptKey, rootPath);

    this.testOneMMKV(kv, decodeOnly);
    hilog.info(0, 'mmkvdemo', 'isFileValue %{public}s', MMKV.isFileValid(kv.mmapID, rootPath));

    return kv;
  }

  testOneMMKV(kv: MMKV, decodeOnly: boolean) {
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
    if (nativeBuffer !== null) {
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

  testReKey() {
    let mmapID: string = 'test/AES_reKey1';
    let kv = this.testMMKV(mmapID, false);

    kv.reKey('Key_seq_1');
    kv.clearMemoryCache();
    this.testMMKV(mmapID, true, 'Key_seq_1');

    kv.reKey('Key_seq_2');
    kv.clearMemoryCache();
    this.testMMKV(mmapID, true, 'Key_seq_2');

    kv.reKey();
    kv.clearMemoryCache();
    this.testMMKV(mmapID, true);
  }

  testImportPreferences() {
    let context = GlobalContainer.getObject('context') as Context
    let preferences = dataPreferences.getPreferencesSync(context, { name: 'default' });
    preferences.putSync('bool', true);
    let pow31 = Math.pow(2, 31);
    preferences.putSync('int32_max', pow31 - 1);
    preferences.putSync('int32_min', -pow31);
    let pow32 = Math.pow(2, 32);
    preferences.putSync('uint32', pow32 - 1);
    preferences.putSync('double_max', Number.MAX_VALUE);
    preferences.putSync('double_min', Number.MIN_VALUE);
    preferences.putSync('string', 'hello, world preferenes');
    preferences.putSync('string_array', ['hello', 'world', 'preferenes']);
    preferences.putSync('empty_string_array', []);
    preferences.putSync('number_array', [pow31 - 1, -pow31, pow32, Number.MAX_VALUE, Number.MIN_VALUE]);
    preferences.putSync('bool_array', [true, false, true, false]);
    let arrayBuffer = StringToArrayBuffer('This is a string 转换成 a Uint8Array 💯');
    preferences.putSync('uint8_array', new Uint8Array(arrayBuffer));
    preferences.flush();

    let kv = MMKV.mmkvWithID('imported');
    let count = kv.importFromPreferences(preferences);
    hilog.info(0, 'mmkvdemo', 'importFromPreferences count %{public}i', count);
    hilog.info(0, 'mmkvdemo', 'decode int32 max = %{public}d', kv.decodeDouble('int32_max'));
    hilog.info(0, 'mmkvdemo', 'decode int32 min = %{public}d', kv.decodeDouble('int32_min'));
    hilog.info(0, 'mmkvdemo', 'decode uint32 max = %{public}d', kv.decodeDouble('uint32'));
    hilog.info(0, 'mmkvdemo', 'decode double max = %{public}d', kv.decodeDouble('double_max'));
    hilog.info(0, 'mmkvdemo', 'decode double min = %{public}d', kv.decodeDouble('double_min'));
    hilog.info(0, 'mmkvdemo', 'decode string = %{public}s', kv.decodeString('string'));
    hilog.info(0, 'mmkvdemo', 'decode string array = %{public}s', ArrayToString(kv.decodeStringSet('string_array')));
    hilog.info(0, 'mmkvdemo', 'decode empty string array = %{public}s', ArrayToString(kv.decodeStringSet('empty_string_array')));
    hilog.info(0, 'mmkvdemo', 'decode number array = %{public}s', ArrayToString(kv.decodeNumberSet('number_array')));
    hilog.info(0, 'mmkvdemo', 'decode bool array = %{public}s', ArrayToString(kv.decodeBoolSet('bool_array')));
    hilog.info(0, 'mmkvdemo', 'decode uint8 array = %{public}s', ArrayBufferToString(kv.decodeBytes('uint8_array')));
  }

  testBackup() {
    let context = GlobalContainer.getObject('context') as Context;
    let backupRootDir = context.filesDir + '/mmkv_backup_3';
    let otherDir = context.filesDir + '/mmkv_3';
    let mmapID = 'test/AES';
    let cryptKey = 'Tencent MMKV';

    {
      let kv = this.testMMKV(mmapID, false, cryptKey, otherDir);
      kv.removeValueForKey('test_restore');
      kv.close();
    }

    let ret = MMKV.backupOneToDirectory(mmapID, backupRootDir, otherDir);
    hilog.info(0, 'mmkvdemo', 'backup one [%{public}s] ret = %{public}s', mmapID, ret);
    if (ret) {
      let mmkv = MMKV.backedUpMMKVWithID(mmapID, backupRootDir, MMKV.SINGLE_PROCESS_MODE, cryptKey);
      hilog.info(0, 'mmkvdemo', 'check on backup file[%{public}s] allKeys: %{public}s',
        mmkv.mmapID, ArrayToString(mmkv.allKeys()));

      mmkv = MMKV.mmkvWithID(mmapID, MMKV.SINGLE_PROCESS_MODE, cryptKey, otherDir);
      hilog.info(0, 'mmkvdemo', 'check on origin file[%{public}s] allKeys: %{public}s',
        mmkv.mmapID, ArrayToString(mmkv.allKeys()));
    }

    /*{
      MMKV mmkv = MMKV.mmkvWithID('imported');
      mmkv.close();
      mmkv = MMKV.mmkvWithID('test/AES_reKey1');
      mmkv.close();
    }*/
    backupRootDir = context.filesDir + '/mmkv_backup';
    let count = MMKV.backupAllToDirectory(backupRootDir);
    hilog.info(0, 'mmkvdemo', 'backup all count %{public}d', count);
    if (count > 0) {
      let mmkv = MMKV.backedUpMMKVWithID('imported', backupRootDir);
      hilog.info(0, 'mmkvdemo', 'check on backup file[%{public}s] allKeys: %{public}s',
        mmkv.mmapID, ArrayToString(mmkv.allKeys()));

      mmkv = MMKV.backedUpMMKVWithID('test/AES_reKey1', backupRootDir);
      hilog.info(0, 'mmkvdemo', 'check on backup file[%{public}s] allKeys: %{public}s',
        mmkv.mmapID, ArrayToString(mmkv.allKeys()));
    }
  }

  testRestore() {
    hilog.info(0, 'mmkvdemo', 'test restore begin');

    let context = GlobalContainer.getObject('context') as Context;
    let backupRootDir = context.filesDir + '/mmkv_backup_3';
    let otherDir = context.filesDir + '/mmkv_3';
    let mmapID = 'test/AES';
    let cryptKey = 'Tencent MMKV';

    let mmkv = MMKV.mmkvWithID(mmapID, MMKV.SINGLE_PROCESS_MODE, cryptKey, otherDir);
    mmkv.encodeBool('test_restore', true);
    hilog.info(0, 'mmkvdemo', 'before restore [%{public}s] allKeys: %{public}s',
      mmkv.mmapID, ArrayToString(mmkv.allKeys()));
    let ret = MMKV.restoreOneFromDirectory(mmapID, backupRootDir, otherDir);
    hilog.info(0, 'mmkvdemo', 'restore one [%{public}s] ret = %{public}s', mmapID, ret);
    if (ret) {
      hilog.info(0, 'mmkvdemo', 'after restore [%{public}s] allKeys: %{public}s',
        mmkv.mmapID, ArrayToString(mmkv.allKeys()));
    }

    /*{
      mmkv = MMKV.mmkvWithID('imported');
      mmkv.close();
      mmkv = MMKV.mmkvWithID('test/AES_reKey1');
      mmkv.close();
    }*/
    backupRootDir = context.filesDir + '/mmkv_backup';
    let count = MMKV.restoreAllFromDirectory(backupRootDir);
    hilog.info(0, 'mmkvdemo', 'restore all count %{public}d', count);
    if (count > 0) {
      mmkv = MMKV.mmkvWithID('imported');
      hilog.info(0, 'mmkvdemo', 'check on restore file[%{public}s] allKeys: %{public}s',
        mmkv.mmapID, ArrayToString(mmkv.allKeys()));

      mmkv = MMKV.mmkvWithID('test/AES_reKey1');
      hilog.info(0, 'mmkvdemo', 'check on restore file[%{public}s] allKeys: %{public}s',
        mmkv.mmapID, ArrayToString(mmkv.allKeys()));
    }
  }

  private testAutoExpireOne(kv: MMKV, decodeOnly: boolean, expiration: number): void {
    if (!decodeOnly) {
      kv.encodeBool('bool', true, expiration);
    }
    hilog.info(0, 'mmkvdemo', 'decode bool = %{public}s', kv.decodeBool('bool'));

    let pow31 = Math.pow(2, 31);
    if (!decodeOnly) {
      kv.encodeInt32('int32', pow31 - 1, expiration);
    }
    hilog.info(0, 'mmkvdemo', 'decode int32 max = %{public}d', kv.decodeInt32('int32'));

    if (!decodeOnly) {
      kv.encodeInt32('int32', -pow31, expiration);
    }
    hilog.info(0, 'mmkvdemo', 'decode int32 min = %{public}d', kv.decodeInt32('int32'));

    let pow32 = Math.pow(2, 32);
    if (!decodeOnly) {
      kv.encodeUInt32('uint32', pow32 - 1, expiration);
    }
    hilog.info(0, 'mmkvdemo', 'decode uint32 max = %{public}d', kv.decodeUInt32('uint32'));

    if (!decodeOnly) {
      kv.encodeUInt32('uint32', 0, expiration);
    }
    hilog.info(0, 'mmkvdemo', 'decode uint32 min = %{public}d', kv.decodeUInt32('uint32'));

    let pow63 = BigInt(2**63);
    if (!decodeOnly) {
      kv.encodeInt64('int64', pow63 - BigInt(1), expiration);
    }
    hilog.info(0, 'mmkvdemo', 'decode int64 max = %{public}i', kv.decodeInt64('int64'));

    if (!decodeOnly) {
      kv.encodeInt64('int64', -pow63, expiration);
    }
    hilog.info(0, 'mmkvdemo', 'decode int64 min = %{public}i', kv.decodeInt64('int64'));

    let pow64 = BigInt(2**64);
    if (!decodeOnly) {
      kv.encodeUInt64('uint64', pow64 - BigInt(1), expiration);
    }
    hilog.info(0, 'mmkvdemo', 'decode uint64 max = %{public}i', kv.decodeUInt64('uint64'));

    if (!decodeOnly) {
      kv.encodeUInt64('uint64', BigInt(0), expiration);
    }
    hilog.info(0, 'mmkvdemo', 'decode uint64 min = %{public}i', kv.decodeUInt64('uint64'));

    if (!decodeOnly) {
      kv.encodeDouble('double', Number.MAX_VALUE, expiration);
    }
    hilog.info(0, 'mmkvdemo', 'decode double max = %{public}d', kv.decodeDouble('double'));

    if (!decodeOnly) {
      kv.encodeDouble('double', Number.MIN_VALUE, expiration);
    }
    hilog.info(0, 'mmkvdemo', 'decode double min = %{public}d', kv.decodeDouble('double'));

    if (!decodeOnly) {
      kv.encodeString('string', 'Hello world to OpenHarmony!', expiration);
    }
    hilog.info(0, 'mmkvdemo', 'decode string = %{public}s', kv.decodeString('string'));

    hilog.info(0, 'mmkvdemo', 'decode non-exist string = %{public}s', kv.decodeString('non-exist'));
    hilog.info(0, 'mmkvdemo', 'decode non-exist string = %{public}s', kv.decodeString('non-exist', 'default_value'));

    if (!decodeOnly) {
      let arrayBuffer = StringToArrayBuffer('This is a string 转换成 a Uint8Array 💯');
      kv.encodeBytes('bytes', arrayBuffer, expiration);
    }
    let bytes = kv.decodeBytes('bytes');
    hilog.info(0, 'mmkvdemo', 'decode bytes = %{public}s', ArrayBufferToString(bytes));

    hilog.info(0, 'mmkvdemo', 'bytes value size = %{public}d, actual value size = %{public}d',
      kv.getValueSize('bytes'), kv.getValueSize('bytes', true));

    if (!decodeOnly) {
      let strArr: string[] = ['abc', 'defg', 'hijk'];
      kv.encodeStringSet('string-set', strArr, expiration);
    }
    let newStrArr = kv.decodeStringSet('string-set');
    hilog.info(0, 'mmkvdemo', 'string-set = %{public}s', ArrayToString(newStrArr));

    if (!decodeOnly) {
      kv.encodeStringSet('empty-string-set', [], expiration);
    }
    let emptyStrArr = kv.decodeStringSet('empty-string-set');
    hilog.info(0, 'mmkvdemo', 'empty-string-set = %{public}s', ArrayToString(emptyStrArr));
    let nonStrArr = kv.decodeStringSet('non-exist-string-set');
    hilog.info(0, 'mmkvdemo', 'non-exist-string-set = %{public}s', ArrayToString(nonStrArr));

    hilog.info(0, 'mmkvdemo', 'contains bool = %{public}s, contains bool_not_exit = %{public}s',
      kv.containsKey('bool'), kv.containsKey('bool_not_exit'));

    kv.removeValueForKey('bool');
    hilog.info(0, 'mmkvdemo', 'after remove, contains bool = %{public}s', kv.containsKey('bool'));

    hilog.info(0, 'mmkvdemo', 'total NonExpire count = %{public}i, total size = %{public}i, actual size = %{public}i',
      kv.count(true), kv.totalSize(), kv.actualSize());

    kv.clearMemoryCache();
    hilog.info(0, 'mmkvdemo', 'all NonExpire Keys = %{public}s', ArrayToString(kv.allKeys(true)));

    kv.removeValuesForKeys(['int32', 'uint32']);
    hilog.info(0, 'mmkvdemo', 'remove "int32" & "uint32", allKeys = %{public}s', ArrayToString(kv.allKeys(true)));
  }

  private async testAutoExpire(): Promise<void> {
    let mmkv = MMKV.mmkvWithID('test_auto_expire');
    mmkv.clearAll();
    mmkv.disableAutoKeyExpire();

    mmkv.enableAutoKeyExpire(1);
    mmkv.encodeBool('auto_expire_key_1', true);
    mmkv.encodeBool('never_expire_key_1', true, MMKV.ExpireNever);

    this.testAutoExpireOne(mmkv, false, 1);
    await new Promise<void>(resolve => setTimeout(resolve, 1000 * 2));
    this.testAutoExpireOne(mmkv, true, 1);

    if (mmkv.containsKey('auto_expire_key_1')) {
      hilog.error(0, 'mmkvdemo', 'auto key expiration auto_expire_key_1');
    } else {
      hilog.info(0, 'mmkvdemo', 'auto key expiration auto_expire_key_1');
    }
    if (mmkv.containsKey('never_expire_key_1')) {
      hilog.info(0, 'mmkvdemo', 'auto key expiration never_expire_key_1');
    } else {
      hilog.error(0, 'mmkvdemo', 'auto key expiration never_expire_key_1');
    }
  }

  private testCompareBeforeSet(): void {
    let mmkv = MMKV.mmkvWithID('testCompareBeforeSet');
    mmkv.enableCompareBeforeSet();

    mmkv.encodeString('key', 'extra');

    {
      let key = 'int';
      let v = 12345;
      mmkv.encodeInt32(key, v);
      let actualSize = mmkv.actualSize();
      hilog.info(0, 'mmkvdemo', 'testCompareBeforeSet actualSize = %{public}i', actualSize);
      hilog.info(0, 'mmkvdemo', 'testCompareBeforeSet v = %{public}d', mmkv.decodeInt32(key, -1));
      mmkv.encodeInt32(key, v);
      let actualSize2 = mmkv.actualSize();
      hilog.info(0, 'mmkvdemo', 'testCompareBeforeSet actualSize = %{public}i', actualSize2);
      if (actualSize2 != actualSize) {
        hilog.error(0, 'mmkvdemo', 'testCompareBeforeSet fail');
      }

      mmkv.encodeInt32(key, v * 23);
      hilog.info(0, 'mmkvdemo', 'testCompareBeforeSet actualSize = %{public}i', mmkv.actualSize());
      hilog.info(0, 'mmkvdemo', 'testCompareBeforeSet v = %{public}d', mmkv.decodeInt32(key, -1));
    }

    {
      let key = 'string';
      let v = 'w012A🏊🏻good';
      mmkv.encodeString(key, v);
      let actualSize = mmkv.actualSize();
      hilog.info(0, 'mmkvdemo', 'testCompareBeforeSet actualSize = %{public}i', actualSize);
      hilog.info(0, 'mmkvdemo', 'testCompareBeforeSet v = %{public}s', mmkv.decodeString(key, ''));
      mmkv.encodeString(key, v);
      let actualSize2 = mmkv.actualSize();
      hilog.info(0, 'mmkvdemo', 'testCompareBeforeSet actualSize = %{public}i', actualSize2);
      if (actualSize2 != actualSize) {
        hilog.error(0, 'mmkvdemo', 'testCompareBeforeSet fail');
      }

      mmkv.encodeString(key, 'temp data 👩🏻‍🏫');
      hilog.info(0, 'mmkvdemo', 'testCompareBeforeSet actualSize = %{public}i', mmkv.actualSize());
      hilog.info(0, 'mmkvdemo', 'testCompareBeforeSet v = %{public}s', mmkv.decodeString(key, ''));
    }
  }

  private testAshmem() {
    let mmapID = 'testAshmem';
    let cryptKey = 'Tencent MMKV';

    let kv = MMKV.mmkvWithAshmemID(mmapID, MMKV.pageSize, MMKV.SINGLE_PROCESS_MODE, cryptKey);
    this.testOneMMKV(kv, false);

    let newKV = MMKV.mmkvWithAshmemFD(mmapID, kv.ashmemFD, kv.ashmemMetaFD, cryptKey);
    this.testOneMMKV(newKV, true);
  }
}

function StringToArrayBuffer(str: string | undefined): ArrayBuffer {
  if (str == undefined) {
    return new ArrayBuffer(0);
  }
  let enc = new util.TextEncoder(); // always utf-8
  let i8Arr = enc.encodeInto(str);
  return i8Arr.buffer;
}

function ArrayBufferToString(arr: ArrayBuffer | undefined): string {
  if (arr == undefined) {
    return 'undefined';
  }
  let out8Arr = new Uint8Array(arr);
  let dnc = util.TextDecoder.create('utf-8');
  return dnc.decodeWithStream(out8Arr);
}

function ArrayToString(arr: string[] | number[] | boolean[] | undefined): string {
  if (arr == undefined) {
    return 'undefined';
  }
  return '[' + arr.join(', ') + ']';
}
