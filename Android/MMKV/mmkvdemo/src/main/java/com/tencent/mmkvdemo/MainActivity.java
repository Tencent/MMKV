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

import android.content.Intent;
import android.content.SharedPreferences;
import android.os.SystemClock;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.util.Arrays;
import java.util.HashSet;

import com.tencent.mmkv.MMKV;

import static com.tencent.mmkvdemo.BenchMarkBaseService.AshmemMMKV_ID;
import static com.tencent.mmkvdemo.BenchMarkBaseService.AshmemMMKV_Size;

public class MainActivity extends AppCompatActivity {
    static private final String KEY_1 = "Ashmem_Key_1";
    static private final String KEY_2 = "Ashmem_Key_2";
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        String rootDir = "mmkv root: " + MMKV.initialize(this);
        System.out.println(rootDir);

        TextView tv = (TextView) findViewById(R.id.sample_text);
        tv.setText(rootDir);

        final Button button = findViewById(R.id.button);
        button.setOnClickListener(new View.OnClickListener() {
            final Baseline baseline = new Baseline(getApplicationContext(), 1000);

            public void onClick(View v) {
                baseline.mmkvBaselineTest();
                baseline.sharedPreferencesBaselineTest();
                baseline.sqliteBaselineTest();

                //testInterProcessReKey();
            }
        });

        //prepareInterProcessAshmem();
        //prepareInterProcessAshmemByContentProvider(KEY_1);

