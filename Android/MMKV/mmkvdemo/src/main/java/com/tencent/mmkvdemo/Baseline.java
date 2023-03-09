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

import static android.content.Context.MODE_PRIVATE;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;
import com.tencent.mmkv.MMKV;
import java.math.RoundingMode;
import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;
import java.util.Locale;
import java.util.Random;

public final class Baseline {
    private String[] m_arrStrings;
    private String[] m_arrKeys;
    private String[] m_arrIntKeys;
    private int m_loops = 1000;
    private Context m_context;
    private static final String MMKV_ID = "baseline3";
    private static final String CryptKey = null;
    //private static final String CryptKey = "baseline_key3";
    private static final String TAG = "MMKV";
    private DecimalFormat m_formatter;

    Baseline(Context context, int loops) {
        m_context = context;
        m_loops = loops;

        m_arrStrings = new String[loops];
        m_arrKeys = new String[loops];
        m_arrIntKeys = new String[loops];
        Random r = new Random();
        final String filename = "mmkv/Android/MMKV/mmkvdemo/src/main/java/com/tencent/mmkvdemo/Baseline.java_";
        for (int index = 0; index < loops; index++) {
            //String str = "[MMKV] [Info]<MemoryFile_OSX.cpp:36>: protection on [/var/mobile/Containers/Data/Application/B93F2BD3-E0DB-49B3-9BB0-C662E2FC11D9/Documents/mmkv/cips_commoncache] is NSFileProtectionCompleteUntilFirstUserAuthentication_";
            //m_arrStrings[index] = str + r.nextInt();
            m_arrStrings[index] = filename + r.nextInt();
            m_arrKeys[index] = "str_" + index;
            m_arrIntKeys[index] = "int_" + index;
        }
        m_formatter = new DecimalFormat("0.0", DecimalFormatSymbols.getInstance(Locale.ENGLISH));
        m_formatter.setRoundingMode(RoundingMode.DOWN);
    }

    public void mmkvBaselineTest() {
        mmkvBatchWriteInt();
        mmkvBatchReadInt();
        mmkvBatchWriteString();
        mmkvBatchReadString();

        //mmkvBatchDeleteString();
        MMKV mmkv = mmkvForTest();
        //mmkv.trim();
        mmkv.clearMemoryCache();
        mmkv.totalSize();
    }

    private MMKV mmkvForTest() {
        return MMKV.mmkvWithID(MMKV_ID, MMKV.SINGLE_PROCESS_MODE, CryptKey);
        //return MMKV.mmkvWithAshmemID(m_context, MMKV_ID, 65536, MMKV.SINGLE_PROCESS_MODE, CryptKey);
    }

    private void mmkvBatchWriteInt() {
        Random r = new Random();
        long startTime = System.nanoTime();

        MMKV mmkv = mmkvForTest();
        for (int index = 0; index < m_loops; index++) {
            int tmp = r.nextInt();
            String key = m_arrIntKeys[index];
            mmkv.encode(key, tmp);
        }
        double endTime = (System.nanoTime() - startTime) / 1000000.0;
        Log.i(TAG, "MMKV write int: loop[" + m_loops + "]: " + m_formatter.format(endTime) + " ms");
    }

    private void mmkvBatchReadInt() {
        long startTime = System.nanoTime();

        MMKV mmkv = mmkvForTest();
        for (int index = 0; index < m_loops; index++) {
            String key = m_arrIntKeys[index];
            int tmp = mmkv.decodeInt(key);
        }
        double endTime = (System.nanoTime() - startTime) / 1000000.0;
        Log.i(TAG, "MMKV read int: loop[" + m_loops + "]: " + m_formatter.format(endTime) + " ms");
    }

    private void mmkvBatchWriteString() {
        long startTime = System.nanoTime();

        MMKV mmkv = mmkvForTest();
        for (int index = 0; index < m_loops; index++) {
            final String valueStr = m_arrStrings[index];
            final String strKey = m_arrKeys[index];
            mmkv.encode(strKey, valueStr);
        }
        double endTime = (System.nanoTime() - startTime) / 1000000.0;
        Log.i(TAG, "MMKV write String: loop[" + m_loops + "]: " + m_formatter.format(endTime) + " ms");
    }

    private void mmkvBatchReadString() {
        long startTime = System.nanoTime();

        MMKV mmkv = mmkvForTest();
        for (int index = 0; index < m_loops; index++) {
            String strKey = m_arrKeys[index];
            String tmpStr = mmkv.decodeString(strKey);
        }
        double endTime = (System.nanoTime() - startTime) / 1000000.0;
        Log.i(TAG, "MMKV read String: loop[" + m_loops + "]: " + m_formatter.format(endTime) + " ms");
    }

    private void mmkvBatchDeleteString() {
        long startTime = System.nanoTime();

        MMKV mmkv = mmkvForTest();
        for (int index = 0; index < m_loops; index++) {
            String strKey = m_arrKeys[index];
            mmkv.removeValueForKey(strKey);
        }
        double endTime = (System.nanoTime() - startTime) / 1000000.0;
        Log.i(TAG, "MMKV delete String: loop[" + m_loops + "]: " + m_formatter.format(endTime) + " ms");
    }

    public void sharedPreferencesBaselineTest() {
        spBatchWriteInt();
        spBatchReadInt();
        spBatchWriteString();
        spBatchReadString();
    }

