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

    Baseline(Context context, int loops) {
        m_context = context;
        m_loops = loops;

        m_arrStrings = new String[loops];
        m_arrKeys = new String[loops];
        m_arrIntKeys = new String[loops];
        Random r = new Random();
        for (int index = 0; index < loops; index++) {
            m_arrStrings[index] = "MMKV-" + r.nextInt();
            m_arrKeys[index] = "str_" + index;
            m_arrIntKeys[index] = "int_" + index;
        }
    }

    public void mmkvBaselineTest() {
        mmkvBatchWriteInt();
        mmkvBatchReadInt();
        mmkvBatchWriteString();
        mmkvBatchReadString();

        //mmkvBatchDeleteString();
        //MMKV mmkv = MMKV.mmkvWithID(MMKV_ID, MMKV.SINGLE_PROCESS_MODE, CryptKey);
        //mmkv.trim();
    }

    private void mmkvBatchWriteInt() {
        Random r = new Random();
        long startTime = System.currentTimeMillis();

        MMKV mmkv = MMKV.mmkvWithID(MMKV_ID, MMKV.SINGLE_PROCESS_MODE, CryptKey);
        for (int index = 0; index < m_loops; index++) {
            int tmp = r.nextInt();
            String key = m_arrIntKeys[index];
            mmkv.encode(key, tmp);
        }
        long endTime = System.currentTimeMillis();
        Log.i("MMKV", "MMKV write int: loop[" + m_loops + "]: " + (endTime - startTime) + " ms");
    }

    private void mmkvBatchReadInt() {
        long startTime = System.currentTimeMillis();

        MMKV mmkv = MMKV.mmkvWithID(MMKV_ID, MMKV.SINGLE_PROCESS_MODE, CryptKey);
        for (int index = 0; index < m_loops; index++) {
            String key = m_arrIntKeys[index];
            int tmp = mmkv.decodeInt(key);
        }
        long endTime = System.currentTimeMillis();
        Log.i("MMKV", "MMKV read int: loop[" + m_loops + "]: " + (endTime - startTime) + " ms");
    }

    private void mmkvBatchWriteString() {
        long startTime = System.currentTimeMillis();

        MMKV mmkv = MMKV.mmkvWithID(MMKV_ID, MMKV.SINGLE_PROCESS_MODE, CryptKey);
        for (int index = 0; index < m_loops; index++) {
            final String valueStr = m_arrStrings[index];
            final String strKey = m_arrKeys[index];
            mmkv.encode(strKey, valueStr);
        }
        long endTime = System.currentTimeMillis();
        Log.i("MMKV", "MMKV write String: loop[" + m_loops + "]: " + (endTime - startTime) + " ms");
    }

    private void mmkvBatchReadString() {
        long startTime = System.currentTimeMillis();

        MMKV mmkv = MMKV.mmkvWithID(MMKV_ID, MMKV.SINGLE_PROCESS_MODE, CryptKey);
        for (int index = 0; index < m_loops; index++) {
            String strKey = m_arrKeys[index];
            String tmpStr = mmkv.decodeString(strKey);
        }
        long endTime = System.currentTimeMillis();
        Log.i("MMKV", "MMKV read String: loop[" + m_loops + "]: " + (endTime - startTime) + " ms");
    }

    private void mmkvBatchDeleteString() {
        long startTime = System.currentTimeMillis();

        MMKV mmkv = MMKV.mmkvWithID(MMKV_ID, MMKV.SINGLE_PROCESS_MODE, CryptKey);
        for (int index = 0; index < m_loops; index++) {
            String strKey = m_arrKeys[index];
            mmkv.removeValueForKey(strKey);
        }
        long endTime = System.currentTimeMillis();
        Log.i("MMKV",
              "MMKV delete String: loop[" + m_loops + "]: " + (endTime - startTime) + " ms");
    }

    public void sharedPreferencesBaselineTest() {
        spBatchWriteInt();
        spBatchReadInt();
        spBatchWrieString();
        spBatchReadStrinfg();
    }

    private void spBatchWriteInt() {
        Random r = new Random();
        long startTime = System.currentTimeMillis();

        SharedPreferences preferences = m_context.getSharedPreferences(MMKV_ID, MODE_PRIVATE);
        SharedPreferences.Editor editor = preferences.edit();
        for (int index = 0; index < m_loops; index++) {
            int tmp = r.nextInt();
            String key = m_arrIntKeys[index];
            editor.putInt(key, tmp);
            //            editor.commit();
            editor.apply();
        }
        long endTime = System.currentTimeMillis();
        Log.i("MMKV", "SharedPreferences write int: loop[" + m_loops + "]: " + (endTime - startTime)
                          + " ms");
    }

    private void spBatchReadInt() {
        long startTime = System.currentTimeMillis();

        SharedPreferences preferences = m_context.getSharedPreferences(MMKV_ID, MODE_PRIVATE);
        for (int index = 0; index < m_loops; index++) {
            String key = m_arrIntKeys[index];
            int tmp = preferences.getInt(key, 0);
        }
        long endTime = System.currentTimeMillis();
        Log.i("MMKV", "SharedPreferences read int: loop[" + m_loops + "]: " + (endTime - startTime)
                          + " ms");
    }

    private void spBatchWrieString() {
        long startTime = System.currentTimeMillis();

        SharedPreferences preferences = m_context.getSharedPreferences(MMKV_ID, MODE_PRIVATE);
        SharedPreferences.Editor editor = preferences.edit();
        for (int index = 0; index < m_loops; index++) {
            final String str = m_arrStrings[index];
            final String key = m_arrKeys[index];
            editor.putString(key, str);
            //            editor.commit();
            editor.apply();
        }
        long endTime = System.currentTimeMillis();
        Log.i("MMKV", "SharedPreferences write String: loop[" + m_loops
                          + "]: " + (endTime - startTime) + " ms");
    }

    private void spBatchReadStrinfg() {
        long startTime = System.currentTimeMillis();

        SharedPreferences preferences = m_context.getSharedPreferences(MMKV_ID, MODE_PRIVATE);
        for (int index = 0; index < m_loops; index++) {
            final String key = m_arrKeys[index];
            final String tmp = preferences.getString(key, null);
        }
        long endTime = System.currentTimeMillis();
        Log.i("MMKV", "SharedPreferences read String: loop[" + m_loops
                          + "]: " + (endTime - startTime) + " ms");
    }

    public void sqliteBaselineTest() {
        sqliteWriteInt();
        sqliteReadInt();
        sqliteWriteString();
        sqliteReadString();
    }

    private void sqliteWriteInt() {
        Random r = new Random();
        long startTime = System.currentTimeMillis();

        SQLIteKV sqlIteKV = new SQLIteKV(m_context);
        sqlIteKV.beginTransaction();
        for (int index = 0; index < m_loops; index++) {
            int tmp = r.nextInt();
            String key = m_arrIntKeys[index];
            sqlIteKV.putInt(key, tmp);
        }
        sqlIteKV.endTransaction();
        long endTime = System.currentTimeMillis();
        Log.i("MMKV", "sqlite write int: loop[" + m_loops + "]: " + (endTime - startTime) + " ms");
    }

    private void sqliteReadInt() {
        long startTime = System.currentTimeMillis();

        SQLIteKV sqlIteKV = new SQLIteKV(m_context);
        sqlIteKV.beginTransaction();
        for (int index = 0; index < m_loops; index++) {
            String key = m_arrIntKeys[index];
            int tmp = sqlIteKV.getInt(key);
        }
        sqlIteKV.endTransaction();
        long endTime = System.currentTimeMillis();
        Log.i("MMKV", "sqlite read int: loop[" + m_loops + "]: " + (endTime - startTime) + " ms");
    }

    private void sqliteWriteString() {
        long startTime = System.currentTimeMillis();

        SQLIteKV sqlIteKV = new SQLIteKV(m_context);
        sqlIteKV.beginTransaction();
        for (int index = 0; index < m_loops; index++) {
            final String value = m_arrStrings[index];
            final String key = m_arrKeys[index];
            sqlIteKV.putString(key, value);
        }
        sqlIteKV.endTransaction();
        long endTime = System.currentTimeMillis();
        Log.i("MMKV",
              "sqlite write String: loop[" + m_loops + "]: " + (endTime - startTime) + " ms");
    }

    private void sqliteReadString() {
        long startTime = System.currentTimeMillis();

        SQLIteKV sqlIteKV = new SQLIteKV(m_context);
        sqlIteKV.beginTransaction();
        for (int index = 0; index < m_loops; index++) {
            final String key = m_arrKeys[index];
            final String tmp = sqlIteKV.getString(key);
        }
        sqlIteKV.endTransaction();
        long endTime = System.currentTimeMillis();
        Log.i("MMKV",
              "sqlite read String: loop[" + m_loops + "]: " + (endTime - startTime) + " ms");
    }
}
