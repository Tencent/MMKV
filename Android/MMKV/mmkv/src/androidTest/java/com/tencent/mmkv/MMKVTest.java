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

import static org.junit.Assert.*;

import android.content.Context;
import android.content.Intent;
import android.os.SystemClock;
import androidx.test.InstrumentationRegistry;
import java.util.HashSet;
import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Test;

public class MMKVTest {

    static MMKV mmkv;
    static final String KeyNotExist = "Key_Not_Exist";
    static final float Delta = 0.000001f;

    @BeforeClass
    public static void setUp() throws Exception {
        Context appContext = InstrumentationRegistry.getTargetContext();
        MMKV.initialize(appContext);

        mmkv = MMKV.mmkvWithID("unitTest", MMKV.SINGLE_PROCESS_MODE, "UnitTestCryptKey");
    }

    @AfterClass
    public static void tearDown() throws Exception {
        mmkv.clearAll();
    }

    @Test
    public void testBool() {
        boolean ret = mmkv.encode("bool", true);
        assertEquals(ret, true);

        boolean value = mmkv.decodeBool("bool");
        assertEquals(value, true);

        value = mmkv.decodeBool(KeyNotExist);
        assertEquals(value, false);

        value = mmkv.decodeBool(KeyNotExist, true);
        assertEquals(value, true);
    }

    @Test
    public void testInt() {
        boolean ret = mmkv.encode("int", Integer.MAX_VALUE);
        assertEquals(ret, true);

        int value = mmkv.decodeInt("int");
        assertEquals(value, Integer.MAX_VALUE);

        value = mmkv.decodeInt(KeyNotExist);
        assertEquals(value, 0);

        value = mmkv.decodeInt(KeyNotExist, -1);
        assertEquals(value, -1);
    }

    @Test
    public void testLong() {
        boolean ret = mmkv.encode("long", Long.MAX_VALUE);
        assertEquals(ret, true);

        long value = mmkv.decodeLong("long");
        assertEquals(value, Long.MAX_VALUE);

        value = mmkv.decodeLong(KeyNotExist);
        assertEquals(value, 0);

        value = mmkv.decodeLong(KeyNotExist, -1);
        assertEquals(value, -1);
    }

    @Test
    public void testFloat() {
        boolean ret = mmkv.encode("float", Float.MAX_VALUE);
        assertEquals(ret, true);

        float value = mmkv.decodeFloat("float");
        assertEquals(value, Float.MAX_VALUE, Delta);

        value = mmkv.decodeFloat(KeyNotExist);
        assertEquals(value, 0, Delta);

        value = mmkv.decodeFloat(KeyNotExist, -1);
        assertEquals(value, -1, Delta);
    }

    @Test
    public void testDouble() {
        boolean ret = mmkv.encode("double", Double.MAX_VALUE);
        assertEquals(ret, true);

        double value = mmkv.decodeDouble("double");
        assertEquals(value, Double.MAX_VALUE, Delta);

        value = mmkv.decodeDouble(KeyNotExist);
        assertEquals(value, 0, Delta);

        value = mmkv.decodeDouble(KeyNotExist, -1);
        assertEquals(value, -1, Delta);
    }

    @Test
    public void testString() {
        String str = "Hello 2018 world cup 世界杯";
        boolean ret = mmkv.encode("string", str);
        assertEquals(ret, true);

        String value = mmkv.decodeString("string");
        assertEquals(value, str);

        value = mmkv.decodeString(KeyNotExist);
        assertEquals(value, null);

        value = mmkv.decodeString(KeyNotExist, "Empty");
        assertEquals(value, "Empty");
    }

    @Test
    public void testStringSet() {
        HashSet<String> set = new HashSet<String>();
        set.add("W");
        set.add("e");
        set.add("C");
        set.add("h");
        set.add("a");
        set.add("t");
        boolean ret = mmkv.encode("string_set", set);
        assertEquals(ret, true);

        HashSet<String> value = (HashSet<String>) mmkv.decodeStringSet("string_set");
        assertEquals(value, set);

        value = (HashSet<String>) mmkv.decodeStringSet(KeyNotExist);
        assertEquals(value, null);

        set = new HashSet<String>();
        set.add("W");
        value = (HashSet<String>) mmkv.decodeStringSet(KeyNotExist, set);
        assertEquals(value, set);
    }