    private void spBatchWriteInt() {
        Random r = new Random();
        long startTime = System.nanoTime();

        SharedPreferences preferences = m_context.getSharedPreferences(MMKV_ID, MODE_PRIVATE);
        SharedPreferences.Editor editor = preferences.edit();
        for (int index = 0; index < m_loops; index++) {
            int tmp = r.nextInt();
            String key = m_arrIntKeys[index];
            editor.putInt(key, tmp);
            // editor.commit();
            editor.apply();
        }
        double endTime = (System.nanoTime() - startTime) / 1000000.0;
        Log.i(TAG, "SharedPreferences write int: loop[" + m_loops + "]: " + m_formatter.format(endTime) + " ms");
    }

    private void spBatchReadInt() {
        long startTime = System.nanoTime();

        SharedPreferences preferences = m_context.getSharedPreferences(MMKV_ID, MODE_PRIVATE);
        for (int index = 0; index < m_loops; index++) {
            String key = m_arrIntKeys[index];
            int tmp = preferences.getInt(key, 0);
        }
        double endTime = (System.nanoTime() - startTime) / 1000000.0;
        Log.i(TAG, "SharedPreferences read int: loop[" + m_loops + "]: " + m_formatter.format(endTime) + " ms");
    }

    private void spBatchWriteString() {
        long startTime = System.nanoTime();

        SharedPreferences preferences = m_context.getSharedPreferences(MMKV_ID, MODE_PRIVATE);
        SharedPreferences.Editor editor = preferences.edit();
        for (int index = 0; index < m_loops; index++) {
            final String str = m_arrStrings[index];
            final String key = m_arrKeys[index];
            editor.putString(key, str);
            // editor.commit();
            editor.apply();
        }
        double endTime = (System.nanoTime() - startTime) / 1000000.0;
        Log.i(TAG, "SharedPreferences write String: loop[" + m_loops + "]: " + m_formatter.format(endTime) + " ms");
    }

    private void spBatchReadString() {
        long startTime = System.nanoTime();

        SharedPreferences preferences = m_context.getSharedPreferences(MMKV_ID, MODE_PRIVATE);
        for (int index = 0; index < m_loops; index++) {
            final String key = m_arrKeys[index];
            final String tmp = preferences.getString(key, null);
        }
        double endTime = (System.nanoTime() - startTime) / 1000000.0;
        Log.i(TAG, "SharedPreferences read String: loop[" + m_loops + "]: " + m_formatter.format(endTime) + " ms");
    }

    public void sqliteBaselineTest(boolean useTransaction) {
        sqliteWriteInt(useTransaction);
        sqliteReadInt(useTransaction);
        sqliteWriteString(useTransaction);
        sqliteReadString(useTransaction);
    }

    private void sqliteWriteInt(boolean useTransaction) {
        Random r = new Random();
        long startTime = System.nanoTime();

        SQLIteKV sqlIteKV = new SQLIteKV(m_context);
        if (useTransaction) {
            sqlIteKV.beginTransaction();
        }
        for (int index = 0; index < m_loops; index++) {
            int tmp = r.nextInt();
            String key = m_arrIntKeys[index];
            sqlIteKV.putInt(key, tmp);
        }
        if (useTransaction) {
            sqlIteKV.endTransaction();
        }
        double endTime = (System.nanoTime() - startTime) / 1000000.0;
        final String msg = useTransaction ? "sqlite transaction" : "sqlite";
        Log.i(TAG, msg + " write int: loop[" + m_loops + "]: " + m_formatter.format(endTime) + " ms");
    }

    private void sqliteReadInt(boolean useTransaction) {
        long startTime = System.nanoTime();

        SQLIteKV sqlIteKV = new SQLIteKV(m_context);
        if (useTransaction) {
            sqlIteKV.beginTransaction();
        }
        for (int index = 0; index < m_loops; index++) {
            String key = m_arrIntKeys[index];
            int tmp = sqlIteKV.getInt(key);
        }
        if (useTransaction) {
            sqlIteKV.endTransaction();
        }
        double endTime = (System.nanoTime() - startTime) / 1000000.0;
        final String msg = useTransaction ? "sqlite transaction" : "sqlite";
        Log.i(TAG, msg + " read int: loop[" + m_loops + "]: " + m_formatter.format(endTime) + " ms");
    }

    private void sqliteWriteString(boolean useTransaction) {
        long startTime = System.nanoTime();

        SQLIteKV sqlIteKV = new SQLIteKV(m_context);
        if (useTransaction) {
            sqlIteKV.beginTransaction();
        }
        for (int index = 0; index < m_loops; index++) {
            final String value = m_arrStrings[index];
            final String key = m_arrKeys[index];
            sqlIteKV.putString(key, value);
        }
        if (useTransaction) {
            sqlIteKV.endTransaction();
        }
        double endTime = (System.nanoTime() - startTime) / 1000000.0;
        final String msg = useTransaction ? "sqlite transaction" : "sqlite";
        Log.i(TAG, msg + " write String: loop[" + m_loops + "]: " + m_formatter.format(endTime) + " ms");
    }

    private void sqliteReadString(boolean useTransaction) {
        long startTime = System.nanoTime();

        SQLIteKV sqlIteKV = new SQLIteKV(m_context);
        if (useTransaction) {
            sqlIteKV.beginTransaction();
        }
        for (int index = 0; index < m_loops; index++) {
            final String key = m_arrKeys[index];
            final String tmp = sqlIteKV.getString(key);
        }
        if (useTransaction) {
            sqlIteKV.endTransaction();
        }
        double endTime = (System.nanoTime() - startTime) / 1000000.0;
        final String msg = useTransaction ? "sqlite transaction" : "sqlite";
        Log.i(TAG, msg + " read String: loop[" + m_loops + "]: " + m_formatter.format(endTime) + " ms");
    }
}
