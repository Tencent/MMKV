/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2018 THL A29 Limited, a Tencent company.
 * All rights reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use
 * this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *       https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.tencent.mmkv;

import android.content.ContentResolver;
import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.support.annotation.Nullable;
import android.util.Log;

import java.lang.reflect.Field;
import java.util.Arrays;
import java.util.EnumMap;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public class MMKV implements SharedPreferences, SharedPreferences.Editor {

    private static EnumMap<MMKVRecoverStrategic, Integer> recoverIndex;
    private static EnumMap<MMKVLogLevel, Integer> logLevel2Index;
    private static MMKVLogLevel[] index2LogLevel;
    static {
        recoverIndex = new EnumMap<>(MMKVRecoverStrategic.class);
        recoverIndex.put(MMKVRecoverStrategic.OnErrorDiscard, 0);
        recoverIndex.put(MMKVRecoverStrategic.OnErrorRecover, 1);

        logLevel2Index = new EnumMap<>(MMKVLogLevel.class);
        logLevel2Index.put(MMKVLogLevel.LevelDebug, 0);
        logLevel2Index.put(MMKVLogLevel.LevelInfo, 1);
        logLevel2Index.put(MMKVLogLevel.LevelWarning, 2);
        logLevel2Index.put(MMKVLogLevel.LevelError, 3);
        logLevel2Index.put(MMKVLogLevel.LevelNone, 4);

        index2LogLevel = new MMKVLogLevel[] {MMKVLogLevel.LevelDebug, MMKVLogLevel.LevelInfo,
                                             MMKVLogLevel.LevelWarning, MMKVLogLevel.LevelError,
                                             MMKVLogLevel.LevelNone};
    }

    public interface LibLoader { void loadLibrary(String libName); }

    // call on program start
    public static String initialize(Context context) {
        String root = context.getFilesDir().getAbsolutePath() + "/mmkv";
        return initialize(root, null);
    }

    public static String initialize(String rootDir) {
        return initialize(rootDir, null);
    }

    public static String initialize(String rootDir, LibLoader loader) {
        if (loader != null) {
            if (BuildConfig.FLAVOR.equals("SharedCpp")) {
                loader.loadLibrary("c++_shared");
            }
            loader.loadLibrary("mmkv");
        } else {
            if (BuildConfig.FLAVOR.equals("SharedCpp")) {
                System.loadLibrary("c++_shared");
            }
            System.loadLibrary("mmkv");
        }
        MMKV.rootDir = rootDir;
        jniInitialize(MMKV.rootDir);
        return rootDir;
    }

    static private String rootDir = null;
    public static String getRootDir() {
        return rootDir;
    }

    public static void setLogLevel(MMKVLogLevel level) {
        int realLevel;
        switch (level) {
            case LevelDebug:
                realLevel = 0;
                break;
            case LevelInfo:
                realLevel = 1;
                break;
            case LevelWarning:
                realLevel = 2;
                break;
            case LevelError:
                realLevel = 3;
                break;
            case LevelNone:
                realLevel = 4;
                break;
            default:
                realLevel = 1;
                break;
        }
        setLogLevel(realLevel);
    }

    // call on program exit
    public static native void onExit();

    static public final int SINGLE_PROCESS_MODE = 0x1;

    static public final int MULTI_PROCESS_MODE = 0x2;

    // in case someone mistakenly pass Context.MODE_MULTI_PROCESS
    static private final int CONTEXT_MODE_MULTI_PROCESS = 0x4;

    static private final int ASHMEM_MODE = 0x8;

    public static MMKV mmkvWithID(String mmapID) {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        long handle = getMMKVWithID(mmapID, SINGLE_PROCESS_MODE, null, null);
        return new MMKV(handle);
    }

    public static MMKV mmkvWithID(String mmapID, int mode) {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        long handle = getMMKVWithID(mmapID, mode, null, null);
        return new MMKV(handle);
    }

    // cryptKey's length <= 16
    public static MMKV mmkvWithID(String mmapID, int mode, String cryptKey) {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        long handle = getMMKVWithID(mmapID, mode, cryptKey, null);
        return new MMKV(handle);
    }

    @Nullable
    public static MMKV mmkvWithID(String mmapID, String relativePath) {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        long handle = getMMKVWithID(mmapID, SINGLE_PROCESS_MODE, null, relativePath);
        if (handle == 0) {
            return null;
        }
        return new MMKV(handle);
    }

    // cryptKey's length <= 16
    @Nullable
    public static MMKV mmkvWithID(String mmapID, int mode, String cryptKey, String relativePath) {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        long handle = getMMKVWithID(mmapID, mode, cryptKey, relativePath);
        if (handle == 0) {
            return null;
        }
        return new MMKV(handle);
    }

    // a memory only MMKV, cleared on program exit
    // size cannot change afterward (because ashmem won't allow it)
    @Nullable
    public static MMKV
    mmkvWithAshmemID(Context context, String mmapID, int size, int mode, String cryptKey) {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        String processName =
            MMKVContentProvider.getProcessNameByPID(context, android.os.Process.myPid());
        if (processName == null || processName.length() == 0) {
            simpleLog(MMKVLogLevel.LevelError, "process name detect fail, try again later");
            return null;
        }
        if (processName.contains(":")) {
            Uri uri = MMKVContentProvider.contentUri(context);
            if (uri == null) {
                simpleLog(MMKVLogLevel.LevelError, "MMKVContentProvider has invalid authority");
                return null;
            }
            simpleLog(MMKVLogLevel.LevelInfo, "getting parcelable mmkv in process, Uri = " + uri);

            Bundle extras = new Bundle();
            extras.putInt(MMKVContentProvider.KEY_SIZE, size);
            extras.putInt(MMKVContentProvider.KEY_MODE, mode);
            if (cryptKey != null) {
                extras.putString(MMKVContentProvider.KEY_CRYPT, cryptKey);
            }
            ContentResolver resolver = context.getContentResolver();
            Bundle result = resolver.call(uri, MMKVContentProvider.FUNCTION_NAME, mmapID, extras);
            if (result != null) {
                result.setClassLoader(ParcelableMMKV.class.getClassLoader());
                ParcelableMMKV parcelableMMKV = result.getParcelable(MMKVContentProvider.KEY);
                if (parcelableMMKV != null) {
                    MMKV mmkv = parcelableMMKV.toMMKV();
                    if (mmkv != null) {
                        simpleLog(MMKVLogLevel.LevelInfo,
                                  mmkv.mmapID() + " fd = " + mmkv.ashmemFD()
                                      + ", meta fd = " + mmkv.ashmemMetaFD());
                    }
                    return mmkv;
                }
            }
        } else {
            simpleLog(MMKVLogLevel.LevelInfo, "getting mmkv in main process");

            mode = mode | ASHMEM_MODE;
            long handle = getMMKVWithIDAndSize(mmapID, size, mode, cryptKey);
            return new MMKV(handle);
        }
        return null;
    }

    public static MMKV defaultMMKV() {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        long handle = getDefaultMMKV(SINGLE_PROCESS_MODE, null);
        return new MMKV(handle);
    }

    public static MMKV defaultMMKV(int mode, String cryptKey) {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        long handle = getDefaultMMKV(mode, cryptKey);
        return new MMKV(handle);
    }

    // encryption & decryption key
    public native String cryptKey();

    // transform plain text into encrypted text
    // or vice versa by passing cryptKey = null
    // or change existing crypt key
    public native boolean reKey(String cryptKey);

    // just reset cryptKey (will not encrypt or decrypt anything)
    // usually you should call this method after other process reKey() the inter-process mmkv
    public native void checkReSetCryptKey(String cryptKey);

    // get device's page size
    public static native int pageSize();

    public native String mmapID();

    public native void lock();

    public native void unlock();

    public native boolean tryLock();

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

    private static final HashMap<String, Parcelable.Creator<?>> mCreators = new HashMap<>();

    public boolean encode(String key, Parcelable value) {
        Parcel source = Parcel.obtain();
        value.writeToParcel(source, value.describeContents());
        byte[] bytes = source.marshall();
        source.recycle();

        return encodeBytes(nativeHandle, key, bytes);
    }

    @SuppressWarnings("unchecked")
    public <T extends Parcelable> T decodeParcelable(String key, Class<T> tClass) {
        return decodeParcelable(key, tClass, null);
    }

    @SuppressWarnings("unchecked")
    public <T extends Parcelable> T decodeParcelable(String key, Class<T> tClass, T defaultValue) {
        if (tClass == null) {
            return defaultValue;
        }

        byte[] bytes = decodeBytes(nativeHandle, key);
        if (bytes == null) {
            return defaultValue;
        }

        Parcel source = Parcel.obtain();
        source.unmarshall(bytes, 0, bytes.length);
        source.setDataPosition(0);

        try {
            String name = tClass.toString();
            Parcelable.Creator<T> creator;
            synchronized (mCreators) {
                creator = (Parcelable.Creator<T>) mCreators.get(name);
                if (creator == null) {
                    Field f = tClass.getField("CREATOR");
                    creator = (Parcelable.Creator<T>) f.get(null);
                    if (creator != null) {
                        mCreators.put(name, creator);
                    }
                }
            }
            if (creator != null) {
                return creator.createFromParcel(source);
            } else {
                throw new Exception("Parcelable protocol requires a "
                                    + "non-null static Parcelable.Creator object called "
                                    + "CREATOR on class " + name);
            }
        } catch (Exception e) {
            simpleLog(MMKVLogLevel.LevelError, e.toString());
        } finally {
            source.recycle();
        }
        return defaultValue;
    }

    // return the actual size consumption of the key's value
    // Note: might be a little bigger than value's length
    public int getValueSize(String key) {
        return valueSize(nativeHandle, key, false);
    }

    // return the actual size of the key's value
    // String's length or byte[]'s length, etc
    public int getValueActualSize(String key) {
        return valueSize(nativeHandle, key, true);
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

    // MMKV's size won't reduce after deleting key-values
    // call this method after lots of deleting f you care about disk usage
    // note that `clearAll` has the similar effect of `trim`
    public native void trim();

    // call this method if the instance is no longer needed in the near future
    // any subsequent call to the instance is undefined behavior
    public native void close();

    // call on memory warning
    // any subsequent call to the instance will load all key-values from file again
    public native void clearMemoryCache();

    // you don't need to call this, really, I mean it
    // unless you care about out of battery
    public void sync() {
        sync(true);
    }

    public void async() {
        sync(false);
    }

    private native void sync(boolean sync);

    public static native boolean isFileValid(String mmapID);

    // SharedPreferences migration
    @SuppressWarnings("unchecked")
    public int importFromSharedPreferences(SharedPreferences preferences) {
        Map<String, ?> kvs = preferences.getAll();
        if (kvs == null || kvs.size() <= 0) {
            return 0;
        }

        for (Map.Entry<String, ?> entry : kvs.entrySet()) {
            String key = entry.getKey();
            Object value = entry.getValue();
            if (key == null || value == null) {
                continue;
            }

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
                simpleLog(MMKVLogLevel.LevelError, "unknown type: " + value.getClass());
            }
        }
        return kvs.size();
    }

    @Override
    public Map<String, ?> getAll() {
        throw new java.lang.UnsupportedOperationException(
            "use allKeys() instead, getAll() not implement because type-erasure inside mmkv");
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
        sync(true);
        return true;
    }

    @Override
    public void apply() {
        sync(false);
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
    public void
    registerOnSharedPreferenceChangeListener(OnSharedPreferenceChangeListener listener) {
        throw new java.lang.UnsupportedOperationException("Not implement in MMKV");
    }

    @Override
    public void
    unregisterOnSharedPreferenceChangeListener(OnSharedPreferenceChangeListener listener) {
        throw new java.lang.UnsupportedOperationException("Not implement in MMKV");
    }

    // Parcelable
    public static MMKV mmkvWithAshmemFD(String mmapID, int fd, int metaFD, String cryptKey) {
        long handle = getMMKVWithAshmemFD(mmapID, fd, metaFD, cryptKey);
        return new MMKV(handle);
    }

    public native int ashmemFD();

    public native int ashmemMetaFD();

    // native buffer
    public static NativeBuffer createNativeBuffer(int size) {
        long pointer = createNB(size);
        if (pointer <= 0) {
            return null;
        }
        return new NativeBuffer(pointer, size);
    }

    public static void destroyNativeBuffer(NativeBuffer buffer) {
        destroyNB(buffer.pointer, buffer.size);
    }

    // return size written, -1 on error
    public int writeValueToNativeBuffer(String key, NativeBuffer buffer) {
        return writeValueToNB(nativeHandle, key, buffer.pointer, buffer.size);
    }

    // callback handler
    private static MMKVHandler gCallbackHandler;
    private static boolean gWantLogReDirecting = false;
    public static void registerHandler(MMKVHandler handler) {
        gCallbackHandler = handler;

        if (gCallbackHandler.wantLogRedirecting()) {
            setLogReDirecting(true);
            gWantLogReDirecting = true;
        } else {
            setLogReDirecting(false);
            gWantLogReDirecting = false;
        }
    }

    public static void unregisterHandler() {
        gCallbackHandler = null;

        setLogReDirecting(false);
        gWantLogReDirecting = false;
    }

    private static int onMMKVCRCCheckFail(String mmapID) {
        MMKVRecoverStrategic strategic = MMKVRecoverStrategic.OnErrorDiscard;
        if (gCallbackHandler != null) {
            strategic = gCallbackHandler.onMMKVCRCCheckFail(mmapID);
        }
        simpleLog(MMKVLogLevel.LevelInfo, "Recover strategic for " + mmapID + " is " + strategic);
        return recoverIndex.get(strategic);
    }

    private static int onMMKVFileLengthError(String mmapID) {
        MMKVRecoverStrategic strategic = MMKVRecoverStrategic.OnErrorDiscard;
        if (gCallbackHandler != null) {
            strategic = gCallbackHandler.onMMKVFileLengthError(mmapID);
        }
        simpleLog(MMKVLogLevel.LevelInfo, "Recover strategic for " + mmapID + " is " + strategic);
        return recoverIndex.get(strategic);
    }

    private static void
    mmkvLogImp(int level, String file, int line, String function, String message) {
        if (gCallbackHandler != null && gWantLogReDirecting) {
            gCallbackHandler.mmkvLog(index2LogLevel[level], file, line, function, message);
        } else {
            switch (index2LogLevel[level]) {
                case LevelDebug:
                    Log.d("MMKV", message);
                    break;
                case LevelInfo:
                    Log.i("MMKV", message);
                    break;
                case LevelWarning:
                    Log.w("MMKV", message);
                    break;
                case LevelError:
                    Log.e("MMKV", message);
                    break;
                case LevelNone:
                    break;
            }
        }
    }

    private static void simpleLog(MMKVLogLevel level, String message) {
        StackTraceElement[] stacktrace = Thread.currentThread().getStackTrace();
        StackTraceElement e = stacktrace[stacktrace.length - 1];
        mmkvLogImp(logLevel2Index.get(level), e.getFileName(), e.getLineNumber(), e.getMethodName(),
                   message);
    }

    // jni
    private long nativeHandle;

    private MMKV(long handle) {
        nativeHandle = handle;
    }

    private static native void jniInitialize(String rootDir);

    private native static long
    getMMKVWithID(String mmapID, int mode, String cryptKey, String relativePath);

    private native static long
    getMMKVWithIDAndSize(String mmapID, int size, int mode, String cryptKey);

    private native static long getDefaultMMKV(int mode, String cryptKey);

    private native static long
    getMMKVWithAshmemFD(String mmapID, int fd, int metaFD, String cryptKey);

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

    private native int valueSize(long handle, String key, boolean actualSize);

    private static native void setLogLevel(int level);

    private static native void setLogReDirecting(boolean enable);

    private static native long createNB(int size);

    private static native void destroyNB(long pointer, int size);

    private native int writeValueToNB(long handle, String key, long pointer, int size);
}
