#!/usr/bin/env python3

import tempfile
import time

import mmkv


def test_get_expire_time_for_key():
    with tempfile.TemporaryDirectory(prefix='mmkv-expire-time-') as root_dir:
        mmkv.MMKV.initializeMMKV(root_dir)
        kv = mmkv.MMKV('expire_time')
        try:
            assert kv.set('value', 'disabled')
            assert kv.getExpireTimeForKey('disabled') == 0
            assert kv.getExpireTimeForKey('missing') == 0

            assert kv.enableAutoKeyExpire(0)
            assert kv.set('value', 'never', 0)
            assert kv.getExpireTimeForKey('never') == 0
            assert kv.getExpireTimeForKey('missing') == 0
            assert kv.getExpireTimeForKey('') == 0

            before = int(time.time())
            assert kv.set('value', 'finite', 60)
            after = int(time.time())
            expires = kv.getExpireTimeForKey('finite')
            assert isinstance(expires, int)
            assert before + 60 <= expires <= after + 60

            max_uint32 = (1 << 32) - 1
            assert kv.set('value', 'saturated', max_uint32)
            assert kv.getExpireTimeForKey('saturated') == max_uint32

            assert kv.set('value', 'expired', 1)
            expires = kv.getExpireTimeForKey('expired')
            time.sleep(max(0, expires - time.time()) + 0.1)
            # Inspect before any contains/read operation can purge the entry.
            assert kv.getExpireTimeForKey('expired') == expires
            assert kv.getExpireTimeForKey('expired') == expires
            assert kv.getString('expired') == ''
            assert kv.getExpireTimeForKey('expired') == 0
        finally:
            kv.close()


if __name__ == '__main__':
    test_get_expire_time_for_key()
    print('test getExpireTimeForKey: passed')
