#!/ usr / bin / env python
# - * - coding : utf - 8 - * -
import sys
import os
import mmkv

def functionalTest(decodeOnly):
    kv = mmkv.MMKV('test_python')

    if not decodeOnly:
        kv.set(True, 'bool')
    print('bool = ', kv.getBool('bool'))

    if not decodeOnly:
        kv.set(-1 * (2**31), 'int32')
    print('int32 = ', kv.getInt('int32'))

    if not decodeOnly:
        kv.set((2**32) - 1, 'uint32')
    print('uint32 = ', kv.getUInt('uint32'))

    if not decodeOnly:
        kv.set(2**63, 'int64')
    print('int64 = ', kv.getLongInt('int64'))

    if not decodeOnly:
        kv.set((2**64) - 1, 'uint64')
    print('uint64 = ', kv.getLongUInt('uint64'))

    if not decodeOnly:
        kv.set(3.1415926, 'float')
    print('float = ', kv.getFloat('float'))

    if not decodeOnly:
        kv.set('Hello world, MMKV for Python!', 'string')
    print('string = ', kv.getString('string'))

    if not decodeOnly:
        ls = range(0, 10)
        kv.set(bytes(ls), 'bytes')
    b = kv.getBytes('bytes')
    print('raw bytes = ', b, ', decoded bytes = ', list(b))

    if not decodeOnly:
        print('keys before remove:', sorted(kv.keys()))
        kv.remove('bool')
        print('"bool" exist after remove: ', ('bool' in kv))
        kv.remove(['int32', 'float'])
        print('keys after remove:', sorted(kv.keys()))


if __name__ == '__main__':
    mmkv.MMKV.initializeMMKV('/tmp/mmkv')
    functionalTest(False)
