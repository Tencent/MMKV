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

import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * A helper class for MMKV based on Anonymous Shared Memory. {@link MMKV#mmkvWithAshmemID}
 */
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

    private Bundle mmkvFromAshmemID(String ashmemID, int size, int mode, String cryptKey) throws RuntimeException {
        MMKV mmkv = MMKV.mmkvWithAshmemID(getContext(), ashmemID, size, mode, cryptKey);
        ParcelableMMKV parcelableMMKV = new ParcelableMMKV(mmkv);
        Log.i("MMKV", ashmemID + " fd = " + mmkv.ashmemFD() + ", meta fd = " + mmkv.ashmemMetaFD());
        Bundle result = new Bundle();
        result.putParcelable(MMKVContentProvider.KEY, parcelableMMKV);
        return result;
    }

    private static String queryAuthority(Context context) {
        try {
            ComponentName componentName = new ComponentName(context, MMKVContentProvider.class.getName());
            PackageManager mgr = context.getPackageManager();
            if (mgr != null) {
                ProviderInfo providerInfo = mgr.getProviderInfo(componentName, 0);
                if (providerInfo != null) {
                    return providerInfo.authority;
                }
            }
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

        return true;
    }

    protected static String getProcessNameByPID(Context context, int pid) {
        ActivityManager manager = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        if (manager != null) {
            // clang-format off
            for (ActivityManager.RunningAppProcessInfo processInfo
                    : manager.getRunningAppProcesses()) {
                if (processInfo.pid == pid) {
                    return processInfo.processName;
                }
            }
            // clang-format on
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
                try {
                    return mmkvFromAshmemID(mmapID, size, mode, cryptKey);
                } catch (Exception e) {
                    Log.e("MMKV", e.getMessage());
                    return null;
                }
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
    public int delete(@NonNull Uri uri, @Nullable String selection, @Nullable String[] selectionArgs) {
        throw new java.lang.UnsupportedOperationException("Not implement in MMKV");
    }

    @Nullable
    @Override
    public Uri insert(@NonNull Uri uri, @Nullable ContentValues values) {
        throw new java.lang.UnsupportedOperationException("Not implement in MMKV");
    }
}
