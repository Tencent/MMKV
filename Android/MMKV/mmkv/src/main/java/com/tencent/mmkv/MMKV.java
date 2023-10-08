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
import android.content.pm.ApplicationInfo;
import android.net.Uri;
import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import dalvik.annotation.optimization.FastNative;

import java.lang.reflect.Field;
import java.util.Arrays;
import java.util.EnumMap;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * An highly efficient, reliable, multi-process key-value storage framework.
 * THE PERFECT drop-in replacement for SharedPreferences and MultiProcessSharedPreferences.
 */
public class MMKV implements SharedPreferences, SharedPreferences.Editor {

    private static final EnumMap<MMKVRecoverStrategic, Integer> recoverIndex;
    private static final EnumMap<MMKVLogLevel, Integer> logLevel2Index;
    private static final MMKVLogLevel[] index2LogLevel;
    private static final Set<Long> checkedHandleSet;

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

        checkedHandleSet = new HashSet<Long>();
    }

    /**
     * The interface for providing a 3rd library loader (the ReLinker https://github.com/KeepSafe/ReLinker, etc).
     */
    public interface LibLoader {
        void loadLibrary(String libName);
    }

    /**
     * Initialize MMKV with default configuration.
     * You must call one of the initialize() methods on App startup process before using MMKV.
     *
     * @param context The context of Android App, usually from Application.
     * @return The root folder of MMKV, defaults to $(FilesDir)/mmkv.
     */
    public static String initialize(Context context) {
        String root = context.getFilesDir().getAbsolutePath() + "/mmkv";
        MMKVLogLevel logLevel = BuildConfig.DEBUG ? MMKVLogLevel.LevelDebug : MMKVLogLevel.LevelInfo;
        return initialize(context, root, null, logLevel, null);
    }

    /**
     * Initialize MMKV with customize log level.
     * You must call one of the initialize() methods on App startup process before using MMKV.
     *
     * @param context  The context of Android App, usually from Application.
     * @param logLevel The log level of MMKV, defaults to {@link MMKVLogLevel#LevelInfo}.
     * @return The root folder of MMKV, defaults to $(FilesDir)/mmkv.
     */
    public static String initialize(Context context, MMKVLogLevel logLevel) {
        String root = context.getFilesDir().getAbsolutePath() + "/mmkv";
        return initialize(context, root, null, logLevel, null);
    }

    /**
     * Initialize MMKV with a 3rd library loader.
     * You must call one of the initialize() methods on App startup process before using MMKV.
     *
     * @param context The context of Android App, usually from Application.
     * @param loader  The 3rd library loader (for example, the <a href="https://github.com/KeepSafe/ReLinker">ReLinker</a> .
     * @return The root folder of MMKV, defaults to $(FilesDir)/mmkv.
     */
    public static String initialize(Context context, LibLoader loader) {
        String root = context.getFilesDir().getAbsolutePath() + "/mmkv";
        MMKVLogLevel logLevel = BuildConfig.DEBUG ? MMKVLogLevel.LevelDebug : MMKVLogLevel.LevelInfo;
        return initialize(context, root, loader, logLevel, null);
    }

    /**
     * Initialize MMKV with a 3rd library loader, and customize log level.
     * You must call one of the initialize() methods on App startup process before using MMKV.
     *
     * @param context  The context of Android App, usually from Application.
     * @param loader   The 3rd library loader (for example, the <a href="https://github.com/KeepSafe/ReLinker">ReLinker</a> .
     * @param logLevel The log level of MMKV, defaults to {@link MMKVLogLevel#LevelInfo}.
     * @return The root folder of MMKV, defaults to $(FilesDir)/mmkv.
     */
    public static String initialize(Context context, LibLoader loader, MMKVLogLevel logLevel) {
        String root = context.getFilesDir().getAbsolutePath() + "/mmkv";
        return initialize(context, root, loader, logLevel, null);
    }

    /**
     * Initialize MMKV with customize root folder.
     * You must call one of the initialize() methods on App startup process before using MMKV.
     *
     * @param context The context of Android App, usually from Application.
     * @param rootDir The root folder of MMKV, defaults to $(FilesDir)/mmkv.
     * @return The root folder of MMKV.
     */
    public static String initialize(Context context, String rootDir) {
        MMKVLogLevel logLevel = BuildConfig.DEBUG ? MMKVLogLevel.LevelDebug : MMKVLogLevel.LevelInfo;
        return initialize(context, rootDir, null, logLevel, null);
    }

    /**
     * Initialize MMKV with customize root folder, and log level.
     * You must call one of the initialize() methods on App startup process before using MMKV.
     *
     * @param context  The context of Android App, usually from Application.
     * @param rootDir  The root folder of MMKV, defaults to $(FilesDir)/mmkv.
     * @param logLevel The log level of MMKV, defaults to {@link MMKVLogLevel#LevelInfo}.
     * @return The root folder of MMKV.
     */
    public static String initialize(Context context, String rootDir, MMKVLogLevel logLevel) {
        return initialize(context, rootDir, null, logLevel, null);
    }

    /**
     * Initialize MMKV with customize root folder, and a 3rd library loader.
     * You must call one of the initialize() methods on App startup process before using MMKV.
     *
     * @param context The context of Android App, usually from Application.
     * @param rootDir The root folder of MMKV, defaults to $(FilesDir)/mmkv.
     * @param loader  The 3rd library loader (for example, the <a href="https://github.com/KeepSafe/ReLinker">ReLinker</a> .
     * @return The root folder of MMKV.
     */
    public static String initialize(Context context, String rootDir, LibLoader loader) {
        MMKVLogLevel logLevel = BuildConfig.DEBUG ? MMKVLogLevel.LevelDebug : MMKVLogLevel.LevelInfo;
        return initialize(context, rootDir, loader, logLevel, null);
    }

    /**
     * Initialize MMKV with customize settings.
     * You must call one of the initialize() methods on App startup process before using MMKV.
     *
     * @param context  The context of Android App, usually from Application.
     * @param rootDir  The root folder of MMKV, defaults to $(FilesDir)/mmkv.
     * @param loader   The 3rd library loader (for example, the <a href="https://github.com/KeepSafe/ReLinker">ReLinker</a> .
     * @param logLevel The log level of MMKV, defaults to {@link MMKVLogLevel#LevelInfo}.
     * @return The root folder of MMKV.
     */
    public static String initialize(Context context, String rootDir, LibLoader loader, MMKVLogLevel logLevel) {
        return initialize(context, rootDir, loader, logLevel, null);
    }

    public static String initialize(Context context, String rootDir, LibLoader loader, MMKVLogLevel logLevel, MMKVHandler handler) {
        // disable process mode in release build
        // FIXME: Find a better way to getApplicationInfo() without using context.
        //  If any one knows how, you're welcome to make a contribution.
        if ((context.getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) == 0) {
            disableProcessModeChecker();
        } else {
            enableProcessModeChecker();
        }
        String cacheDir = context.getCacheDir().getAbsolutePath();

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

    private static String doInitialize(String rootDir, String cacheDir, LibLoader loader, MMKVLogLevel logLevel, boolean wantLogReDirecting) {
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
        MMKV.rootDir = rootDir;
        return MMKV.rootDir;
    }

    /**
     * @deprecated This method is deprecated due to failing to automatically disable checkProcessMode() without Context.
     * Use the {@link #initialize(Context, String)} method instead.
     */
    @Deprecated
    public static String initialize(String rootDir) {
        MMKVLogLevel logLevel = BuildConfig.DEBUG ? MMKVLogLevel.LevelDebug : MMKVLogLevel.LevelInfo;
        return doInitialize(rootDir, rootDir + "/.tmp", null, logLevel, false);
    }

    /**
     * @deprecated This method is deprecated due to failing to automatically disable checkProcessMode() without Context.
     * Use the {@link #initialize(Context, String, MMKVLogLevel)} method instead.
     */
    @Deprecated
    public static String initialize(String rootDir, MMKVLogLevel logLevel) {
        return doInitialize(rootDir, rootDir + "/.tmp", null, logLevel, false);
    }

    /**
     * @deprecated This method is deprecated due to failing to automatically disable checkProcessMode() without Context.
     * Use the {@link #initialize(Context, String, LibLoader)} method instead.
     */
    @Deprecated
    public static String initialize(String rootDir, LibLoader loader) {
        MMKVLogLevel logLevel = BuildConfig.DEBUG ? MMKVLogLevel.LevelDebug : MMKVLogLevel.LevelInfo;
        return doInitialize(rootDir, rootDir + "/.tmp", loader, logLevel, false);
    }

    /**
     * @deprecated This method is deprecated due to failing to automatically disable checkProcessMode() without Context.
     * Use the {@link #initialize(Context, String, LibLoader, MMKVLogLevel)} method instead.
     */
    @Deprecated
    public static String initialize(String rootDir, LibLoader loader, MMKVLogLevel logLevel) {
        return doInitialize(rootDir, rootDir + "/.tmp", loader, logLevel, false);
    }

    static private String rootDir = null;

    /**
     * @return The root folder of MMKV, defaults to $(FilesDir)/mmkv.
     */
    public static String getRootDir() {
        return rootDir;
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

    /**
     * Set the log level of MMKV.
     *
     * @param level Defaults to {@link MMKVLogLevel#LevelInfo}.
     */
    public static void setLogLevel(MMKVLogLevel level) {
        int realLevel = logLevel2Int(level);
        setLogLevel(realLevel);
    }

    /**
     * Notify MMKV that App is about to exit. It's totally fine not calling it at all.
     */
    public static native void onExit();

    /**
     * Single-process mode. The default mode on an MMKV instance.
     */
    static public final int SINGLE_PROCESS_MODE = 1 << 0;

    /**
     * Multi-process mode.
     * To enable multi-process accessing of an MMKV instance, you must set this mode whenever you getting that instance.
     */
    static public final int MULTI_PROCESS_MODE = 1 << 1;

    // in case someone mistakenly pass Context.MODE_MULTI_PROCESS
    static private final int CONTEXT_MODE_MULTI_PROCESS = 1 << 2;

    static private final int ASHMEM_MODE = 1 << 3;

    static private final int BACKUP_MODE = 1 << 4;

    /**
     * Create an MMKV instance with an unique ID (in single-process mode).
     *
     * @param mmapID The unique ID of the MMKV instance.
     * @throws RuntimeException if there's an runtime error.
     */
    public static MMKV mmkvWithID(String mmapID) throws RuntimeException {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        long handle = getMMKVWithID(mmapID, SINGLE_PROCESS_MODE, null, null, 0);
        return checkProcessMode(handle, mmapID, SINGLE_PROCESS_MODE);
    }

    /**
     * Create an MMKV instance in single-process or multi-process mode.
     *
     * @param mmapID The unique ID of the MMKV instance.
     * @param mode   The process mode of the MMKV instance, defaults to {@link #SINGLE_PROCESS_MODE}.
     * @throws RuntimeException if there's an runtime error.
     */
    public static MMKV mmkvWithID(String mmapID, int mode) throws RuntimeException {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        long handle = getMMKVWithID(mmapID, mode, null, null, 0);
        return checkProcessMode(handle, mmapID, mode);
    }

    /**
     * Create an MMKV instance in single-process or multi-process mode.
     *
     * @param mmapID The unique ID of the MMKV instance.
     * @param mode   The process mode of the MMKV instance, defaults to {@link #SINGLE_PROCESS_MODE}.
     * @param expectedCapacity The file size you expected when opening or creating file
     * @throws RuntimeException if there's an runtime error.
     */
    public static MMKV mmkvWithID(String mmapID, int mode, long expectedCapacity) throws RuntimeException {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        long handle = getMMKVWithID(mmapID, mode, null, null, expectedCapacity);
        return checkProcessMode(handle, mmapID, mode);
    }

    /**
     * Create an MMKV instance in customize process mode, with an encryption key.
     *
     * @param mmapID   The unique ID of the MMKV instance.
     * @param mode     The process mode of the MMKV instance, defaults to {@link #SINGLE_PROCESS_MODE}.
     * @param cryptKey The encryption key of the MMKV instance (no more than 16 bytes).
     * @throws RuntimeException if there's an runtime error.
     */
    public static MMKV mmkvWithID(String mmapID, int mode, @Nullable String cryptKey) throws RuntimeException {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        long handle = getMMKVWithID(mmapID, mode, cryptKey, null, 0);
        return checkProcessMode(handle, mmapID, mode);
    }

    /**
     * Create an MMKV instance in customize folder.
     *
     * @param mmapID   The unique ID of the MMKV instance.
     * @param rootPath The folder of the MMKV instance, defaults to $(FilesDir)/mmkv.
     * @throws RuntimeException if there's an runtime error.
     */
    public static MMKV mmkvWithID(String mmapID, String rootPath) throws RuntimeException {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        long handle = getMMKVWithID(mmapID, SINGLE_PROCESS_MODE, null, rootPath, 0);
        return checkProcessMode(handle, mmapID, SINGLE_PROCESS_MODE);
    }

    /**
     * Create an MMKV instance in customize folder.
     *
     * @param mmapID   The unique ID of the MMKV instance.
     * @param rootPath The folder of the MMKV instance, defaults to $(FilesDir)/mmkv.
     * @param expectedCapacity The file size you expected when opening or creating file
     * @throws RuntimeException if there's an runtime error.
     */
    public static MMKV mmkvWithID(String mmapID, String rootPath, long expectedCapacity) throws RuntimeException {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        long handle = getMMKVWithID(mmapID, SINGLE_PROCESS_MODE, null, rootPath, expectedCapacity);
        return checkProcessMode(handle, mmapID, SINGLE_PROCESS_MODE);
    }

    /**
     * Create an MMKV instance with customize settings all in one.
     *
     * @param mmapID   The unique ID of the MMKV instance.
     * @param mode     The process mode of the MMKV instance, defaults to {@link #SINGLE_PROCESS_MODE}.
     * @param cryptKey The encryption key of the MMKV instance (no more than 16 bytes).
     * @param rootPath The folder of the MMKV instance, defaults to $(FilesDir)/mmkv.
     * @param expectedCapacity The file size you expected when opening or creating file
     * @throws RuntimeException if there's an runtime error.
     */
    public static MMKV mmkvWithID(String mmapID, int mode, @Nullable String cryptKey, String rootPath, long expectedCapacity)
            throws RuntimeException {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        long handle = getMMKVWithID(mmapID, mode, cryptKey, rootPath, expectedCapacity);
        return checkProcessMode(handle, mmapID, mode);
    }

    /**
     * Create an MMKV instance with customize settings all in one.
     *
     * @param mmapID   The unique ID of the MMKV instance.
     * @param mode     The process mode of the MMKV instance, defaults to {@link #SINGLE_PROCESS_MODE}.
     * @param cryptKey The encryption key of the MMKV instance (no more than 16 bytes).
     * @param rootPath The folder of the MMKV instance, defaults to $(FilesDir)/mmkv.
     * @throws RuntimeException if there's an runtime error.
     */
    public static MMKV mmkvWithID(String mmapID, int mode, @Nullable String cryptKey, String rootPath)
            throws RuntimeException {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        long handle = getMMKVWithID(mmapID, mode, cryptKey, rootPath, 0);
        return checkProcessMode(handle, mmapID, mode);
    }

    /**
     * Get an backed-up MMKV instance with customize settings all in one.
     *
     * @param mmapID   The unique ID of the MMKV instance.
     * @param mode     The process mode of the MMKV instance, defaults to {@link #SINGLE_PROCESS_MODE}.
     * @param cryptKey The encryption key of the MMKV instance (no more than 16 bytes).
     * @param rootPath The backup folder of the MMKV instance.
     * @throws RuntimeException if there's an runtime error.
     */
    public static MMKV backedUpMMKVWithID(String mmapID, int mode, @Nullable String cryptKey, String rootPath)
            throws RuntimeException {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        mode |= BACKUP_MODE;
        long handle = getMMKVWithID(mmapID, mode, cryptKey, rootPath, 0);
        return checkProcessMode(handle, mmapID, mode);
    }

    /**
     * Create an MMKV instance base on Anonymous Shared Memory, aka not synced to any disk files.
     *
     * @param context  The context of Android App, usually from Application.
     * @param mmapID   The unique ID of the MMKV instance.
     * @param size     The maximum size of the underlying Anonymous Shared Memory.
     *                 Anonymous Shared Memory on Android can't grow dynamically, must set an appropriate size on creation.
     * @param mode     The process mode of the MMKV instance, defaults to {@link #SINGLE_PROCESS_MODE}.
     * @param cryptKey The encryption key of the MMKV instance (no more than 16 bytes).
     * @throws RuntimeException if there's an runtime error.
     */
    public static MMKV mmkvWithAshmemID(Context context, String mmapID, int size, int mode, @Nullable String cryptKey)
            throws RuntimeException {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        String processName = MMKVContentProvider.getProcessNameByPID(context, android.os.Process.myPid());
        if (processName == null || processName.length() == 0) {
            String message = "process name detect fail, try again later";
            simpleLog(MMKVLogLevel.LevelError, message);
            throw new IllegalStateException(message);
        }
        if (processName.contains(":")) {
            Uri uri = MMKVContentProvider.contentUri(context);
            if (uri == null) {
                String message = "MMKVContentProvider has invalid authority";
                simpleLog(MMKVLogLevel.LevelError, message);
                throw new IllegalStateException(message);
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
                                mmkv.mmapID() + " fd = " + mmkv.ashmemFD() + ", meta fd = " + mmkv.ashmemMetaFD());
                        return mmkv;
                    }
                }
            }
        }
        simpleLog(MMKVLogLevel.LevelInfo, "getting mmkv in main process");

        mode = mode | ASHMEM_MODE;
        long handle = getMMKVWithIDAndSize(mmapID, size, mode, cryptKey);
        if (handle != 0) {
            return new MMKV(handle);
        }
        throw new IllegalStateException("Fail to create an Ashmem MMKV instance [" + mmapID + "]");
    }

    /**
     * Create the default MMKV instance in single-process mode.
     *
     * @throws RuntimeException if there's an runtime error.
     */
    public static MMKV defaultMMKV() throws RuntimeException {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        long handle = getDefaultMMKV(SINGLE_PROCESS_MODE, null);
        return checkProcessMode(handle, "DefaultMMKV", SINGLE_PROCESS_MODE);
    }

    /**
     * Create the default MMKV instance in customize process mode, with an encryption key.
     *
     * @param mode     The process mode of the MMKV instance, defaults to {@link #SINGLE_PROCESS_MODE}.
     * @param cryptKey The encryption key of the MMKV instance (no more than 16 bytes).
     * @throws RuntimeException if there's an runtime error.
     */
    public static MMKV defaultMMKV(int mode, @Nullable String cryptKey) throws RuntimeException {
        if (rootDir == null) {
            throw new IllegalStateException("You should Call MMKV.initialize() first.");
        }

        long handle = getDefaultMMKV(mode, cryptKey);
        return checkProcessMode(handle, "DefaultMMKV", mode);
    }

    private static MMKV checkProcessMode(long handle, String mmapID, int mode) throws RuntimeException {
        if (handle == 0) {
            throw new RuntimeException("Fail to create an MMKV instance [" + mmapID + "] in JNI");
        }
        if (!isProcessModeCheckerEnabled) {
            return new MMKV(handle);
        }
        synchronized (checkedHandleSet) {
            if (!checkedHandleSet.contains(handle)) {
                if (!checkProcessMode(handle)) {
                    String message;
                    if (mode == SINGLE_PROCESS_MODE) {
                        message = "Opening a multi-process MMKV instance [" + mmapID + "] with SINGLE_PROCESS_MODE!";
                    } else {
                        message = "Opening an MMKV instance [" + mmapID + "] with MULTI_PROCESS_MODE, ";
                        message += "while it's already been opened with SINGLE_PROCESS_MODE by someone somewhere else!";
                    }
                    throw new IllegalArgumentException(message);
                }
                checkedHandleSet.add(handle);
            }
        }
        return new MMKV(handle);
    }

    // Enable checkProcessMode() when initializing an MMKV instance, it's automatically enabled on debug build.
    private static boolean isProcessModeCheckerEnabled = true;

    /**
     * Manually enable the process mode checker.
     * By default, it's automatically enabled in DEBUG build, and disabled in RELEASE build.
     * If it's enabled, MMKV will throw exceptions when an MMKV instance is created with mismatch process mode.
     */
    public static void enableProcessModeChecker() {
        synchronized (checkedHandleSet) {
            isProcessModeCheckerEnabled = true;
        }
        Log.i("MMKV", "Enable checkProcessMode()");
    }

    /**
     * Manually disable the process mode checker.
     * By default, it's automatically enabled in DEBUG build, and disabled in RELEASE build.
     * If it's enabled, MMKV will throw exceptions when an MMKV instance is created with mismatch process mode.
     */
    public static void disableProcessModeChecker() {
        synchronized (checkedHandleSet) {
            isProcessModeCheckerEnabled = false;
        }
        Log.i("MMKV", "Disable checkProcessMode()");
    }

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

    public boolean encode(String key, boolean value) {
        return encodeBool(nativeHandle, key, value);
    }

    /**
     * Set value with customize expiration in sections.
     *
     * @param expireDurationInSecond override the default duration, {@link #ExpireNever} (0) means never expire.
     */
    public boolean encode(String key, boolean value, int expireDurationInSecond) {
        return encodeBool_2(nativeHandle, key, value, expireDurationInSecond);
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

    /**
     * Set value with customize expiration in sections.
     *
     * @param expireDurationInSecond override the default duration, {@link #ExpireNever} (0) means never expire.
     */
    public boolean encode(String key, int value, int expireDurationInSecond) {
        return encodeInt_2(nativeHandle, key, value, expireDurationInSecond);
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

    /**
     * Set value with customize expiration in sections.
     *
     * @param expireDurationInSecond override the default duration, {@link #ExpireNever} (0) means never expire.
     */
    public boolean encode(String key, long value, int expireDurationInSecond) {
        return encodeLong_2(nativeHandle, key, value, expireDurationInSecond);
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

    /**
     * Set value with customize expiration in sections.
     *
     * @param expireDurationInSecond override the default duration, {@link #ExpireNever} (0) means never expire.
     */
    public boolean encode(String key, float value, int expireDurationInSecond) {
        return encodeFloat_2(nativeHandle, key, value, expireDurationInSecond);
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

    /**
     * Set value with customize expiration in sections.
     *
     * @param expireDurationInSecond override the default duration, {@link #ExpireNever} (0) means never expire.
     */
    public boolean encode(String key, double value, int expireDurationInSecond) {
        return encodeDouble_2(nativeHandle, key, value, expireDurationInSecond);
    }

    public double decodeDouble(String key) {
        return decodeDouble(nativeHandle, key, 0);
    }

    public double decodeDouble(String key, double defaultValue) {
        return decodeDouble(nativeHandle, key, defaultValue);
    }

    public boolean encode(String key, @Nullable String value) {
        return encodeString(nativeHandle, key, value);
    }

    /**
     * Set value with customize expiration in sections.
     *
     * @param expireDurationInSecond override the default duration, {@link #ExpireNever} (0) means never expire.
     */
    public boolean encode(String key, @Nullable String value, int expireDurationInSecond) {
        return encodeString_2(nativeHandle, key, value, expireDurationInSecond);
    }

    @Nullable
    public String decodeString(String key) {
        return decodeString(nativeHandle, key, null);
    }

    @Nullable
    public String decodeString(String key, @Nullable String defaultValue) {
        return decodeString(nativeHandle, key, defaultValue);
    }

    public boolean encode(String key, @Nullable Set<String> value) {
        return encodeSet(nativeHandle, key, (value == null) ? null : value.toArray(new String[0]));
    }

    /**
     * Set value with customize expiration in sections.
     *
     * @param expireDurationInSecond override the default duration, {@link #ExpireNever} (0) means never expire.
     */
    public boolean encode(String key, @Nullable Set<String> value, int expireDurationInSecond) {
        return encodeSet_2(nativeHandle, key, (value == null) ? null : value.toArray(new String[0]), expireDurationInSecond);
    }

    @Nullable
    public Set<String> decodeStringSet(String key) {
        return decodeStringSet(key, null);
    }

    @Nullable
    public Set<String> decodeStringSet(String key, @Nullable Set<String> defaultValue) {
        return decodeStringSet(key, defaultValue, HashSet.class);
    }

    @SuppressWarnings("unchecked")
    @Nullable
    public Set<String> decodeStringSet(String key, @Nullable Set<String> defaultValue, Class<? extends Set> cls) {
        String[] result = decodeStringSet(nativeHandle, key);
        if (result == null) {
            return defaultValue;
        }
        Set<String> a;
        try {
            a = cls.newInstance();
        } catch (IllegalAccessException e) {
            return defaultValue;
        } catch (InstantiationException e) {
            return defaultValue;
        }
        a.addAll(Arrays.asList(result));
        return a;
    }

    public boolean encode(String key, @Nullable byte[] value) {
        return encodeBytes(nativeHandle, key, value);
    }

    /**
     * Set value with customize expiration in sections.
     *
     * @param expireDurationInSecond override the default duration, {@link #ExpireNever} (0) means never expire.
     */
    public boolean encode(String key, @Nullable byte[] value, int expireDurationInSecond) {
        return encodeBytes_2(nativeHandle, key, value, expireDurationInSecond);
    }

    @Nullable
    public byte[] decodeBytes(String key) {
        return decodeBytes(key, null);
    }

    @Nullable
    public byte[] decodeBytes(String key, @Nullable byte[] defaultValue) {
        byte[] ret = decodeBytes(nativeHandle, key);
        return (ret != null) ? ret : defaultValue;
    }

    private static final HashMap<String, Parcelable.Creator<?>> mCreators = new HashMap<>();

    private byte[] getParcelableByte(@NonNull Parcelable value) {
        Parcel source = Parcel.obtain();
        value.writeToParcel(source, 0);
        byte[] bytes = source.marshall();
        source.recycle();
        return bytes;
    }

    public boolean encode(String key, @Nullable Parcelable value) {
        if (value == null) {
            return encodeBytes(nativeHandle, key, null);
        }
        byte[] bytes = getParcelableByte(value);
        return encodeBytes(nativeHandle, key, bytes);
    }

    /**
     * Set value with customize expiration in sections.
     *
     * @param expireDurationInSecond override the default duration, {@link #ExpireNever} (0) means never expire.
     */
    public boolean encode(String key, @Nullable Parcelable value, int expireDurationInSecond) {
        if (value == null) {
            return encodeBytes_2(nativeHandle, key, null, expireDurationInSecond);
        }
        byte[] bytes = getParcelableByte(value);
        return encodeBytes_2(nativeHandle, key, bytes, expireDurationInSecond);
    }

    @SuppressWarnings("unchecked")
    @Nullable
    public <T extends Parcelable> T decodeParcelable(String key, Class<T> tClass) {
        return decodeParcelable(key, tClass, null);
    }

    @SuppressWarnings("unchecked")
    @Nullable
    public <T extends Parcelable> T decodeParcelable(String key, Class<T> tClass, @Nullable T defaultValue) {
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

    /**
     * Get the actual size consumption of the key's value.
     * Note: might be a little bigger than value's length.
     *
     * @param key The key of the value.
     */
    public int getValueSize(String key) {
        return valueSize(nativeHandle, key, false);
    }

    /**
     * Get the actual size of the key's value. String's length or byte[]'s length, etc.
     *
     * @param key The key of the value.
     */
    public int getValueActualSize(String key) {
        return valueSize(nativeHandle, key, true);
    }

    /**
     * Check whether or not MMKV contains the key.
     *
     * @param key The key of the value.
     */
    public boolean containsKey(String key) {
        return containsKey(nativeHandle, key);
    }

    /**
     * @return All the keys.
     */
    @Nullable
    public String[] allKeys() {
        return allKeys(nativeHandle, false);
    }

    /**
     * @return All non-expired keys. Note that this call has costs.
     */
    @Nullable
    public String[] allNonExpireKeys() {
        return allKeys(nativeHandle, true);
    }

    /**
     * @return The total count of all the keys.
     */
    public long count() {
        return count(nativeHandle, false);
    }

    /**
     * @return The total count of all non-expired keys. Note that this call has costs.
     */
    public long countNonExpiredKeys() {
        return count(nativeHandle, true);
    }

    /**
     * Get the size of the underlying file. Align to the disk block size, typically 4K for an Android device.
     */
    public long totalSize() {
        return totalSize(nativeHandle);
    }

    /**
     * Get the actual used size of the MMKV instance.
     * This size might increase and decrease as MMKV doing insertion and full write back.
     */
    public long actualSize() {
        return actualSize(nativeHandle);
    }

    public void removeValueForKey(String key) {
        removeValueForKey(nativeHandle, key);
    }

    /**
     * Batch remove some keys from the MMKV instance.
     *
     * @param arrKeys The keys to be removed.
     */
    public native void removeValuesForKeys(String[] arrKeys);

    /**
     * Clear all the key-values inside the MMKV instance.
     * The data file will be trimmed down to `pageSize`, and some sync operations will be called
     * If you do not want to trim the file, use {@link #clearAllWithKeepingSpace()} instead for better performance
     */
    public native void clearAll();

    /**
     * Faster {@link #clearAll()} implementation
     * The file size is kept as previous for later use
     */
    public native void clearAllWithKeepingSpace();

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

    /**
     * Save all mmap memory to file synchronously.
     * You don't need to call this, really, I mean it.
     * Unless you worry about the device running out of battery.
     */
    public void sync() {
        sync(true);
    }

    /**
     * Save all mmap memory to file asynchronously.
     * No need to call this unless you worry about the device running out of battery.
     */
    public void async() {
        sync(false);
    }

    private native void sync(boolean sync);

    /**
     * Check whether the MMKV file is valid or not.
     * Note: Don't use this to check the existence of the instance, the result is undefined on nonexistent files.
     */
    public static boolean isFileValid(String mmapID) {
        return isFileValid(mmapID, null);
    }

    /**
     * Check whether the MMKV file is valid or not on customize folder.
     *
     * @param mmapID   The unique ID of the MMKV instance.
     * @param rootPath The folder of the MMKV instance, defaults to $(FilesDir)/mmkv.
     */
    public static native boolean isFileValid(String mmapID, @Nullable String rootPath);

    /**
     * Atomically migrate all key-values from an existent SharedPreferences to the MMKV instance.
     *
     * @param preferences The SharedPreferences to import from.
     * @return The total count of key-values imported.
     */
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

    public static final int ExpireNever = 0;
    public static final int ExpireInMinute = 60;
    public static final int ExpireInHour = 60 * 60;
    public static final int ExpireInDay = 24 * 60 * 60;
    public static final int ExpireInMonth = 30 * 24 * 60 * 60;
    public static final int ExpireInYear = 365 * 30 * 24 * 60 * 60;

    /**
     * Enable auto key expiration. This is a upgrade operation, the file format will change.
     * And the file won't be accessed correctly by older version (v1.2.16) of MMKV.
     * NOTICE: enableCompareBeforeSet will be invalid when Expiration is on
     * @param expireDurationInSecond the expire duration for all keys, {@link #ExpireNever} (0) means no default duration (aka each key will have it's own expire date)
     */
    public native boolean enableAutoKeyExpire(int expireDurationInSecond);

    /**
     * Disable auto key expiration. This is a downgrade operation.
     */
    public native boolean disableAutoKeyExpire();

    /**
     * Enable data compare before set, for better performance
     * If data for key seldom changes, use it
     * When encryption or expiration is on, compare-before-set will be invalid.
     * For encryption, compare operation must decrypt data which is time consuming
     * For expiration, compare is useless because in most cases the expiration time changes every time.
     */
    public void enableCompareBeforeSet() {
        if (isExpirationEnabled()) {
            Log.e("MMKV", "enableCompareBeforeSet is invalid when Expiration is on");
            if (BuildConfig.DEBUG) {
                throw new RuntimeException("enableCompareBeforeSet is invalid when Expiration is on");
            }
        }
        if (isEncryptionEnabled()) {
            Log.e("MMKV", "enableCompareBeforeSet is invalid when key encryption is on");
            if (BuildConfig.DEBUG) {
                throw new RuntimeException("enableCompareBeforeSet is invalid when Expiration is on");
            }
        }
        nativeEnableCompareBeforeSet();
    }

    @FastNative
    private native void nativeEnableCompareBeforeSet();

    /**
     * Disable data compare before set
     * disabled at default
     */
    public native void disableCompareBeforeSet();

    /**
     * Intentionally Not Supported. Because MMKV does type-eraser inside to get better performance.
     */
    @Override
    public Map<String, ?> getAll() {
        throw new java.lang.UnsupportedOperationException(
                "Intentionally Not Supported. Use allKeys() instead, getAll() not implement because type-erasure inside mmkv");
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

    public Editor putString(String key, @Nullable String value, int expireDurationInSecond) {
        encodeString_2(nativeHandle, key, value, expireDurationInSecond);
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

    public Editor putStringSet(String key, @Nullable Set<String> values, int expireDurationInSecond) {
        encode(key, values, expireDurationInSecond);
        return this;
    }

    public Editor putBytes(String key, @Nullable byte[] bytes) {
        encode(key, bytes);
        return this;
    }

    public Editor putBytes(String key, @Nullable byte[] bytes, int expireDurationInSecond) {
        encode(key, bytes, expireDurationInSecond);
        return this;
    }

    public byte[] getBytes(String key, @Nullable byte[] defValue) {
        return decodeBytes(key, defValue);
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

    public Editor putInt(String key, int value, int expireDurationInSecond) {
        encodeInt_2(nativeHandle, key, value, expireDurationInSecond);
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

    public Editor putLong(String key, long value, int expireDurationInSecond) {
        encodeLong_2(nativeHandle, key, value, expireDurationInSecond);
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

    public Editor putFloat(String key, float value, int expireDurationInSecond) {
        encodeFloat_2(nativeHandle, key, value, expireDurationInSecond);
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

    public Editor putBoolean(String key, boolean value, int expireDurationInSecond) {
        encodeBool_2(nativeHandle, key, value, expireDurationInSecond);
        return this;
    }

    @Override
    public Editor remove(String key) {
        removeValueForKey(key);
        return this;
    }

    /**
     * {@link #clearAll()}
     */
    @Override
    public Editor clear() {
        clearAll();
        return this;
    }

    /**
     * @deprecated This method is only for compatibility purpose. You should remove all the calls after migration to MMKV.
     * MMKV doesn't rely on commit() to save data to file.
     * If you really worry about losing battery and data corruption, call {@link #async()} or {@link #sync()} instead.
     */
    @Override
    @Deprecated
    public boolean commit() {
        sync(true);
        return true;
    }

    /**
     * @deprecated This method is only for compatibility purpose. You should remove all the calls after migration to MMKV.
     * MMKV doesn't rely on apply() to save data to file.
     * If you really worry about losing battery and data corruption, call {@link #async()} instead.
     */
    @Override
    @Deprecated
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

    /**
     * Intentionally Not Supported by MMKV. We believe it's better not for a storage framework to notify the change of data.
     * Check {@link #registerContentChangeNotify} for a potential replacement on inter-process scene.
     */
    @Override
    public void registerOnSharedPreferenceChangeListener(OnSharedPreferenceChangeListener listener) {
        throw new java.lang.UnsupportedOperationException("Intentionally Not implement in MMKV");
    }

    /**
     * Intentionally Not Supported by MMKV. We believe it's better not for a storage framework to notify the change of data.
     */
    @Override
    public void unregisterOnSharedPreferenceChangeListener(OnSharedPreferenceChangeListener listener) {
        throw new java.lang.UnsupportedOperationException("Intentionally Not implement in MMKV");
    }

    /**
     * Get an ashmem MMKV instance that has been initiated by another process.
     * Normally you should just call {@link #mmkvWithAshmemID(Context, String, int, int, String)} instead.
     *
     * @param mmapID   The unique ID of the MMKV instance.
     * @param fd       The file descriptor of the ashmem of the MMKV file, transferred from another process by binder.
     * @param metaFD   The file descriptor of the ashmem of the MMKV crc file, transferred from another process by binder.
     * @param cryptKey The encryption key of the MMKV instance (no more than 16 bytes).
     * @throws RuntimeException If any failure in JNI or runtime.
     */
    // Parcelable
    public static MMKV mmkvWithAshmemFD(String mmapID, int fd, int metaFD, String cryptKey) throws RuntimeException {
        long handle = getMMKVWithAshmemFD(mmapID, fd, metaFD, cryptKey);
        if (handle == 0) {
            throw new RuntimeException("Fail to create an ashmem MMKV instance [" + mmapID + "] in JNI");
        }
        return new MMKV(handle);
    }

    /**
     * @return The file descriptor of the ashmem of the MMKV file.
     */
    public native int ashmemFD();

    /**
     * @return The file descriptor of the ashmem of the MMKV crc file.
     */
    public native int ashmemMetaFD();

    /**
     * Create an native buffer, whose underlying memory can be directly transferred to another JNI method.
     * Avoiding unnecessary JNI boxing and unboxing.
     * An NativeBuffer must be manually {@link #destroyNativeBuffer} to avoid memory leak.
     *
     * @param size The size of the underlying memory.
     */
    @Nullable
    public static NativeBuffer createNativeBuffer(int size) {
        long pointer = createNB(size);
        if (pointer <= 0) {
            return null;
        }
        return new NativeBuffer(pointer, size);
    }

    /**
     * Destroy the native buffer. An NativeBuffer must be manually destroy to avoid memory leak.
     */
    public static void destroyNativeBuffer(NativeBuffer buffer) {
        destroyNB(buffer.pointer, buffer.size);
    }

    /**
     * Write the value of the key to the native buffer.
     *
     * @return The size written. Return -1 on any error.
     */
    public int writeValueToNativeBuffer(String key, NativeBuffer buffer) {
        return writeValueToNB(nativeHandle, key, buffer.pointer, buffer.size);
    }

    // callback handler
    private static MMKVHandler gCallbackHandler;
    private static boolean gWantLogReDirecting = false;

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

    private static void simpleLog(MMKVLogLevel level, String message) {
        StackTraceElement[] stacktrace = Thread.currentThread().getStackTrace();
        StackTraceElement e = stacktrace[stacktrace.length - 1];
        Integer i = logLevel2Index.get(level);
        int intLevel = (i == null) ? 0 : i;
        mmkvLogImp(intLevel, e.getFileName(), e.getLineNumber(), e.getMethodName(), message);
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

    private static native void setWantsContentChangeNotify(boolean needsNotify);

    /**
     * Check inter-process content change manually.
     */
    public native void checkContentChangedByOuterProcess();

    // jni
    private final long nativeHandle;

    private MMKV(long handle) {
        nativeHandle = handle;
    }

    private static native void jniInitialize(String rootDir, String cacheDir, int level, boolean wantLogReDirecting);

    private native static long
    getMMKVWithID(String mmapID, int mode, @Nullable String cryptKey, @Nullable String rootPath,
                  long expectedCapacity);

    private native static long getMMKVWithIDAndSize(String mmapID, int size, int mode, @Nullable String cryptKey);

    private native static long getDefaultMMKV(int mode, @Nullable String cryptKey);

    private native static long getMMKVWithAshmemFD(String mmapID, int fd, int metaFD, @Nullable String cryptKey);

    private native boolean encodeBool(long handle, String key, boolean value);

    private native boolean encodeBool_2(long handle, String key, boolean value, int expireDurationInSecond);

    private native boolean decodeBool(long handle, String key, boolean defaultValue);

    private native boolean encodeInt(long handle, String key, int value);

    private native boolean encodeInt_2(long handle, String key, int value, int expireDurationInSecond);

    private native int decodeInt(long handle, String key, int defaultValue);

    private native boolean encodeLong(long handle, String key, long value);

    private native boolean encodeLong_2(long handle, String key, long value, int expireDurationInSecond);

    private native long decodeLong(long handle, String key, long defaultValue);

    private native boolean encodeFloat(long handle, String key, float value);

    private native boolean encodeFloat_2(long handle, String key, float value, int expireDurationInSecond);

    private native float decodeFloat(long handle, String key, float defaultValue);

    private native boolean encodeDouble(long handle, String key, double value);

    private native boolean encodeDouble_2(long handle, String key, double value, int expireDurationInSecond);

    private native double decodeDouble(long handle, String key, double defaultValue);

    private native boolean encodeString(long handle, String key, @Nullable String value);

    private native boolean encodeString_2(long handle, String key, @Nullable String value, int expireDurationInSecond);

    @Nullable
    private native String decodeString(long handle, String key, @Nullable String defaultValue);

    private native boolean encodeSet(long handle, String key, @Nullable String[] value);

    private native boolean encodeSet_2(long handle, String key, @Nullable String[] value, int expireDurationInSecond);

    @Nullable
    private native String[] decodeStringSet(long handle, String key);

    private native boolean encodeBytes(long handle, String key, @Nullable byte[] value);

    private native boolean encodeBytes_2(long handle, String key, @Nullable byte[] value, int expireDurationInSecond);

    @Nullable
    private native byte[] decodeBytes(long handle, String key);

    private native boolean containsKey(long handle, String key);

    private native String[] allKeys(long handle, boolean filterExpire);

    private native long count(long handle, boolean filterExpire);

    private native long totalSize(long handle);

    private native long actualSize(long handle);

    private native void removeValueForKey(long handle, String key);

    private native int valueSize(long handle, String key, boolean actualSize);

    private static native void setLogLevel(int level);

    private static native void setCallbackHandler(boolean logReDirecting, boolean hasCallback);

    private static native long createNB(int size);

    private static native void destroyNB(long pointer, int size);

    private native int writeValueToNB(long handle, String key, long pointer, int size);

    private native boolean isCompareBeforeSetEnabled();

    @FastNative
    private native boolean isEncryptionEnabled();

    @FastNative
    private native boolean isExpirationEnabled();

    private static native boolean checkProcessMode(long handle);
}