    @Test
    public void testBytes() {
        byte[] bytes = {'m', 'm', 'k', 'v'};
        boolean ret = mmkv.encode("bytes", bytes);
        assertEquals(ret, true);

        byte[] value = mmkv.decodeBytes("bytes");
        assertArrayEquals(value, bytes);
    }

    @Test
    public void testRemove() {
        boolean ret = mmkv.encode("bool_1", true);
        ret &= mmkv.encode("int_1", Integer.MIN_VALUE);
        ret &= mmkv.encode("long_1", Long.MIN_VALUE);
        ret &= mmkv.encode("float_1", Float.MIN_VALUE);
        ret &= mmkv.encode("double_1", Double.MIN_VALUE);
        ret &= mmkv.encode("string_1", "hello");

        HashSet<String> set = new HashSet<String>();
        set.add("W");
        set.add("e");
        set.add("C");
        set.add("h");
        set.add("a");
        set.add("t");
        ret &= mmkv.encode("string_set_1", set);

        byte[] bytes = {'m', 'm', 'k', 'v'};
        ret &= mmkv.encode("bytes_1", bytes);
        assertEquals(ret, true);

        {
            long count = mmkv.count();

            mmkv.removeValueForKey("bool_1");
            mmkv.removeValuesForKeys(new String[] {"int_1", "long_1"});

            long newCount = mmkv.count();
            assertEquals(count, newCount + 3);
        }

        boolean bValue = mmkv.decodeBool("bool_1");
        assertEquals(bValue, false);

        int iValue = mmkv.decodeInt("int_1");
        assertEquals(iValue, 0);

        long lValue = mmkv.decodeLong("long_1");
        assertEquals(lValue, 0);

        float fValue = mmkv.decodeFloat("float_1");
        assertEquals(fValue, Float.MIN_VALUE, Delta);

        double dValue = mmkv.decodeDouble("double_1");
        assertEquals(dValue, Double.MIN_VALUE, Delta);

        String sValue = mmkv.decodeString("string_1");
        assertEquals(sValue, "hello");

        HashSet<String> hashSet = (HashSet<String>) mmkv.decodeStringSet("string_set_1");
        assertEquals(hashSet, set);

        byte[] byteValue = mmkv.decodeBytes("bytes_1");
        assertArrayEquals(bytes, byteValue);
    }

    @Test
    public void testIPCUpdateInt() {
        MMKV mmkv = MMKV.mmkvWithID(MMKVTestService.SharedMMKVID, MMKV.MULTI_PROCESS_MODE);
        mmkv.encode(MMKVTestService.SharedMMKVKey, 1024);

        Context appContext = InstrumentationRegistry.getTargetContext();
        Intent intent = new Intent(appContext, MMKVTestService.class);
        intent.putExtra(MMKVTestService.CMD_Key, MMKVTestService.CMD_Update);
        appContext.startService(intent);

        SystemClock.sleep(1000 * 3);
        int value = mmkv.decodeInt(MMKVTestService.SharedMMKVKey);
        assertEquals(value, 1024 + 1);
    }

    @Test
    public void testIPCLock() {
        Context appContext = InstrumentationRegistry.getTargetContext();

        Intent intent = new Intent(appContext, MMKVTestService.class);
        intent.putExtra(MMKVTestService.CMD_Key, MMKVTestService.CMD_Lock);
        appContext.startService(intent);

        SystemClock.sleep(1000 * 3);
        MMKV mmkv = MMKV.mmkvWithID(MMKVTestService.SharedMMKVID, MMKV.MULTI_PROCESS_MODE);
        boolean ret = mmkv.tryLock();
        assertEquals(ret, false);

        intent.putExtra(MMKVTestService.CMD_Key, MMKVTestService.CMD_Kill);
        appContext.startService(intent);

        SystemClock.sleep(1000 * 3);
        ret = mmkv.tryLock();
        assertEquals(ret, true);
    }
}
