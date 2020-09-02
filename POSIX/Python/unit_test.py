#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import mmkv

KeyNotExist = 'KeyNotExist'


def test_bool(kv):
    ret = kv.set(True, 'bool')
    assert ret

    value = kv.getBool('bool')
    assert value

    value = kv.getBool(KeyNotExist)
    assert not value

    value = kv.getBool(KeyNotExist, True)
    assert value

    print('test bool: passed')


def test_int(kv):
    MinInt = -1 * (2 ** 31)
    ret = kv.set(MinInt, 'Int')
    assert ret

    value = kv.getInt('Int')
    assert value == MinInt

    MaxInt = (2 ** 31) - 1
    ret = kv.set(MaxInt, 'Int')
    assert ret

    value = kv.getInt('Int')
    assert value == MaxInt

    value = kv.getInt(KeyNotExist)
    assert value == 0

    value = kv.getInt(KeyNotExist, -1)
    assert value == -1

    print('test int: passed')


def test_uint(kv):
    MaxUInt = (2 ** 32) - 1
    ret = kv.set(MaxUInt, 'UInt')
    assert ret

    value = kv.getUInt('UInt')
    assert value == MaxUInt

    value = kv.getUInt(KeyNotExist)
    assert value == 0

    value = kv.getUInt(KeyNotExist, 1)
    assert value == 1

    print('test unsigned int: passed')


def test_long_int(kv):
    MinLongInt = -1 * (2 ** 63)
    ret = kv.set(MinLongInt, 'LongInt')
    assert ret

    value = kv.getLongInt('LongInt')
    assert value == MinLongInt

    MaxLongInt = (2 ** 63) - 1
    ret = kv.set(MaxLongInt, 'LongInt')
    assert ret

    value = kv.getLongInt('LongInt')
    assert value == MaxLongInt

    value = kv.getLongInt(KeyNotExist)
    assert value == 0

    value = kv.getLongInt(KeyNotExist, -1)
    assert value == -1

    print('test long int: passed')


def test_long_uint(kv):
    MaxLongUInt = (2 ** 64) - 1
    ret = kv.set(MaxLongUInt, 'LongUInt')
    assert ret

    value = kv.getLongUInt('LongUInt')
    assert value == MaxLongUInt

    value = kv.getLongUInt(KeyNotExist)
    assert value == 0

    value = kv.getLongUInt(KeyNotExist, 1)
    assert value == 1

    print('test long unsigned int: passed')


def is_float_equal(a, b, accuracy=1e-09):
    return abs(a - b) <= max(accuracy * max(abs(a), abs(b)), 0.0)


def test_float(kv):
    MinFloat = sys.float_info.min
    ret = kv.set(MinFloat, 'Float')
    assert ret

    value = kv.getFloat('Float')
    assert is_float_equal(value, MinFloat)

    MaxFloat = sys.float_info.max
    ret = kv.set(MaxFloat, 'Float')
    assert ret

    value = kv.getFloat('Float')
    assert is_float_equal(value, MaxFloat)

    value = kv.getFloat(KeyNotExist)
    assert is_float_equal(value, 0.0)

    value = kv.getFloat(KeyNotExist, -1.0)
    assert is_float_equal(value, -1.0)

    print('test float: passed')


def test_string(kv):
    OlympicString = 'Hello 2021 Olympic Games 奥运会'
    ret = kv.set(OlympicString, 'String')
    assert ret

    value = kv.getString('String')
    if sys.version_info >= (3, 0):
        assert value == OlympicString
    else:
        assert value == OlympicString.decode('utf-8')

    value = kv.getString(KeyNotExist)
    assert (not value) or (len(value) == 0)

    value = kv.getString(KeyNotExist, 'a')
    assert value == 'a'

    print('test string: passed')


def test_bytes(kv):
    OlympicYears = [0, 4, 8, 12, 16, 21]
    OlympicYearsBytes = bytes(OlympicYears)
    ret = kv.set(OlympicYearsBytes, 'Bytes')
    assert ret

    value = kv.getBytes('Bytes')
    assert value == OlympicYearsBytes
    if sys.version_info >= (3, 0):
        value = list(value)
        assert value == OlympicYears

    value = kv.getBytes(KeyNotExist)
    assert (not value) or (len(value) == 0)

    default_value = None
    if sys.version_info >= (3, 0):
        default_value = bytes('哦豁', 'utf8')
    else:
        default_value = bytes('a')
    value = kv.getBytes(KeyNotExist, default_value)
    assert value == default_value

    print('test bytes: passed')


def test_equal(kv, mmap_id):
    assert kv.mmapID() == mmap_id

    another = mmkv.MMKV(mmap_id)
    assert kv == another

    kv_keys = sorted(kv.keys())
    another_keys = sorted(another.keys())
    assert kv_keys == another_keys

    print('test equal: passed')


if __name__ == '__main__':
    mmkv.MMKV.initializeMMKV('/tmp/mmkv')
    kv = mmkv.MMKV('unit_test_python')
    test_bool(kv)
    test_int(kv)
    test_uint(kv)
    test_long_int(kv)
    test_long_uint(kv)
    test_float(kv)
    test_string(kv)
    test_bytes(kv)
    test_equal(kv, 'unit_test_python')
