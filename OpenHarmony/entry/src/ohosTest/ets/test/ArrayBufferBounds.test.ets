/*
 * Copyright (C) 2026 THL A29 Limited, a Tencent company.
 * Licensed under the BSD 3-Clause License.
 */

import { abilityDelegatorRegistry } from '@kit.TestKit';
import { beforeAll, beforeEach, afterAll, describe, expect, it } from '@ohos/hypium';
import { MMKV, MMKVLogLevel } from '@tencent/mmkv';
import native from 'libmmkv.so';

interface SliceCodec {
  name: string;
  elementSize: number;
  encode: (handle: bigint, key: string, value: ArrayBuffer, offset: number, length: number,
    expiration?: number) => boolean;
  decode: (handle: bigint, key: string, defaultValue?: ArrayBuffer) => ArrayBuffer;
}

function assertBytes(actual: ArrayBuffer, expected: Uint8Array): void {
  expect(actual instanceof ArrayBuffer).assertTrue();
  const bytes = new Uint8Array(actual);
  expect(bytes.length).assertEqual(expected.length);
  for (let index = 0; index < expected.length; index++) {
    expect(bytes[index]).assertEqual(expected[index]);
  }
}

export default function arrayBufferBoundsTest(): void {
  describe('ArrayBufferBounds', () => {
    const codecs: SliceCodec[] = [
      { name: 'bytes', elementSize: 1, encode: native.encodeBytes, decode: native.decodeBytes },
      { name: 'int32', elementSize: 4, encode: native.encodeInt32Array, decode: native.decodeInt32Array },
      { name: 'uint32', elementSize: 4, encode: native.encodeUInt32Array, decode: native.decodeUInt32Array },
      { name: 'int64', elementSize: 8, encode: native.encodeInt64Array, decode: native.decodeInt64Array },
      { name: 'uint64', elementSize: 8, encode: native.encodeUInt64Array, decode: native.decodeUInt64Array }
    ];
    const source = new Uint8Array(24);
    for (let index = 0; index < source.length; index++) {
      source[index] = index + 1;
    }
    const buffer = source.buffer as ArrayBuffer;
    const fallback = new Uint8Array([255, 254, 253, 252, 251, 250, 249, 248]).buffer as ArrayBuffer;
    const key = 'slice';
    let handle: bigint = 0n;
    let mmapID: string = '';
    let rootDir: string = '';

    beforeAll(() => {
      const context = abilityDelegatorRegistry.getAbilityDelegator().getAppContext();
      rootDir = context.filesDir + '/mmkv_arraybuffer_bounds';
      MMKV.initializeWithPath(rootDir, context.cacheDir, MMKVLogLevel.LevelWarning);
      mmapID = 'arraybuffer_bounds_' + Date.now().toString() + '_' + Math.random().toString().slice(2);
      handle = native.mmkvWithID(mmapID, MMKV.SINGLE_PROCESS_MODE, undefined, rootDir);
      expect(handle !== 0n).assertTrue();
    });

    beforeEach(() => {
      expect(handle !== 0n).assertTrue();
      native.clearAll(handle, false);
      expect(native.disableAutoKeyExpire(handle)).assertTrue();
    });

    afterAll(() => {
      if (handle !== 0n) {
        native.close(handle);
        handle = 0n;
        expect(native.removeStorage(mmapID, rootDir)).assertTrue();
      }
    });

    const seed = (codec: SliceCodec): void => {
      expect(codec.encode(handle, key, buffer, codec.elementSize, codec.elementSize * 2)).assertTrue();
      expect(native.containsKey(handle, key)).assertTrue();
      assertBytes(codec.decode(handle, key, fallback), source.subarray(codec.elementSize, codec.elementSize * 3));
    };

    const reject = (codec: SliceCodec, offset: number, length: number, expiration?: number): void => {
      expect(codec.encode(handle, key, buffer, offset, length, expiration)).assertFalse();
      expect(native.containsKey(handle, key)).assertTrue();
      expect(native.count(handle, false) === 1n).assertTrue();
      assertBytes(codec.decode(handle, key, fallback), source.subarray(codec.elementSize, codec.elementSize * 3));
    };

    codecs.forEach((codec: SliceCodec) => {
      it(codec.name + '_valid_nonzero_offset', 0, () => {
        seed(codec); // Five arguments: the optional expiration is genuinely omitted.
        native.clearMemoryCache(handle);
        assertBytes(codec.decode(handle, key, fallback), source.subarray(codec.elementSize, codec.elementSize * 3));
      });

      it(codec.name + '_explicit_undefined_expiration', 0, () => {
        expect(codec.encode(handle, key, buffer, codec.elementSize, codec.elementSize * 2, undefined)).assertTrue();
        assertBytes(codec.decode(handle, key, fallback), source.subarray(codec.elementSize, codec.elementSize * 3));
      });

      it(codec.name + '_invalid_numbers_preserve_value', 0, () => {
        seed(codec);
        const invalid: number[] = [-1, 0.5, NaN, Infinity, -Infinity, 4294967295, 4294967296, 4294967297];
        invalid.forEach((value: number) => {
          reject(codec, value, codec.elementSize * 2);
          reject(codec, codec.elementSize, value);
        });
      });

      it(codec.name + '_out_of_bounds_preserve_value', 0, () => {
        seed(codec);
        reject(codec, buffer.byteLength + 1, 0);
        reject(codec, buffer.byteLength, codec.elementSize);
        reject(codec, buffer.byteLength - codec.elementSize, codec.elementSize * 2);
        reject(codec, 0, buffer.byteLength + codec.elementSize);
      });

      if (codec.elementSize > 1) {
        it(codec.name + '_misaligned_slices_preserve_value', 0, () => {
          seed(codec);
          reject(codec, 1, codec.elementSize);
          reject(codec, 0, codec.elementSize + 1);
        });
      }

      it(codec.name + '_empty_slices_roundtrip', 0, () => {
        [0, 1, buffer.byteLength].forEach((offset: number) => {
          seed(codec);
          expect(codec.encode(handle, key, buffer, offset, 0)).assertTrue();
          expect(native.containsKey(handle, key)).assertTrue();
          assertBytes(codec.decode(handle, key, fallback), new Uint8Array(0));
        });
        expect(codec.encode(handle, key, new ArrayBuffer(0), 0, 0)).assertTrue();
        expect(native.containsKey(handle, key)).assertTrue();
        assertBytes(codec.decode(handle, key, fallback), new Uint8Array(0));
      });

      it(codec.name + '_negative_zero_is_valid', 0, () => {
        expect(codec.encode(handle, key, buffer, -0, codec.elementSize)).assertTrue();
        assertBytes(codec.decode(handle, key, fallback), source.subarray(0, codec.elementSize));
      });

      it(codec.name + '_expiration_path_preserves_value_on_rejection', 0, () => {
        expect(native.enableAutoKeyExpire(handle, 0)).assertTrue();
        expect(codec.encode(handle, key, buffer, codec.elementSize, codec.elementSize * 2, 3600)).assertTrue();
        assertBytes(codec.decode(handle, key, fallback), source.subarray(codec.elementSize, codec.elementSize * 3));
        reject(codec, -1, codec.elementSize, 3600);
        reject(codec, buffer.byteLength, codec.elementSize, 3600);
        expect(codec.encode(handle, key, new ArrayBuffer(0), 0, 0, 3600)).assertTrue();
        expect(native.containsKey(handle, key)).assertTrue();
        assertBytes(codec.decode(handle, key, fallback), new Uint8Array(0));
      });
    });

    it('public_typed_subviews_roundtrip', 0, () => {
      const wrapperID = mmapID + '_public';
      const kv = MMKV.mmkvWithID(wrapperID, MMKV.SINGLE_PROCESS_MODE, undefined, rootDir);
      try {
        const int32 = new Int32Array([-99, -2147483648, 2147483647, 99]);
        const uint32 = new Uint32Array([99, 0, 4294967295, 99]);
        const int64 = new BigInt64Array([-99n, -9223372036854775808n, 9223372036854775807n, 99n]);
        const uint64 = new BigUint64Array([99n, 0n, 18446744073709551615n, 99n]);
        expect(kv.encodeTypedArray('int32', int32.subarray(1, 3))).assertTrue();
        expect(kv.encodeTypedArray('uint32', uint32.subarray(1, 3))).assertTrue();
        expect(kv.encodeTypedArray('int64', int64.subarray(1, 3))).assertTrue();
        expect(kv.encodeTypedArray('uint64', uint64.subarray(1, 3))).assertTrue();
        const int32Result = kv.decodeInt32Array('int32', new Int32Array(1))!;
        const uint32Result = kv.decodeUint32Array('uint32', new Uint32Array(1))!;
        const int64Result = kv.decodeInt64Array('int64', new BigInt64Array(1))!;
        const uint64Result = kv.decodeUint64Array('uint64', new BigUint64Array(1))!;
        expect(int32Result.length).assertEqual(2);
        expect(uint32Result.length).assertEqual(2);
        expect(int64Result.length).assertEqual(2);
        expect(uint64Result.length).assertEqual(2);
        for (let index = 0; index < 2; index++) {
          expect(int32Result[index]).assertEqual(int32[index + 1]);
          expect(uint32Result[index]).assertEqual(uint32[index + 1]);
          expect(int64Result[index] === int64[index + 1]).assertTrue();
          expect(uint64Result[index] === uint64[index + 1]).assertTrue();
        }
      } finally {
        kv.close();
        expect(MMKV.removeStorage(wrapperID, rootDir)).assertTrue();
      }
    });
  });
}
