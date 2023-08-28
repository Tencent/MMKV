package com.tencent.mmkv;

import androidx.annotation.Nullable;

import java.util.EnumMap;

import com.tencent.mmkv.MMKV.LibLoader;

import android.util.Log;

class JNIDelegate {
    /**
     * Notify MMKV that App is about to exit. It's totally fine not calling it at all.
     */
    public static native void onExit();

    /**
     * @return The encryption key (no more than 16 bytes).
     */
    @Nullable
    public native String cryptKey();

    /**
     * Transform plain text into encrypted text, or vice versa by passing a null encryption key.
     * You can also change existing crypt key with a different cryptKey.
     *
     * @param cryptKey The new encryption key (no more than 16 bytes).
     * @return True if success, otherwise False.
     */
    public native boolean reKey(@Nullable String cryptKey);

    /**
     * Just reset the encryption key (will not encrypt or decrypt anything).
     * Usually you should call this method after another process has {@link #reKey(String)} the multi-process MMKV instance.
     *
     * @param cryptKey The new encryption key (no more than 16 bytes).
     */
    public native void checkReSetCryptKey(@Nullable String cryptKey);

    /**
     * @return The device's memory page size.
     */
    public static native int pageSize();

    /**
     * @return The version of MMKV.
     */
    public static native String version();

    /**
     * @return The unique ID of the MMKV instance.
     */
    public native String mmapID();

    /**
     * Exclusively inter-process lock the MMKV instance.
     * It will block and wait until it successfully locks the file.
     * It will make no effect if the MMKV instance is created with {@link #SINGLE_PROCESS_MODE}.
     */
    public native void lock();

    /**
     * Exclusively inter-process unlock the MMKV instance.
     * It will make no effect if the MMKV instance is created with {@link #SINGLE_PROCESS_MODE}.
     */
    public native void unlock();

    /**
     * Try exclusively inter-process lock the MMKV instance.
     * It will not block if the file has already been locked by another process.
     * It will make no effect if the MMKV instance is created with {@link #SINGLE_PROCESS_MODE}.
     *
     * @return True if successfully locked, otherwise return immediately with False.
     */
    public native boolean tryLock();

    /**
     * Batch remove some keys from the MMKV instance.
     *
     * @param arrKeys The keys to be removed.
     */
    public native void removeValuesForKeys(String[] arrKeys);

    /**
     * Clear all the key-values inside the MMKV instance.
     */
    public native void clearAll();

    /**
     * The {@link #totalSize()} of an MMKV instance won't reduce after deleting key-values,
     * call this method after lots of deleting if you care about disk usage.
     * Note that {@link #clearAll()}  has a similar effect.
     */
    public native void trim();

    /**
     * Call this method if the MMKV instance is no longer needed in the near future.
     * Any subsequent call to any MMKV instances with the same ID is undefined behavior.
     */
    public native void close();

    /**
     * Clear memory cache of the MMKV instance.
     * You can call it on memory warning.
     * Any subsequent call to the MMKV instance will trigger all key-values loading from the file again.
     */
    public native void clearMemoryCache();

    native void sync(boolean sync);

    /**
     * Check whether the MMKV file is valid or not on customize folder.
     *
     * @param mmapID   The unique ID of the MMKV instance.
     * @param rootPath The folder of the MMKV instance, defaults to $(FilesDir)/mmkv.
     */
    public static native boolean isFileValid(String mmapID, @Nullable String rootPath);

    /**
     * backup one MMKV instance to dstDir
     *
     * @param mmapID   the MMKV ID to backup
     * @param rootPath the customize root path of the MMKV, if null then backup from the root dir of MMKV
     * @param dstDir   the backup destination directory
     */
    public static native boolean backupOneToDirectory(String mmapID, String dstDir, @Nullable String rootPath);

    /**
     * restore one MMKV instance from srcDir
     *
     * @param mmapID   the MMKV ID to restore
     * @param srcDir   the restore source directory
     * @param rootPath the customize root path of the MMKV, if null then restore to the root dir of MMKV
     */
    public static native boolean restoreOneMMKVFromDirectory(String mmapID, String srcDir, @Nullable String rootPath);

    /**
     * backup all MMKV instance to dstDir
     *
     * @param dstDir the backup destination directory
     * @return count of MMKV successfully backuped
     */
    public static native long backupAllToDirectory(String dstDir);

    /**
     * restore all MMKV instance from srcDir
     *
     * @param srcDir the restore source directory
     * @return count of MMKV successfully restored
     */
    public static native long restoreAllFromDirectory(String srcDir);

