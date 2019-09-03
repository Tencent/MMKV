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

import static com.tencent.mmkvdemo.BenchMarkBaseService.AshmemMMKV_ID;
import static com.tencent.mmkvdemo.BenchMarkBaseService.AshmemMMKV_Size;

import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;
import com.getkeepsafe.relinker.ReLinker;
import com.tencent.mmkv.MMKV;
import com.tencent.mmkv.MMKVContentChangeNotification;
import com.tencent.mmkv.MMKVHandler;
import com.tencent.mmkv.MMKVLogLevel;
import com.tencent.mmkv.MMKVRecoverStrategic;
import com.tencent.mmkv.NativeBuffer;
import java.util.Arrays;
import java.util.HashSet;
import java.util.LinkedHashSet;
import org.jetbrains.annotations.Nullable;

public class MainActivity
    extends AppCompatActivity implements MMKVHandler, MMKVContentChangeNotification {
    static private final String KEY_1 = "Ashmem_Key_1";
    static private final String KEY_2 = "Ashmem_Key_2";
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // set root dir
        //String rootDir = MMKV.initialize(this);
        String dir = getFilesDir().getAbsolutePath() + "/mmkv";
        String rootDir = MMKV.initialize(dir, new MMKV.LibLoader() {
            @Override
            public void loadLibrary(String libName) {
                ReLinker.loadLibrary(MainActivity.this, libName);
            }
        }, MMKVLogLevel.LevelInfo);
        Log.i("MMKV", "mmkv root: " + rootDir);

        // set log level
        MMKV.setLogLevel(MMKVLogLevel.LevelInfo);

        // you can turn off logging
        //MMKV.setLogLevel(MMKVLogLevel.LevelNone);

        MMKV.registerHandler(this);
        MMKV.registerContentChangeNotify(this);

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
                //testInterProcessLockPhase2();
            }
        });

        //testHolderForMultiThread();

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

        String otherDir = getFilesDir().getAbsolutePath() + "/mmkv_3";
        MMKV kv = testMMKV("test/AES", "Tencent MMKV", false, otherDir);
        if (kv != null) {
            kv.checkContentChangedByOuterProcess();
            kv.close();
        }

        testAshmem();
        testReKey();

        KotlinUsecaseKt.kotlinFunctionalTest();

        testInterProcessLogic();
        testImportSharedPreferences();
        //testInterProcessLockPhase1();
        //testCornerSize();
        //testFastRemoveCornerSize();
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

    @Nullable
    private MMKV testMMKV(String mmapID, String cryptKey, boolean decodeOnly, String relativePath) {
        //MMKV kv = MMKV.defaultMMKV();
        MMKV kv = MMKV.mmkvWithID(mmapID, MMKV.SINGLE_PROCESS_MODE, cryptKey, relativePath);
        if (kv == null) {
            return null;
        }

        if (!decodeOnly) {
            kv.encode("bool", true);
        }
        Log.i("MMKV", "bool: " + kv.decodeBool("bool"));

        if (!decodeOnly) {
            kv.encode("int", Integer.MIN_VALUE);
        }
        Log.i("MMKV", "int: " + kv.decodeInt("int"));

        if (!decodeOnly) {
            kv.encode("long", Long.MAX_VALUE);
        }
        Log.i("MMKV", "long: " + kv.decodeLong("long"));

        if (!decodeOnly) {
            kv.encode("float", -3.14f);
        }
        Log.i("MMKV", "float: " + kv.decodeFloat("float"));

        if (!decodeOnly) {
            kv.encode("double", Double.MIN_VALUE);
        }
        Log.i("MMKV", "double: " + kv.decodeDouble("double"));

        if (!decodeOnly) {
            kv.encode("string", "Hello from mmkv");
        }
        Log.i("MMKV", "string: " + kv.decodeString("string"));

        if (!decodeOnly) {
            byte[] bytes = {'m', 'm', 'k', 'v'};
            kv.encode("bytes", bytes);
        }
        byte[] bytes = kv.decodeBytes("bytes");
        Log.i("MMKV", "bytes: " + new String(bytes));
        Log.i("MMKV", "bytes length = " + bytes.length
                          + ", value size consumption = " + kv.getValueSize("bytes")
                          + ", value size = " + kv.getValueActualSize("bytes"));

        int sizeNeeded = kv.getValueActualSize("bytes");
        NativeBuffer nativeBuffer = MMKV.createNativeBuffer(sizeNeeded);
        if (nativeBuffer != null) {
            int size = kv.writeValueToNativeBuffer("bytes", nativeBuffer);
            Log.i("MMKV", "size Needed = " + sizeNeeded + " written size = " + size);
            MMKV.destroyNativeBuffer(nativeBuffer);
        }

        if (!decodeOnly) {
            TestParcelable testParcelable = new TestParcelable(1024, "Hi Parcelable");
            kv.encode("parcel", testParcelable);
        }
        TestParcelable result = kv.decodeParcelable("parcel", TestParcelable.class);
        if (result != null) {
            Log.d("MMKV", "parcel: " + result.iValue + ", " + result.sValue);
        } else {
            Log.e("MMKV", "fail to decodeParcelable of key:parcel");
        }

        Log.i("MMKV", "allKeys: " + Arrays.toString(kv.allKeys()));
        Log.i("MMKV", "count = " + kv.count() + ", totalSize = " + kv.totalSize());
        Log.i("MMKV", "containsKey[string]: " + kv.containsKey("string"));

        kv.removeValueForKey("bool");
        Log.i("MMKV", "bool: " + kv.decodeBool("bool"));
        kv.removeValuesForKeys(new String[] {"int", "long"});

        kv.encode("null string", "some string");
        Log.i("MMKV", "string before set null: " + kv.decodeString("null string"));
        kv.encode("null string", (String) null);
        Log.i("MMKV", "string after set null: " + kv.decodeString("null string")
                          + ", containsKey:" + kv.contains("null string"));

        //kv.sync();
        //kv.async();
        //kv.clearAll();
        kv.clearMemoryCache();
        Log.i("MMKV", "allKeys: " + Arrays.toString(kv.allKeys()));
        Log.i("MMKV", "isFileValid[" + kv.mmapID() + "]: " + MMKV.isFileValid(kv.mmapID()));

        return kv;
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

        Log.i("MMKV", "allKeys: " + Arrays.toString(kv.allKeys()));

        Log.i("MMKV", "bool: " + kv.getBoolean("bool", false));
        Log.i("MMKV", "int: " + kv.getInt("int", 0));
        Log.i("MMKV", "long: " + kv.getLong("long", 0));
        Log.i("MMKV", "float: " + kv.getFloat("float", 0));
        Log.i("MMKV", "double: " + kv.decodeDouble("double"));
        Log.i("MMKV", "string: " + kv.getString("string", null));
        Log.i("MMKV", "string-set: " + kv.getStringSet("string-set", null));
        Log.i("MMKV",
              "linked-string-set: " + kv.decodeStringSet("string-set", null, LinkedHashSet.class));
    }

    private void testReKey() {
        final String mmapID = "testAES_reKey1";
        MMKV kv = testMMKV(mmapID, null, false, null);
        if (kv == null) {
            return;
        }

        kv.reKey("Key_seq_1");
        kv.clearMemoryCache();
        testMMKV(mmapID, "Key_seq_1", true, null);

        kv.reKey("Key_seq_2");
        kv.clearMemoryCache();
        testMMKV(mmapID, "Key_seq_2", true, null);

        kv.reKey(null);
        kv.clearMemoryCache();
        testMMKV(mmapID, null, true, null);
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
        Log.i("MMKV", "bool: " + kv.decodeBool("bool"));

        kv.encode("int", Integer.MIN_VALUE);
        Log.i("MMKV", "int: " + kv.decodeInt("int"));

        kv.encode("long", Long.MAX_VALUE);
        Log.i("MMKV", "long: " + kv.decodeLong("long"));

        kv.encode("float", -3.14f);
        Log.i("MMKV", "float: " + kv.decodeFloat("float"));

        kv.encode("double", Double.MIN_VALUE);
        Log.i("MMKV", "double: " + kv.decodeDouble("double"));

        kv.encode("string", "Hello from mmkv");
        Log.i("MMKV", "string: " + kv.decodeString("string"));

        byte[] bytes = {'m', 'm', 'k', 'v'};
        kv.encode("bytes", bytes);
        Log.i("MMKV", "bytes: " + new String(kv.decodeBytes("bytes")));

        Log.i("MMKV", "allKeys: " + Arrays.toString(kv.allKeys()));
        Log.i("MMKV", "count = " + kv.count() + ", totalSize = " + kv.totalSize());
        Log.i("MMKV", "containsKey[string]: " + kv.containsKey("string"));

        kv.removeValueForKey("bool");
        Log.i("MMKV", "bool: " + kv.decodeBool("bool"));
        kv.removeValueForKey("int");
        kv.removeValueForKey("long");
        //kv.removeValuesForKeys(new String[] {"int", "long"});
        //kv.clearAll();
        kv.clearMemoryCache();
        Log.i("MMKV", "allKeys: " + Arrays.toString(kv.allKeys()));
        Log.i("MMKV", "isFileValid[" + kv.mmapID() + "]: " + MMKV.isFileValid(kv.mmapID()));
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

    private void testHolderForMultiThread() {
        final int COUNT = 1;
        final int THREAD_COUNT = 1;
        final String ID = "Hotel";
        final String KEY = "California";
        final String VALUE = "You can checkout any time you like, but you can never leave.";

        final MMKV mmkv = MMKV.mmkvWithID(ID);
        Runnable task = new Runnable() {
            public void run() {
                for (int i = 0; i < COUNT; ++i) {
                    mmkv.putString(KEY, VALUE);
                    mmkv.getString(KEY, null);
                    mmkv.remove(KEY);
                }
            }
        };

        for (int i = 0; i < THREAD_COUNT; ++i) {
            new Thread(task, "MMKV-" + i).start();
        }
    }

    private void testInterProcessLockPhase1() {
        MMKV mmkv1 = MMKV.mmkvWithID(MyService.LOCK_PHASE_1, MMKV.MULTI_PROCESS_MODE);
        mmkv1.lock();
        Log.d("locked in main", MyService.LOCK_PHASE_1);

        Intent intent = new Intent(this, MyService.class);
        intent.putExtra(BenchMarkBaseService.CMD_ID, MyService.CMD_LOCK);
        startService(intent);
    }
    private void testInterProcessLockPhase2() {
        MMKV mmkv2 = MMKV.mmkvWithID(MyService.LOCK_PHASE_2, MMKV.MULTI_PROCESS_MODE);
        mmkv2.lock();
        Log.d("locked in main", MyService.LOCK_PHASE_2);
    }

    private void testCornerSize() {
        MMKV mmkv = MMKV.mmkvWithID("cornerSize", MMKV.MULTI_PROCESS_MODE, "aes");
        mmkv.clearAll();
        int size = MMKV.pageSize() - 2;
        size -= 4;
        String key = "key";
        int keySize = 3 + 1;
        size -= keySize;
        int valueSize = 3;
        size -= valueSize;
        byte[] value = new byte[size];
        mmkv.encode(key, value);
    }

    private void testFastRemoveCornerSize() {
        MMKV mmkv = MMKV.mmkvWithID("fastRemoveCornerSize");
        mmkv.clearAll();
        int size = MMKV.pageSize() - 4;
        size -= 4; // place holder size
        String key = "key";
        int keySize = 3 + 1;
        size -= keySize;
        int valueSize = 3;
        size -= valueSize;
        size -= (keySize + 1); // total size of fast remove
        size /= 16;
        byte[] value = new byte[size];
        for (int i = 0; i < value.length; i++) {
            value[i] = 'A';
        }
        for (int i = 0; i < 16; i++) {
            mmkv.encode(key, value); // when a full write back is occur, here's corruption happens
            mmkv.removeValueForKey(key);
        }
    }

    @Override
    public MMKVRecoverStrategic onMMKVCRCCheckFail(String mmapID) {
        return MMKVRecoverStrategic.OnErrorRecover;
    }

    @Override
    public MMKVRecoverStrategic onMMKVFileLengthError(String mmapID) {
        return MMKVRecoverStrategic.OnErrorRecover;
    }

    @Override
    public boolean wantLogRedirecting() {
        return true;
    }

    @Override
    public void mmkvLog(MMKVLogLevel level, String file, int line, String func, String message) {
        String log = "<" + file + ":" + line + "::" + func + "> " + message;
        switch (level) {
            case LevelDebug:
                Log.d("redirect logging MMKV", log);
                break;
            case LevelInfo:
                Log.i("redirect logging MMKV", log);
                break;
            case LevelWarning:
                Log.w("redirect logging MMKV", log);
                break;
            case LevelError:
                Log.e("redirect logging MMKV", log);
                break;
            case LevelNone:
                Log.e("redirect logging MMKV", log);
                break;
        }
    }

    @Override
    public void onContentChangedByOuterProcess(String mmapID) {
        Log.i("content changed", mmapID);
    }
}
