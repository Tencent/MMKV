package com.tencent.mmkv;

public final class NativeBuffer {
    public long pointer;
    public int size;

    public NativeBuffer(long ptr, int length) {
        pointer = ptr;
        size = length;
    }
}