        final Button button_read_int = findViewById(R.id.button_read_int);
        button_read_int.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                interProcessBaselineTest(BenchMarkBaseService.CMD_READ_INT);
            }
        });

        final Button button_write_int = findViewById(R.id.button_write_int);
        button_write_int.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                interProcessBaselineTest(BenchMarkBaseService.CMD_WRITE_INT);
            }
        });

        final Button button_read_string = findViewById(R.id.button_read_string);
        button_read_string.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                interProcessBaselineTest(BenchMarkBaseService.CMD_READ_STRING);
            }
        });

        final Button button_write_string = findViewById(R.id.button_write_string);
        button_write_string.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                interProcessBaselineTest(BenchMarkBaseService.CMD_WRITE_STRING);
            }
        });

        testMMKV("testAES", "Tencent MMKV", false);
        testAshmem();
        testReKey();

        KotlinUsecaseKt.kotlinFunctionalTest();

        //testInterProcessLogic();
        //estImportSharedPreferences();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        MMKV.onExit();
    }

    private void testInterProcessLogic() {
        MMKV mmkv = MMKV.mmkvWithID(MyService.MMKV_ID, MMKV.MULTI_PROCESS_MODE);
        mmkv.putInt(MyService.CMD_ID, 1024);
        Log.d("mmkv in main", "" + mmkv.decodeInt(MyService.CMD_ID));

        Intent intent = new Intent(this, MyService.class);
        intent.putExtra(BenchMarkBaseService.CMD_ID, MyService.CMD_REMOVE);
        startService(intent);

        SystemClock.sleep(1000 * 3);
        int value = mmkv.decodeInt(MyService.CMD_ID);
        Log.d("mmkv", "" + value);
    }

    private void testMMKV(String mmapID, String cryptKey, boolean decodeOnly) {
        //MMKV kv = MMKV.defaultMMKV();
        MMKV kv = MMKV.mmkvWithID(mmapID, MMKV.SINGLE_PROCESS_MODE, cryptKey);

        if (!decodeOnly) {
            kv.encode("bool", true);
        }
        System.out.println("bool: " + kv.decodeBool("bool"));

        if (!decodeOnly) {
            kv.encode("int", Integer.MIN_VALUE);
        }
        System.out.println("int: " + kv.decodeInt("int"));

        if (!decodeOnly) {
            kv.encode("long", Long.MAX_VALUE);
        }
        System.out.println("long: " + kv.decodeLong("long"));

        if (!decodeOnly) {
            kv.encode("float", -3.14f);
        }
        System.out.println("float: " + kv.decodeFloat("float"));

        if (!decodeOnly) {
            kv.encode("double", Double.MIN_VALUE);
        }
        System.out.println("double: " + kv.decodeDouble("double"));

        if (!decodeOnly) {
            kv.encode("string", "Hello from mmkv");
        }
        System.out.println("string: " + kv.decodeString("string"));

        if (!decodeOnly) {
            byte[] bytes = {'m', 'm', 'k', 'v'};
            kv.encode("bytes", bytes);
        }
        System.out.println("bytes: " + new String(kv.decodeBytes("bytes")));

        System.out.println("allKeys: " + Arrays.toString(kv.allKeys()));
        System.out.println("count = " + kv.count() + ", totalSize = " + kv.totalSize());
        System.out.println("containsKey[string]: " + kv.containsKey("string"));

        kv.removeValueForKey("bool");
        System.out.println("bool: " + kv.decodeBool("bool"));
        kv.removeValuesForKeys(new String[] {"int", "long"});
        //kv.clearAll();
        kv.clearMemoryCache();
        System.out.println("allKeys: " + Arrays.toString(kv.allKeys()));
        System.out.println("isFileValid[" + kv.mmapID() + "]: " + MMKV.isFileValid(kv.mmapID()));
    }

    private void testImportSharedPreferences() {
        SharedPreferences preferences = getSharedPreferences("imported", MODE_PRIVATE);
        SharedPreferences.Editor editor = preferences.edit();
        editor.putBoolean("bool", true);
        editor.putInt("int", Integer.MIN_VALUE);
        editor.putLong("long", Long.MAX_VALUE);
        editor.putFloat("float", -3.14f);
        editor.putString("string", "hello, imported");
        HashSet<String> set = new HashSet<String>();
        set.add("W");
        set.add("e");
        set.add("C");
        set.add("h");
        set.add("a");
        set.add("t");
        editor.putStringSet("string-set", set);
        editor.commit();

        MMKV kv = MMKV.mmkvWithID("imported");
        kv.importFromSharedPreferences(preferences);
        editor.clear().commit();

        System.out.println("allKeys: " + Arrays.toString(kv.allKeys()));

        System.out.println("bool: " + kv.getBoolean("bool", false));
        System.out.println("int: " + kv.getInt("int", 0));
        System.out.println("long: " + kv.getLong("long", 0));
        System.out.println("float: " + kv.getFloat("float", 0));
        System.out.println("double: " + kv.decodeDouble("double"));
        System.out.println("string: " + kv.getString("string", null));
        System.out.println("string-set: " + kv.getStringSet("string-set", null));
    }

    private void testReKey() {
        final String mmapID = "testAES_reKey";
        testMMKV(mmapID, null, false);

        MMKV kv = MMKV.mmkvWithID(mmapID, MMKV.SINGLE_PROCESS_MODE, null);
        kv.reKey("Key_seq_1");
        kv.clearMemoryCache();
        testMMKV(mmapID, "Key_seq_1", true);

        kv.reKey("Key_seq_2");
        kv.clearMemoryCache();
        testMMKV(mmapID, "Key_seq_2", true);

        kv.reKey(null);
        kv.clearMemoryCache();
        testMMKV(mmapID, null, true);
    }

    private void interProcessBaselineTest(String cmd) {
        Intent intent = new Intent(this, MyService.class);
        intent.putExtra(BenchMarkBaseService.CMD_ID, cmd);
        startService(intent);

        intent = new Intent(this, MyService_1.class);
        intent.putExtra(BenchMarkBaseService.CMD_ID, cmd);
        startService(intent);
    }

    private void testAshmem() {
        String cryptKey = "Tencent MMKV";
        MMKV kv = MMKV.mmkvWithAshmemID(this, "testAshmem", MMKV.pageSize(),
                                        MMKV.SINGLE_PROCESS_MODE, cryptKey);

        kv.encode("bool", true);
        System.out.println("bool: " + kv.decodeBool("bool"));

        kv.encode("int", Integer.MIN_VALUE);
        System.out.println("int: " + kv.decodeInt("int"));

        kv.encode("long", Long.MAX_VALUE);
        System.out.println("long: " + kv.decodeLong("long"));

        kv.encode("float", -3.14f);
        System.out.println("float: " + kv.decodeFloat("float"));

        kv.encode("double", Double.MIN_VALUE);
        System.out.println("double: " + kv.decodeDouble("double"));

        kv.encode("string", "Hello from mmkv");
        System.out.println("string: " + kv.decodeString("string"));

        byte[] bytes = {'m', 'm', 'k', 'v'};
        kv.encode("bytes", bytes);
        System.out.println("bytes: " + new String(kv.decodeBytes("bytes")));

        System.out.println("allKeys: " + Arrays.toString(kv.allKeys()));
        System.out.println("count = " + kv.count() + ", totalSize = " + kv.totalSize());
        System.out.println("containsKey[string]: " + kv.containsKey("string"));

        kv.removeValueForKey("bool");
        System.out.println("bool: " + kv.decodeBool("bool"));
        kv.removeValuesForKeys(new String[] {"int", "long"});
        //kv.clearAll();
        kv.clearMemoryCache();
        System.out.println("allKeys: " + Arrays.toString(kv.allKeys()));
        System.out.println("isFileValid[" + kv.mmapID() + "]: " + MMKV.isFileValid(kv.mmapID()));
    }

    private void prepareInterProcessAshmem() {
        Intent intent = new Intent(this, MyService_1.class);
        intent.putExtra(BenchMarkBaseService.CMD_ID, MyService_1.CMD_PREPARE_ASHMEM);
        startService(intent);
    }

    private void prepareInterProcessAshmemByContentProvider(String cryptKey) {
        // first of all, init ashmem mmkv in main process
        MMKV.mmkvWithAshmemID(this, AshmemMMKV_ID, AshmemMMKV_Size, MMKV.MULTI_PROCESS_MODE,
                              cryptKey);

        // then other process can get by ContentProvider
        Intent intent = new Intent(this, MyService.class);
        intent.putExtra(BenchMarkBaseService.CMD_ID, BenchMarkBaseService.CMD_PREPARE_ASHMEM_BY_CP);
        startService(intent);

        intent = new Intent(this, MyService_1.class);
        intent.putExtra(BenchMarkBaseService.CMD_ID, BenchMarkBaseService.CMD_PREPARE_ASHMEM_BY_CP);
        startService(intent);
    }

    private void testInterProcessReKey() {
        MMKV mmkv = MMKV.mmkvWithAshmemID(this, AshmemMMKV_ID, AshmemMMKV_Size,
                                          MMKV.MULTI_PROCESS_MODE, KEY_1);
        mmkv.reKey(KEY_2);

        prepareInterProcessAshmemByContentProvider(KEY_2);
    }
}
