/*
 * Copyright (C) 2026 THL A29 Limited, a Tencent company.
 * Licensed under the BSD 3-Clause License.
 */

import { abilityDelegatorRegistry } from '@kit.TestKit';
import { beforeAll, beforeEach, afterEach, describe, expect, it } from '@ohos/hypium';
import { MMKV, MMKVLogLevel } from '@tencent/mmkv';

function nowSeconds(): number {
  return Math.floor(Date.now() / 1000);
}

export default function expireTimeTest(): void {
  describe('ExpireTime', () => {
    let rootDir: string = '';
    let mmapID: string = '';
    let kv: MMKV | undefined = undefined;

    beforeAll(() => {
      const context = abilityDelegatorRegistry.getAbilityDelegator().getAppContext();
      rootDir = context.filesDir + '/mmkv_expire_time';
      MMKV.initializeWithPath(rootDir, context.cacheDir, MMKVLogLevel.LevelWarning);
    });

    beforeEach(() => {
      mmapID = 'expire_' + Date.now().toString() + '_' + Math.random().toString().slice(2);
      kv = MMKV.mmkvWithConfig(mmapID, { rootPath: rootDir, enableKeyExpire: false });
    });

    afterEach(() => {
      kv!.close();
      kv = undefined;
      expect(MMKV.removeStorage(mmapID, rootDir)).assertTrue();
    });

    it('disabled_missing_never_and_closed_return_zero', 0, () => {
      expect(kv!.encodeBool('disabled', true)).assertTrue();
      expect(kv!.getExpireTimeForKey('disabled')).assertEqual(0);
      expect(kv!.enableAutoKeyExpire(60)).assertTrue();
      expect(kv!.getExpireTimeForKey('missing')).assertEqual(0);
      expect(kv!.getExpireTimeForKey('')).assertEqual(0);
      expect(kv!.encodeBool('never', true, MMKV.ExpireNever)).assertTrue();
      expect(kv!.getExpireTimeForKey('never')).assertEqual(0);
      kv!.close();
      expect(kv!.getExpireTimeForKey('never')).assertEqual(0);
    });

    it('default_and_explicit_absolute_timestamps_survive_reopen', 0, () => {
      expect(kv!.enableAutoKeyExpire(60)).assertTrue();
      const before = nowSeconds();
      expect(kv!.encodeBool('default', true)).assertTrue();
      expect(kv!.encodeBool('explicit', true, 120)).assertTrue();
      const after = nowSeconds();
      const defaultTime = kv!.getExpireTimeForKey('default');
      const explicitTime = kv!.getExpireTimeForKey('explicit');
      expect(defaultTime >= before + 60 && defaultTime <= after + 60).assertTrue();
      expect(explicitTime >= before + 120 && explicitTime <= after + 120).assertTrue();
      kv!.clearMemoryCache();
      expect(kv!.getExpireTimeForKey('default')).assertEqual(defaultTime);
      kv!.close();
      kv = MMKV.mmkvWithID(mmapID, undefined, undefined, rootDir);
      expect(kv!.getExpireTimeForKey('explicit')).assertEqual(explicitTime);
      kv!.removeValueForKey('explicit');
      expect(kv!.getExpireTimeForKey('explicit')).assertEqual(0);
      expect(kv!.disableAutoKeyExpire()).assertTrue();
      expect(kv!.getExpireTimeForKey('default')).assertEqual(0);
    });

    it('maximum_uint32_timestamp_remains_positive', 0, () => {
      expect(kv!.enableAutoKeyExpire(0)).assertTrue();
      expect(kv!.encodeBool('unsigned', true, 0xffffffff)).assertTrue();
      expect(kv!.getExpireTimeForKey('unsigned')).assertEqual(0xffffffff);
    });

    it('expired_timestamp_remains_until_read_removes_key', 0, async () => {
      expect(kv!.enableAutoKeyExpire(0)).assertTrue();
      expect(kv!.encodeBool('expired', true, 1)).assertTrue();
      const timestamp = kv!.getExpireTimeForKey('expired');
      expect(timestamp > 0).assertTrue();
      await new Promise<void>(resolve => setTimeout(resolve, 2000));
      expect(timestamp <= nowSeconds()).assertTrue();
      expect(kv!.getExpireTimeForKey('expired')).assertEqual(timestamp);
      expect(kv!.decodeBool('expired', false)).assertFalse();
      expect(kv!.getExpireTimeForKey('expired')).assertEqual(0);
    });
  });
}
