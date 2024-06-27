#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import mmkv
import time


def functional_test(mmap_id, decode_only):
    # pass MMKVMode.MultiProcess to get a multi-process instance
    # kv = mmkv.MMKV('test_python', mmkv.MMKVMode.MultiProcess)
    kv = mmkv.MMKV(mmap_id)

    if not decode_only:
        kv.set(True, 'bool')
    print('bool = ', kv.getBool('bool'))

    if not decode_only:
        kv.set(-1 * (2 ** 31), 'int32')
    print('int32 = ', kv.getInt('int32'))

    if not decode_only:
        kv.set((2 ** 32) - 1, 'uint32')
    print('uint32 = ', kv.getUInt('uint32'))

    if not decode_only:
        kv.set(2 ** 63, 'int64')
    print('int64 = ', kv.getLongInt('int64'))

    if not decode_only:
        kv.set((2 ** 64) - 1, 'uint64')
    print('uint64 = ', kv.getLongUInt('uint64'))

    if not decode_only:
        kv.set(3.1415926, 'float')
    print('float = ', kv.getFloat('float'))

    if not decode_only:
        kv.set('Hello world, MMKV for Python!', 'string')
    print('string = ', kv.getString('string'))

    if not decode_only:
        ls = range(0, 10)
        kv.set(bytes(ls), 'bytes')
    b = kv.getBytes('bytes')
    print('raw bytes = ', b)
    if sys.version_info >= (3, 0):
        print('decoded bytes = ', list(b))

    if not decode_only:
        print('keys before remove:', sorted(kv.keys()))
        kv.remove('bool')
        print('"bool" exist after remove: ', ('bool' in kv))
        kv.remove(['int32', 'float'])
        print('keys after remove:', sorted(kv.keys()))
    else:
        print('keys:', sorted(kv.keys()))


def test_backup():
    root_dir = "/tmp/mmkv_backup"
    mmap_id = "test_python"
    ret = mmkv.MMKV.backupOneToDirectory(mmap_id, root_dir)
    print("backup one return: ", ret)

    kv = mmkv.MMKV("test/Encrypt", mmkv.MMKVMode.SingleProcess, "cryptKey")
    kv.remove("test_restore_key")

    count = mmkv.MMKV.backupAllToDirectory(root_dir)
    print("backup all count: ", count)


# just for testing
def utf8len(s):
    return len(s.encode('utf-8'))


def test_expected_capacity():
    key = "key0"
    value = "🏊🏻®4️⃣🐅_"
    dataLen = 10000
    for i in range(dataLen):
        value += "0"

    print("value size =", utf8len(value))
    expectedSize = utf8len(key) + utf8len(value)
    # if we know exactly the sizes of key and value, set expectedCapacity for performance improvement
    kv = mmkv.MMKV("mmkv_capacity0", mmkv.MMKVMode.SingleProcess, "", "", expectedSize)
    # 0 times expand
    kv.set(value, key)
    print("data size from MMKV =", len(kv.getString(key)))

    countTick = 10
    expectedSize *= countTick
    kv = mmkv.MMKV("mmkv_capacity1", mmkv.MMKVMode.SingleProcess, "", "", expectedSize)
    for i in range(countTick):
        key1 = "key" + str(i)
        # 0 times expand
        kv.set(value, key1)


def test_restore():
    root_dir = "/tmp/mmkv_backup"
    mmap_id = "test/Encrypt"
    aes_key = "cryptKey"
    a_kv = mmkv.MMKV(mmap_id, mmkv.MMKVMode.SingleProcess, aes_key)
    a_kv.set("string value before restore", "test_restore_key")
    print("before restore [", a_kv.mmapID(), "] allKeys: ", a_kv.keys())

    ret = mmkv.MMKV.restoreOneFromDirectory(mmap_id, root_dir)
    print("restore one return: ", ret)
    if ret:
        print("after restore [", a_kv.mmapID(), "] allKeys: ", a_kv.keys())

    count = mmkv.MMKV.restoreAllFromDirectory(root_dir)
    print("restore all count: ", count)
    if count > 0:
        backup_mmkv = mmkv.MMKV(mmap_id, mmkv.MMKVMode.SingleProcess, aes_key)
        print("check on restore [", backup_mmkv.mmapID(), "] allKeys: ", backup_mmkv.keys())

        backup_mmkv = mmkv.MMKV("test_python")
        print("check on restore [", backup_mmkv.mmapID(), "] allKeys: ", backup_mmkv.keys())


