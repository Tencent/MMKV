package com.tencent.mmkv;

/**
 * A native memory wrapper, whose underlying memory can be passed to another JNI method directly.
 * Avoiding unnecessary JNI boxing and unboxing.
 * Must be destroy manually {@link MMKV#destroyNativeBuffer}.
 */
public final class NativeBuffer {
    public long pointer;
    public int size;

    public NativeBuffer(long ptr, int length) {
        pointer = ptr;
        size = length;
    }
}
