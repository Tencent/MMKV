package com.tencent.mmkv;

import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.pm.ProviderInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

public class MMKVContentProvider extends ContentProvider {

    static protected final String KEY = "KEY";
    static protected final String KEY_SIZE = "KEY_SIZE";
    static protected final String KEY_MODE = "KEY_MODE";
    static protected final String KEY_CRYPT = "KEY_CRYPT";
    static protected final String FUNCTION_NAME = "mmkvFromAshmemID";

    static private Uri gUri;
    @Nullable
    static protected Uri contentUri(Context context) {
        if (MMKVContentProvider.gUri != null) {
            return MMKVContentProvider.gUri;
        }
        if (context == null) {
            return null;
        }
        String authority = queryAuthority(context);
        if (authority == null) {
            return null;
        }
        MMKVContentProvider.gUri = Uri.parse(ContentResolver.SCHEME_CONTENT + "://" + authority);
        return MMKVContentProvider.gUri;
    }

    private Bundle mmkvFromAshmemID(String ashmemID, int size, int mode, String cryptKey) {
        MMKV mmkv = MMKV.mmkvWithAshmemID(getContext(), ashmemID, size, mode, cryptKey);
        if (mmkv != null) {
            ParcelableMMKV parcelableMMKV = new ParcelableMMKV(mmkv);
            System.out.println(ashmemID + " fd = " + mmkv.ashmemFD() + ", meta fd = " + mmkv.ashmemMetaFD());
            Bundle result = new Bundle();
            result.putParcelable(MMKVContentProvider.KEY, parcelableMMKV);
            return result;
        }
        return null;
    }

    private static String queryAuthority(Context context) {
        try {
            ComponentName componentName =
                new ComponentName(context, MMKVContentProvider.class.getName());
            ProviderInfo providerInfo =
                context.getPackageManager().getProviderInfo(componentName, 0);
            return providerInfo.authority;
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    @Override
    public boolean onCreate() {
        Context context = getContext();
        if (context == null) {
            return false;
        }
        String authority = queryAuthority(context);
        if (authority == null) {
            return false;
        }

        if (MMKVContentProvider.gUri == null) {
            MMKVContentProvider.gUri = Uri.parse(ContentResolver.SCHEME_CONTENT + "://" + authority);
        }

        return true;
    }

    protected static String getProcessNameByPID(Context context, int pid) {
        ActivityManager manager =
            (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        if (manager != null) {
            for (ActivityManager.RunningAppProcessInfo processInfo : manager.getRunningAppProcesses()) {
                if (processInfo.pid == pid) {
                    return processInfo.processName;
                }
            }
        }
        return "";
    }

    @Nullable
    @Override
    public Bundle call(@NonNull String method, @Nullable String mmapID, @Nullable Bundle extras) {
        if (method.equals(MMKVContentProvider.FUNCTION_NAME)) {
            if (extras != null) {
                int size = extras.getInt(MMKVContentProvider.KEY_SIZE);
                int mode = extras.getInt(MMKVContentProvider.KEY_MODE);
                String cryptKey = extras.getString(MMKVContentProvider.KEY_CRYPT);
                return mmkvFromAshmemID(mmapID, size, mode, cryptKey);
            }
        }
        return null;
    }

    @Nullable
    @Override
    public String getType(@NonNull Uri uri) {
        return null;
    }

    @Nullable
    @Override
    public Cursor query(@NonNull Uri uri,
                        @Nullable String[] projection,
                        @Nullable String selection,
                        @Nullable String[] selectionArgs,
                        @Nullable String sortOrder) {
        throw new java.lang.UnsupportedOperationException("Not implement in MMKV");
    }

    @Override
    public int update(@NonNull Uri uri,
                      @Nullable ContentValues values,
                      @Nullable String selection,
                      @Nullable String[] selectionArgs) {
        throw new java.lang.UnsupportedOperationException("Not implement in MMKV");
    }

    @Override
    public int
    delete(@NonNull Uri uri, @Nullable String selection, @Nullable String[] selectionArgs) {
        throw new java.lang.UnsupportedOperationException("Not implement in MMKV");
    }

    @Nullable
    @Override
    public Uri insert(@NonNull Uri uri, @Nullable ContentValues values) {
        throw new java.lang.UnsupportedOperationException("Not implement in MMKV");
    }
}
