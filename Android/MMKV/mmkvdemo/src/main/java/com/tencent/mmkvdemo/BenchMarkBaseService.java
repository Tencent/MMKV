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

package com.tencent.mmkvdemo;

import android.app.Service;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.IBinder;
import android.util.Log;
import androidx.annotation.Nullable;
import com.tencent.mmkv.MMKV;
import com.tencent.mmkv.ParcelableMMKV;
import java.util.Random;

public abstract class BenchMarkBaseService extends Service {
    public static final String CMD_ID = "cmd_id";
    public static final String CMD_READ_INT = "cmd_read_int";
    public static final String CMD_WRITE_INT = "cmd_write_int";
    public static final String CMD_READ_STRING = "cmd_read_string";
    public static final String CMD_WRITE_STRING = "cmd_write_string";
    public static final String CMD_PREPARE_ASHMEM_BY_CP = "cmd_prepare_ashmem_by_ContentProvider";
    public static final String CMD_PREPARE_ASHMEM_KEY = "cmd_prepare_ashmem_key";

    // 1M, ashmem cannot change size after opened
    public static final int AshmemMMKV_Size = 1024 * 1024;
    public static final String AshmemMMKV_ID = "testAshmemMMKVByCP";

    private String[] m_arrStrings;
    private String[] m_arrKeys;
    private String[] m_arrIntKeys;

