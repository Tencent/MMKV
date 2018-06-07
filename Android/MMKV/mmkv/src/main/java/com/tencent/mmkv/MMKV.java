package com.tencent.mmkv;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.annotation.Nullable;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public class MMKV implements SharedPreferences, SharedPreferences.Editor {
    // call on program start
    public static String initialize(Context context) {
        String rootDir = context.getFilesDir().getAbsolutePath() + "/mmkv";
        initialize(rootDir);
        return rootDir;
    }

    // call on program exit
    public static native void onExit();

    static public final boolean SINGLE_THREAD_MODE = false;

    static public final boolean MULTI_THREAD_MODE = true;

    public static MMKV mmkvWithID(String mmapID) {
        long handle = getMMKVWithID(mmapID, false);
        return new MMKV(handle);
    }

    public static MMKV mmkvWithID(String mmapID, boolean threadMode) {
        long handle = getMMKVWithID(mmapID, threadMode);
        return new MMKV(handle);
    }

    public static MMKV defaultMMKV() {
        long handle = getDefaultMMKV(false);
        return new MMKV(handle);
    }

    public static MMKV defaultMMKV(boolean threadMode) {
        long handle = getDefaultMMKV(threadMode);
        return new MMKV(handle);
    }
//    public native void lock();
//    public native void unlock();

    public native String mmapID();

    public boolean encode(String key, boolean value) {
        return encodeBool(nativeHandle, key, value);
    }

    public boolean decodeBool(String key) {
        return decodeBool(nativeHandle, key, false);
    }

    public boolean decodeBool(String key, boolean defaultValue) {
        return decodeBool(nativeHandle, key, defaultValue);
    }

    public boolean encode(String key, int value) {
        return encodeInt(nativeHandle, key, value);
    }

    public int decodeInt(String key) {
        return decodeInt(nativeHandle, key, 0);
    }

    public int decodeInt(String key, int defaultValue) {
        return decodeInt(nativeHandle, key, defaultValue);
    }

    public boolean encode(String key, long value) {
        return encodeLong(nativeHandle, key, value);
    }

    public long decodeLong(String key) {
        return decodeLong(nativeHandle, key, 0);
    }

    public long decodeLong(String key, long defaultValue) {
        return decodeLong(nativeHandle, key, defaultValue);
    }

    public boolean encode(String key, float value) {
        return encodeFloat(nativeHandle, key, value);
    }

    public float decodeFloat(String key) {
        return decodeFloat(nativeHandle, key, 0);
    }

    public float decodeFloat(String key, float defaultValue) {
        return decodeFloat(nativeHandle, key, defaultValue);
    }

    public boolean encode(String key, double value) {
        return encodeDouble(nativeHandle, key, value);
    }

    public double decodeDouble(String key) {
        return decodeDouble(nativeHandle, key, 0);
    }

    public double decodeDouble(String key, double defaultValue) {
        return decodeDouble(nativeHandle, key, defaultValue);
    }

    public boolean encode(String key, String value) {
        return encodeString(nativeHandle, key, value);
    }

    public String decodeString(String key) {
        return decodeString(nativeHandle, key, null);
    }

    public String decodeString(String key, String defaultValue) {
        return decodeString(nativeHandle, key, defaultValue);
    }

    public boolean encode(String key, Set<String> value) {
        return encodeSet(nativeHandle, key, value.toArray(new String[value.size()]));
    }

    public Set<String> decodeStringSet(String key) {
        return decodeStringSet(key, null);
    }

    public Set<String> decodeStringSet(String key, Set<String> defaultValue) {
        String[] result = decodeStringSet(nativeHandle, key);
        if (result == null) {
            return defaultValue;
        }
        return new HashSet<String>(Arrays.asList(result));
    }

    public boolean encode(String key, byte[] value) {
        return encodeBytes(nativeHandle, key, value);
    }

    public byte[] decodeBytes(String key) {
        return decodeBytes(nativeHandle, key);
    }

    public boolean containsKey(String key) {
        return containsKey(nativeHandle, key);
    }

    public native String[] allKeys();

    public long count() {
        return count(nativeHandle);
    }

    // used file size
    public long totalSize() {
        return totalSize(nativeHandle);
    }

    public void removeValueForKey(String key) {
        removeValueForKey(nativeHandle, key);
    }

    public native void removeValuesForKeys(String[] arrKeys);

    public native void clearAll();

    // you don't need to call this, really, I mean it
    // unless you care about out of battery
    public native void sync();

    public static native boolean isFileValid(String mmapID);

    // SharedPreferences migration
    @SuppressWarnings("unchecked")
    public int importFromSharedPreferences(SharedPreferences preferences) {
        Map<String, ?> kvs = preferences.getAll();
        if (kvs.size() <= 0) {
            return 0;
        }

        for (Map.Entry<String, ?> entry : kvs.entrySet()) {
            String key = entry.getKey();
            Object value = entry.getValue();
            if (value instanceof Boolean) {
                encodeBool(nativeHandle, key, (boolean) value);
            } else if (value instanceof Integer) {
                encodeInt(nativeHandle, key, (int) value);
            } else if (value instanceof Long) {
                encodeLong(nativeHandle, key, (long) value);
            } else if (value instanceof Float) {
                encodeFloat(nativeHandle, key, (float) value);
            } else if (value instanceof Double) {
                encodeDouble(nativeHandle, key, (double) value);
            } else if (value instanceof String) {
                encodeString(nativeHandle, key, (String) value);
            } else if (value instanceof Set) {
                encode(key, (Set<String>) value);
            } else {
                System.out.println("unknown type: " + value.getClass());
            }
        }
        return kvs.size();
    }

    @Override
    public Map<String, ?> getAll() {
        throw new java.lang.UnsupportedOperationException("use allKeys() instead, getAll() not implement because type-erasure inside mmkv");
    }

    @Nullable
    @Override
    public String getString(String key, @Nullable String defValue) {
        return decodeString(nativeHandle, key, defValue);
    }

    @Override
    public Editor putString(String key, @Nullable String value) {
        encodeString(nativeHandle, key, value);
        return this;
    }

    @Nullable
    @Override
    public Set<String> getStringSet(String key, @Nullable Set<String> defValues) {
        return decodeStringSet(key, defValues);
    }

    @Override
    public Editor putStringSet(String key, @Nullable Set<String> values) {
        encode(key, values);
        return this;
    }

    @Override
    public int getInt(String key, int defValue) {
        return decodeInt(nativeHandle, key, defValue);
    }

    @Override
    public Editor putInt(String key, int value) {
        encodeInt(nativeHandle, key, value);
        return this;
    }

    @Override
    public long getLong(String key, long defValue) {
        return decodeLong(nativeHandle, key, defValue);
    }

    @Override
    public Editor putLong(String key, long value) {
        encodeLong(nativeHandle, key, value);
        return this;
    }

    @Override
    public float getFloat(String key, float defValue) {
        return decodeFloat(nativeHandle, key, defValue);
    }

    @Override
    public Editor putFloat(String key, float value) {
        encodeFloat(nativeHandle, key, value);
        return this;
    }

    @Override
    public boolean getBoolean(String key, boolean defValue) {
        return decodeBool(nativeHandle, key, defValue);
    }

    @Override
    public Editor putBoolean(String key, boolean value) {
        encodeBool(nativeHandle, key, value);
        return this;
    }

    @Override
    public Editor remove(String key) {
        removeValueForKey(key);
        return this;
    }

    @Override
    public Editor clear() {
        clearAll();
        return this;
    }

    @Override
    public boolean commit() {
        sync();
        return true;
    }

    @Override
    public void apply() {
        // TODO: create a thread?
        //sync();
    }

    @Override
    public boolean contains(String key) {
        return containsKey(key);
    }

    @Override
    public Editor edit() {
        return this;
    }

    @Override
    public void registerOnSharedPreferenceChangeListener(OnSharedPreferenceChangeListener listener) {
        throw new java.lang.UnsupportedOperationException("Not implement in MMKV");
    }

    @Override
    public void unregisterOnSharedPreferenceChangeListener(OnSharedPreferenceChangeListener listener) {
        throw new java.lang.UnsupportedOperationException("Not implement in MMKV");
    }

    // jni
    private MMKV(long handle) {
        nativeHandle = handle;
    }

    private static native void initialize(String rootDir);

    private native static long getMMKVWithID(String mmapID, boolean isMultiThread);

    private native static long getDefaultMMKV(boolean isMultiThread);

    private native boolean encodeBool(long handle, String key, boolean value);

    private native boolean decodeBool(long handle, String key, boolean defaultValue);

    private native boolean encodeInt(long handle, String key, int value);

    private native int decodeInt(long handle, String key, int defaultValue);

    private native boolean encodeLong(long handle, String key, long value);

    private native long decodeLong(long handle, String key, long defaultValue);

    private native boolean encodeFloat(long handle, String key, float value);

    private native float decodeFloat(long handle, String key, float defaultValue);

    private native boolean encodeDouble(long handle, String key, double value);

    private native double decodeDouble(long handle, String key, double defaultValue);

    private native boolean encodeString(long handle, String key, String value);

    private native String decodeString(long handle, String key, String defaultValue);

    private native boolean encodeSet(long handle, String key, String[] value);

    private native String[] decodeStringSet(long handle, String key);

    private native boolean encodeBytes(long handle, String key, byte[] value);

    private native byte[] decodeBytes(long handle, String key);

    private native boolean containsKey(long handle, String key);

    private native long count(long handle);

    private native long totalSize(long handle);

    private native void removeValueForKey(long handle, String key);

    private long nativeHandle;
}