def test_auto_expire():
    kv = mmkv.MMKV("test_auto_expire")
    kv.clearAll(True)
    kv.disableAutoKeyExpire()

    kv.set(True, "auto_expire_key_1")
    kv.enableAutoKeyExpire(1)
    kv.set("never_expire_value_1", "never_expire_key_1", 0)

    time.sleep(2)
    print("contains auto_expire_key_1:", "auto_expire_key_1" in kv)
    print("contains never_expire_key_1:", "never_expire_key_1" in kv)

    kv.remove("never_expire_key_1")
    kv.enableAutoKeyExpire(0)
    kv.set("never_expire_value_1", "never_expire_key_1")
    kv.set(True, "auto_expire_key_1", 1)
    time.sleep(2)
    print("contains never_expire_key_1:", "never_expire_key_1" in kv)
    print("contains auto_expire_key_1:", "auto_expire_key_1" in kv)
    print("count filter expire key:", kv.count(True))
    print("all non expire keys:", kv.keys(True))


def test_compare_before_set():
    kv = mmkv.MMKV("testCompareBeforeSet")
    kv.enableCompareBeforeSet()
    kv.set("extraValue", "extraKey")

    key = "bool"
    kv.set(True, key)
    print("testCompareBeforeSet: bool value = ", kv.getBool(key))
    actualSize1 = kv.actualSize()
    print("testCompareBeforeSet: actualSize = ", actualSize1)
    print("testCompareBeforeSet: bool value = ", kv.getBool(key))
    kv.set(True, key)
    actualSize2 = kv.actualSize()
    print("testCompareBeforeSet: actualSize2 = ", actualSize2)
    if actualSize1 != actualSize2:
        panic("size not match")

    kv.set(False, key)
    print("testCompareBeforeSet: bool value = ", kv.getBool(key))
    if kv.getBool(key):
        print("value not update")

    s1 = "🏊🏻®hhh4️⃣🐅_yyy"
    s2 = "0aA🏊🏻®hhh4️⃣🐅_zzz"
    key = "string"
    kv.set(s1, key)
    resultString = kv.getString(key)
    print("testCompareBeforeSet: string = ", resultString)
    actualSize1 = kv.actualSize()
    print("testCompareBeforeSet: actualSize = ", actualSize1)
    resultString = kv.getString(key)
    print("testCompareBeforeSet: string = ", resultString)
    kv.set(s1, key)
    actualSize2 = kv.actualSize()
    if actualSize1 != actualSize2:
        print("size not match")

    kv.set(s2, key)
    resultString = kv.getString(key)
    print("testCompareBeforeSet: string = ", resultString)
    if resultString != s2:
        print("value not update")

    kv.disableCompareBeforeSet()


def test_remove_storage():
    kv = mmkv.MMKV("test_remove", mmkv.MMKVMode.MultiProcess)
    kv.set(True, "bool")

    mmkv.MMKV.removeStorage("test_remove")
    kv = mmkv.MMKV("test_remove", mmkv.MMKVMode.MultiProcess)
    if kv.count() != 0:
        print("storage not successfully remove")

    rootDir = "/dev/mmkv_sg"
    kv = mmkv.MMKV("test_remove/sg", rootDir = rootDir)
    kv.set(True, "bool")

    mmkv.MMKV.removeStorage("test_remove/sg")
    kv = mmkv.MMKV("test_remove/sg", rootDir = rootDir)
    if kv.count() != 0:
        print("storage not successfully remove")


def logger(log_level, file, line, function, message):
    level = {mmkv.MMKVLogLevel.NoLog: 'N',
             mmkv.MMKVLogLevel.Debug: 'D',
             mmkv.MMKVLogLevel.Info: 'I',
             mmkv.MMKVLogLevel.Warning: 'W',
             mmkv.MMKVLogLevel.Error: 'E'}
    print('r-[{0}] <{1}:{2}:{3}> {4}'.format(level[log_level], file, line, function, message))


def error_handler(mmap_id, error_type):
    print('[{}] has error {}'.format(mmap_id, error_type))
    return mmkv.MMKVErrorType.OnErrorRecover


def content_change_handler(mmap_id):
    print("[%s]'s content has been changed by other process" % mmap_id)


if __name__ == '__main__':
    # you can enable logging & log handler
    # mmkv.MMKV.initializeMMKV('/tmp/mmkv', mmkv.MMKVLogLevel.Info, logger)
    mmkv.MMKV.initializeMMKV('/tmp/mmkv')

    # redirect logging
    # mmkv.MMKV.registerLogHandler(logger)

    # try recover on error
    # mmkv.MMKV.registerErrorHandler(error_handler)

    # get notified after content changed by other process
    # mmkv.MMKV.registerContentChangeHandler(content_change_handler)

    test_expected_capacity()

    functional_test('test_python', False)

    test_backup()

    test_restore()

    test_auto_expire()
    test_compare_before_set()
    test_remove_storage()

    # mmkv.MMKV.unRegisterLogHandler()
    # mmkv.MMKV.unRegisterErrorHandler()
    # mmkv.MMKV.unRegisterContentChangeHandler()
    mmkv.MMKV.onExit()