    /**
     * Enable auto key expiration. This is a upgrade operation, the file format will change.
     * And the file won't be accessed correctly by older version (v1.2.16) of MMKV.
     *
     * @param expireDurationInSecond the expire duration for all keys, {@link #ExpireNever} (0) means no default duration (aka each key will have it's own expire date)
     */
    public native boolean enableAutoKeyExpire(int expireDurationInSecond);

    /**
     * Disable auto key expiration. This is a downgrade operation.
     */
    public native boolean disableAutoKeyExpire();

    /**
     * @return The file descriptor of the ashmem of the MMKV file.
     */
    public native int ashmemFD();

    /**
     * @return The file descriptor of the ashmem of the MMKV crc file.
     */
    public native int ashmemMetaFD();

    static native void setWantsContentChangeNotify(boolean needsNotify);

    /**
     * Check inter-process content change manually.
     */
    public native void checkContentChangedByOuterProcess();

    static native void jniInitialize(String rootDir, String cacheDir, int level, boolean wantLogReDirecting);

    static native long
    getMMKVWithID(String mmapID, int mode, @Nullable String cryptKey, @Nullable String rootPath,
                  long expectedCapacity);

    static native long getMMKVWithIDAndSize(String mmapID, int size, int mode, @Nullable String cryptKey);

    static native long getDefaultMMKV(int mode, @Nullable String cryptKey);

    static native long getMMKVWithAshmemFD(String mmapID, int fd, int metaFD, @Nullable String cryptKey);

    native boolean encodeBool(long handle, String key, boolean value);

    native boolean encodeBool_2(long handle, String key, boolean value, int expireDurationInSecond);

    native boolean decodeBool(long handle, String key, boolean defaultValue);

    native boolean encodeInt(long handle, String key, int value);

    native boolean encodeInt_2(long handle, String key, int value, int expireDurationInSecond);

    native int decodeInt(long handle, String key, int defaultValue);

    native boolean encodeLong(long handle, String key, long value);

    native boolean encodeLong_2(long handle, String key, long value, int expireDurationInSecond);

    native long decodeLong(long handle, String key, long defaultValue);

    native boolean encodeFloat(long handle, String key, float value);

    native boolean encodeFloat_2(long handle, String key, float value, int expireDurationInSecond);

    native float decodeFloat(long handle, String key, float defaultValue);

    native boolean encodeDouble(long handle, String key, double value);

    native boolean encodeDouble_2(long handle, String key, double value, int expireDurationInSecond);

    native double decodeDouble(long handle, String key, double defaultValue);

    native boolean encodeString(long handle, String key, @Nullable String value);

    native boolean encodeString_2(long handle, String key, @Nullable String value, int expireDurationInSecond);

    @Nullable
    native String decodeString(long handle, String key, @Nullable String defaultValue);

    native boolean encodeSet(long handle, String key, @Nullable String[] value);

    native boolean encodeSet_2(long handle, String key, @Nullable String[] value, int expireDurationInSecond);

    @Nullable
    native String[] decodeStringSet(long handle, String key);

    native boolean encodeBytes(long handle, String key, @Nullable byte[] value);

    native boolean encodeBytes_2(long handle, String key, @Nullable byte[] value, int expireDurationInSecond);

    @Nullable
    native byte[] decodeBytes(long handle, String key);

    native boolean containsKey(long handle, String key);

    native String[] allKeys(long handle, boolean filterExpire);

    native long count(long handle, boolean filterExpire);

    native long totalSize(long handle);

    native long actualSize(long handle);

    native void removeValueForKey(long handle, String key);

    native int valueSize(long handle, String key, boolean actualSize);

    static native void setLogLevel(int level);

    static native void setCallbackHandler(boolean logReDirecting, boolean hasCallback);

    static native long createNB(int size);

    static native void destroyNB(long pointer, int size);

    native int writeValueToNB(long handle, String key, long pointer, int size);

    static native boolean checkProcessMode(long handle);

    // callback handler
    private static MMKVHandler gCallbackHandler;
    private static boolean gWantLogReDirecting = false;

    private static final EnumMap<MMKVRecoverStrategic, Integer> recoverIndex;
    private static final EnumMap<MMKVLogLevel, Integer> logLevel2Index;
    private static final MMKVLogLevel[] index2LogLevel;

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

