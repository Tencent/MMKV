#!/ usr / bin / env python
# - * - coding : utf - 8 - * -
import mmkv


def functional_test(decode_only):
    kv = mmkv.MMKV('test_python')

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
    print('raw bytes = ', b, ', decoded bytes = ', list(b))

    if not decode_only:
        print('keys before remove:', sorted(kv.keys()))
        kv.remove('bool')
        print('"bool" exist after remove: ', ('bool' in kv))
        kv.remove(['int32', 'float'])
        print('keys after remove:', sorted(kv.keys()))
    else:
        print('keys:', sorted(kv.keys()))


if __name__ == '__main__':
    # you can pass LogInfo to enable logging
    # mmkv.MMKV.initializeMMKV('/tmp/mmkv', mmkv.MMKVLogLevel.LogInfo)
    mmkv.MMKV.initializeMMKV('/tmp/mmkv')
    functional_test(False)