    private static final int m_loops = 1000;
    public static final String MMKV_ID = "benchmark_interprocess";
    //public static final String MMKV_ID = "benchmark_interprocess_crypt1";
    private static final String SP_ID = "benchmark_interprocess_sp";
    public static final String CryptKey = null;
    //public static final String CryptKey = "Tencent MMKV";
    private static final boolean SQLite_Use_Transaction = false;
    private static final String TAG = "MMKV";

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "onCreate BenchMarkBaseService");

        MMKV.initialize(this);
        MMKV.unregisterHandler();
        MMKV.unregisterContentChangeNotify();
        {
            long startTime = System.currentTimeMillis();

            MMKV mmkv = MMKV.mmkvWithID(MMKV_ID, MMKV.MULTI_PROCESS_MODE, CryptKey);

            long endTime = System.currentTimeMillis();
            Log.i(TAG, "load [" + MMKV_ID + "]: " + (endTime - startTime) + " ms");
        }
        m_arrStrings = new String[m_loops];
        m_arrKeys = new String[m_loops];
        m_arrIntKeys = new String[m_loops];
        Random r = new Random();
        final String filename =
            "mmkv/Android/MMKV/mmkvdemo/src/main/java/com/tencent/mmkvdemo/BenchMarkBaseService.java_";
        for (int index = 0; index < m_loops; index++) {
            //String str = "[MMKV] [Info]<MemoryFile_OSX.cpp:36>: protection on [/var/mobile/Containers/Data/Application/B93F2BD3-E0DB-49B3-9BB0-C662E2FC11D9/Documents/mmkv/cips_commoncache] is NSFileProtectionCompleteUntilFirstUserAuthentication_";
            //m_arrStrings[index] = str + r.nextInt();
            m_arrStrings[index] = filename + r.nextInt();
            m_arrKeys[index] = "testStr_" + index;
            m_arrIntKeys[index] = "int_" + index;
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "onDestroy BenchMarkBaseService");

        MMKV.onExit();
    }

    protected void batchWriteInt(String caller) {
        mmkvBatchWriteInt(caller);
        sqliteWriteInt(caller, SQLite_Use_Transaction);
        spBatchWriteInt(caller);
    }

    private void mmkvBatchWriteInt(String caller) {
        Random r = new Random();
        Log.i(TAG, caller + " mmkv write int: loop[" + m_loops + "] star.");
        long startTime = System.currentTimeMillis();

        MMKV mmkv = GetMMKV();
        for (int index = 0; index < m_loops; index++) {
            int tmp = r.nextInt();
            String key = m_arrIntKeys[index];
            mmkv.encode(key, tmp);
        }
        long endTime = System.currentTimeMillis();
        Log.i(TAG, caller + " mmkv write int: loop[" + m_loops + "]: " + (endTime - startTime) + " ms");
    }

    private void sqliteWriteInt(String caller, boolean useTransaction) {
        Random r = new Random();
        long startTime = System.currentTimeMillis();

        SQLiteKV sqliteKV = new SQLiteKV(this);
        if (useTransaction) {
            sqliteKV.beginTransaction();
        }
        for (int index = 0; index < m_loops; index++) {
            int tmp = r.nextInt();
            String key = m_arrIntKeys[index];
            sqliteKV.putInt(key, tmp);
        }
        if (useTransaction) {
            sqliteKV.endTransaction();
        }
        long endTime = System.currentTimeMillis();
        final String msg = useTransaction ? " sqlite transaction " : " sqlite ";
        Log.i(TAG, caller + msg + "write int: loop[" + m_loops + "]: " + (endTime - startTime) + " ms");
    }

    private void spBatchWriteInt(String caller) {
        Random r = new Random();
        long startTime = System.currentTimeMillis();

        SharedPreferences preferences = MultiProcessSharedPreferences.getSharedPreferences(this, SP_ID, MODE_PRIVATE);
        SharedPreferences.Editor editor = preferences.edit();
        for (int index = 0; index < m_loops; index++) {
            int tmp = r.nextInt();
            String key = m_arrIntKeys[index];
            editor.putInt(key, tmp);
            //editor.commit();
            editor.apply();
        }
        long endTime = System.currentTimeMillis();
        Log.i(TAG, caller + " MultiProcessSharedPreferences write int: loop[" + m_loops + "]: " + (endTime - startTime)
                       + " ms");
    }

    protected void batchReadInt(String caller) {
        mmkvBatchReadInt(caller);
        sqliteReadInt(caller, SQLite_Use_Transaction);
        spBatchReadInt(caller);
    }

    private void mmkvBatchReadInt(String caller) {
        Log.i(TAG, caller + " mmkv read int: loop[" + m_loops + "] star.");
        long startTime = System.currentTimeMillis();

        MMKV mmkv = GetMMKV();
        for (int index = 0; index < m_loops; index++) {
            String key = m_arrIntKeys[index];
            int tmp = mmkv.decodeInt(key);
        }
        long endTime = System.currentTimeMillis();
        Log.i(TAG, caller + " mmkv read int: loop[" + m_loops + "]: " + (endTime - startTime) + " ms");
    }

    private void sqliteReadInt(String caller, boolean useTransaction) {
        long startTime = System.currentTimeMillis();

        SQLiteKV sqliteKV = new SQLiteKV(this);
        if (useTransaction) {
            sqliteKV.beginTransaction();
        }
        for (int index = 0; index < m_loops; index++) {
            String key = m_arrIntKeys[index];
            int tmp = sqliteKV.getInt(key);
        }
        if (useTransaction) {
            sqliteKV.endTransaction();
        }
        long endTime = System.currentTimeMillis();
        final String msg = useTransaction ? " sqlite transaction " : " sqlite ";
        Log.i(TAG, caller + msg + "read int: loop[" + m_loops + "]: " + (endTime - startTime) + " ms");
    }

    private void spBatchReadInt(String caller) {
        long startTime = System.currentTimeMillis();

        SharedPreferences preferences = MultiProcessSharedPreferences.getSharedPreferences(this, SP_ID, MODE_PRIVATE);
        for (int index = 0; index < m_loops; index++) {
            String key = m_arrIntKeys[index];
            int tmp = preferences.getInt(key, 0);
        }
        long endTime = System.currentTimeMillis();
        Log.i(TAG, caller + " MultiProcessSharedPreferences read int: loop[" + m_loops + "]: " + (endTime - startTime)
                       + " ms");
    }

    protected void batchWriteString(String caller) {
        mmkvBatchWriteString(caller);
        sqliteWriteString(caller, SQLite_Use_Transaction);
        spBatchWrieString(caller);
    }

    private void mmkvBatchWriteString(String caller) {
        Log.i(TAG, caller + " mmkv write String: loop[" + m_loops + "] star.");
        long startTime = System.currentTimeMillis();

        MMKV mmkv = GetMMKV();
        for (int index = 0; index < m_loops; index++) {
            final String valueStr = m_arrStrings[index];
            final String strKey = m_arrKeys[index];
            mmkv.encode(strKey, valueStr);
        }
        long endTime = System.currentTimeMillis();
        Log.i(TAG, caller + " mmkv write String: loop[" + m_loops + "]: " + (endTime - startTime) + " ms");
    }

    private void sqliteWriteString(String caller, boolean useTransaction) {
        long startTime = System.currentTimeMillis();

        SQLiteKV sqliteKV = new SQLiteKV(this);
        if (useTransaction) {
            sqliteKV.beginTransaction();
        }
        for (int index = 0; index < m_loops; index++) {
            final String value = m_arrStrings[index];
            final String key = m_arrKeys[index];
            sqliteKV.putString(key, value);
        }
        if (useTransaction) {
            sqliteKV.endTransaction();
        }
        long endTime = System.currentTimeMillis();
        final String msg = useTransaction ? " sqlite transaction " : " sqlite ";
        Log.i(TAG, caller + msg + "write String: loop[" + m_loops + "]: " + (endTime - startTime) + " ms");
    }

    private void spBatchWrieString(String caller) {
        long startTime = System.currentTimeMillis();

        SharedPreferences preferences = MultiProcessSharedPreferences.getSharedPreferences(this, SP_ID, MODE_PRIVATE);
        SharedPreferences.Editor editor = preferences.edit();
        for (int index = 0; index < m_loops; index++) {
            final String str = m_arrStrings[index];
            final String key = m_arrKeys[index];
            editor.putString(key, str);
            //editor.commit();
            editor.apply();
        }
        long endTime = System.currentTimeMillis();
        Log.i(TAG, caller + " MultiProcessSharedPreferences write String: loop[" + m_loops
                       + "]: " + (endTime - startTime) + " ms");
    }

    protected void batchReadString(String caller) {
        mmkvBatchReadString(caller);
        sqliteReadString(caller, SQLite_Use_Transaction);
        spBatchReadStrinfg(caller);
    }

    private void mmkvBatchReadString(String caller) {
        Log.i(TAG, caller + " mmkv read String: loop[" + m_loops + "] star.");
        long startTime = System.currentTimeMillis();

        MMKV mmkv = GetMMKV();
        for (int index = 0; index < m_loops; index++) {
            final String strKey = m_arrKeys[index];
            final String tmpStr = mmkv.decodeString(strKey);
        }
        long endTime = System.currentTimeMillis();
        Log.i(TAG, caller + " mmkv read String: loop[" + m_loops + "]: " + (endTime - startTime) + " ms");
    }

    private void sqliteReadString(String caller, boolean useTransaction) {
        long startTime = System.currentTimeMillis();

        SQLiteKV sqliteKV = new SQLiteKV(this);
        if (useTransaction) {
            sqliteKV.beginTransaction();
        }
        for (int index = 0; index < m_loops; index++) {
            final String key = m_arrKeys[index];
            final String tmp = sqliteKV.getString(key);
        }
        if (useTransaction) {
            sqliteKV.endTransaction();
        }
        long endTime = System.currentTimeMillis();
        final String msg = useTransaction ? " sqlite transaction " : " sqlite ";
        Log.i(TAG, caller + msg + "read String: loop[" + m_loops + "]: " + (endTime - startTime) + " ms");
    }

    private void spBatchReadStrinfg(String caller) {
        long startTime = System.currentTimeMillis();

        SharedPreferences preferences = MultiProcessSharedPreferences.getSharedPreferences(this, SP_ID, MODE_PRIVATE);
        for (int index = 0; index < m_loops; index++) {
            final String key = m_arrKeys[index];
            final String tmp = preferences.getString(key, null);
        }
        long endTime = System.currentTimeMillis();
        Log.i(TAG, caller + " MultiProcessSharedPreferences read String: loop[" + m_loops
                       + "]: " + (endTime - startTime) + " ms");
    }

    MMKV m_ashmemMMKV;

    protected MMKV GetMMKV() {
        if (m_ashmemMMKV != null) {
            return m_ashmemMMKV;
        } else {
            return MMKV.mmkvWithID(MMKV_ID, MMKV.MULTI_PROCESS_MODE, CryptKey);
        }
    }

    public class AshmemMMKVGetter extends IAshmemMMKV.Stub {

        private AshmemMMKVGetter() {
            // 1M, ashmem cannot change size after opened
            final String id = "tetAshmemMMKV";
            try {
                m_ashmemMMKV = MMKV.mmkvWithAshmemID(BenchMarkBaseService.this, id, AshmemMMKV_Size,
                        MMKV.MULTI_PROCESS_MODE, CryptKey);
                m_ashmemMMKV.encode("bool", true);
            } catch (Exception e) {
                Log.e("MMKV", e.getMessage());
            }
        }

        public ParcelableMMKV GetAshmemMMKV() {
            return new ParcelableMMKV(m_ashmemMMKV);
        }
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        Log.i(TAG, "onBind, intent=" + intent);
        return new AshmemMMKVGetter();
    }

    protected void prepareAshmemMMKVByCP() {
        // it's ok for other process not knowing cryptKey
        final String cryptKey = null;
        try {
            m_ashmemMMKV = MMKV.mmkvWithAshmemID(this, AshmemMMKV_ID, AshmemMMKV_Size, MMKV.MULTI_PROCESS_MODE, cryptKey);
        } catch (Exception e) {
            Log.e("MMKV", e.getMessage());
        }
    }
}