        index2LogLevel = new MMKVLogLevel[]{MMKVLogLevel.LevelDebug, MMKVLogLevel.LevelInfo, MMKVLogLevel.LevelWarning,
                MMKVLogLevel.LevelError, MMKVLogLevel.LevelNone};
    }

    /**
     * Register a handler for MMKV log redirecting, and error handling.
     *
     * @deprecated This method is deprecated.
     * Use the {@link #initialize(Context, String, LibLoader, MMKVLogLevel, MMKVHandler)} method instead.
     */
    public static void registerHandler(MMKVHandler handler) {
        gCallbackHandler = handler;
        gWantLogReDirecting = gCallbackHandler.wantLogRedirecting();
        setCallbackHandler(gWantLogReDirecting, true);
    }

    /**
     * Unregister the handler for MMKV.
     */
    public static void unregisterHandler() {
        gCallbackHandler = null;

        setCallbackHandler(false, false);
        gWantLogReDirecting = false;
    }

    private static void mmkvLogImp(int level, String file, int line, String function, String message) {
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

    private static int onMMKVCRCCheckFail(String mmapID) {
        MMKVRecoverStrategic strategic = MMKVRecoverStrategic.OnErrorDiscard;
        if (gCallbackHandler != null) {
            strategic = gCallbackHandler.onMMKVCRCCheckFail(mmapID);
        }
        simpleLog(MMKVLogLevel.LevelInfo, "Recover strategic for " + mmapID + " is " + strategic);
        Integer value = recoverIndex.get(strategic);
        return (value == null) ? 0 : value;
    }

    private static int onMMKVFileLengthError(String mmapID) {
        MMKVRecoverStrategic strategic = MMKVRecoverStrategic.OnErrorDiscard;
        if (gCallbackHandler != null) {
            strategic = gCallbackHandler.onMMKVFileLengthError(mmapID);
        }
        simpleLog(MMKVLogLevel.LevelInfo, "Recover strategic for " + mmapID + " is " + strategic);
        Integer value = recoverIndex.get(strategic);
        return (value == null) ? 0 : value;
    }

    static void simpleLog(MMKVLogLevel level, String message) {
        StackTraceElement[] stacktrace = Thread.currentThread().getStackTrace();
        StackTraceElement e = stacktrace[stacktrace.length - 1];
        Integer i = logLevel2Index.get(level);
        int intLevel = (i == null) ? 0 : i;
        mmkvLogImp(intLevel, e.getFileName(), e.getLineNumber(), e.getMethodName(), message);
    }

    static private String rootDir = null;

    public static String getRootDir() {
        return rootDir;
    }

    static String doInitialize(String rootDir, String cacheDir, LibLoader loader, MMKVLogLevel logLevel, boolean wantLogReDirecting) {
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
        jniInitialize(rootDir, cacheDir, logLevel2Int(logLevel), wantLogReDirecting);
        JNIDelegate.rootDir = rootDir;
        return rootDir;
    }

    static String doInitialize(String rootDir, String cacheDir, LibLoader loader, MMKVLogLevel logLevel, MMKVHandler handler) {
        gCallbackHandler = handler;
        if (gCallbackHandler != null && gCallbackHandler.wantLogRedirecting()) {
            gWantLogReDirecting = true;
        }

        String ret = doInitialize(rootDir, cacheDir, loader, logLevel, gWantLogReDirecting);

        if (gCallbackHandler != null) {
            setCallbackHandler(gWantLogReDirecting, true);
        }

        return ret;
    }

    private static int logLevel2Int(MMKVLogLevel level) {
        int realLevel;
        switch (level) {
            case LevelDebug:
                realLevel = 0;
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
            case LevelInfo:
            default:
                realLevel = 1;
                break;
        }
        return realLevel;
    }

    // content change notification of other process
    // trigger by getXXX() or setXXX() or checkContentChangedByOuterProcess()
    private static MMKVContentChangeNotification gContentChangeNotify;

    /**
     * Register for MMKV inter-process content change notification.
     * The notification will trigger only when any method is manually called on the MMKV instance.
     * For example {@link #checkContentChangedByOuterProcess()}.
     *
     * @param notify The notification handler.
     */
    public static void registerContentChangeNotify(MMKVContentChangeNotification notify) {
        gContentChangeNotify = notify;
        setWantsContentChangeNotify(gContentChangeNotify != null);
    }

    /**
     * Unregister for MMKV inter-process content change notification.
     */
    public static void unregisterContentChangeNotify() {
        gContentChangeNotify = null;
        setWantsContentChangeNotify(false);
    }

    private static void onContentChangedByOuterProcess(String mmapID) {
        if (gContentChangeNotify != null) {
            gContentChangeNotify.onContentChangedByOuterProcess(mmapID);
        }
    }

    /**
     * Set the log level of MMKV.
     *
     * @param level Defaults to {@link MMKVLogLevel#LevelInfo}.
     */
    public static void setLogLevel(MMKVLogLevel level) {
        int realLevel = logLevel2Int(level);
        setLogLevel(realLevel);
    }

    private final long nativeHandle;

    JNIDelegate(long handle) {
        this.nativeHandle = handle;
    }
}
