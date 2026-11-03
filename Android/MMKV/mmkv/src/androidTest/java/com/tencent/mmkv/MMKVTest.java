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
import android.os.Parcel;
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
    public void testBufferAndFeatureStateAPIs() {
        MMKV kv = MMKV.mmkvWithID("bufferAndFeatureStateTest");
        kv.clearAll();
        try {
            byte[] bytes = {'m', 'm', 'k', 'v'};
            assertTrue(kv.encode("buffer", bytes));

            byte[] output = new byte[bytes.length];
            assertEquals(bytes.length, kv.writeValueToBuffer("buffer", output));
            assertArrayEquals(bytes, output);

            assertFalse(kv.isExpirationEnabled());
            assertTrue(kv.enableAutoKeyExpire(MMKV.ExpireNever));
            assertTrue(kv.isExpirationEnabled());
            assertTrue(kv.disableAutoKeyExpire());
            assertFalse(kv.isExpirationEnabled());

            assertFalse(kv.isCompareBeforeSetEnabled());
            kv.enableCompareBeforeSet();
            assertTrue(kv.isCompareBeforeSetEnabled());
            kv.disableCompareBeforeSet();
            assertFalse(kv.isCompareBeforeSetEnabled());
        } finally {
            kv.clearAll();
        }
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
    public void testGetExpireTimeForKey() throws InterruptedException {
        MMKV kv = MMKV.mmkvWithID("expireTimeTest");
        kv.clearAll();
        try {
            assertTrue(kv.disableAutoKeyExpire());
            assertTrue(kv.encode("disabled", true));
            assertEquals(0L, kv.getExpireTimeForKey("disabled"));
            assertEquals(0L, kv.getExpireTimeForKey("missing"));

            assertTrue(kv.enableAutoKeyExpire(60));
            assertEquals(0L, kv.getExpireTimeForKey("missing"));
            assertTrue(kv.encode("never", true, MMKV.ExpireNever));
            assertEquals(0L, kv.getExpireTimeForKey("never"));

            long before = System.currentTimeMillis() / 1000;
            assertTrue(kv.encode("default", true));
            long after = System.currentTimeMillis() / 1000;
            long defaultExpiration = kv.getExpireTimeForKey("default");
            assertTrue(defaultExpiration >= before + 60);
            assertTrue(defaultExpiration <= after + 60);

            // A uint32_t timestamp must remain positive across JNI, including after 2038.
            assertTrue(kv.encode("unsigned", true, -1));
            assertEquals(0xFFFFFFFFL, kv.getExpireTimeForKey("unsigned"));

            assertTrue(kv.encode("expired", true, 1));
            long expiration = kv.getExpireTimeForKey("expired");
            assertTrue(expiration > 0);
            Thread.sleep(2100);
            assertEquals(expiration, kv.getExpireTimeForKey("expired"));
            assertFalse(kv.containsKey("expired"));
            assertEquals(0L, kv.getExpireTimeForKey("expired"));

            kv.removeValueForKey("default");
            assertEquals(0L, kv.getExpireTimeForKey("default"));
            assertTrue(kv.disableAutoKeyExpire());
            assertEquals(0L, kv.getExpireTimeForKey("unsigned"));
        } finally {
            kv.clearAll();
            kv.close();
        }
    }

    @Test
    public void testExpirationOverflow() {
        MMKV kv = MMKV.mmkvWithID("expirationOverflowTest");
        kv.clearAll();
        // JNI casts int to uint32_t, so -1 exercises UINT32_MAX.
        assertTrue(kv.enableAutoKeyExpire(-1));

        assertTrue(kv.encode("expiration_overflow_auto", true));
        assertTrue(kv.decodeBool("expiration_overflow_auto"));

        assertTrue(kv.encode("expiration_overflow_manual", "manual", -1));
        assertEquals("manual", kv.decodeString("expiration_overflow_manual"));

        byte[] bytes = {'d', 'a', 't', 'a'};
        assertTrue(kv.encode("expiration_overflow_bytes", bytes, -1));
        assertArrayEquals(bytes, kv.decodeBytes("expiration_overflow_bytes"));

        assertEquals(3, kv.countNonExpiredKeys());
        kv.clearAll();
    }

    @Test
    public void testCloseIsIdempotentAndAllowsReopen() {
        String mmapID = "closeLifecycleTest";
        MMKV kv = MMKV.mmkvWithID(mmapID);
        assertTrue(kv.encode("value", true));

        kv.close();
        kv.close();
        assertFalse(kv.encode("afterClose", true));
        // Two live wrappers backed by the same native instance are outside the
        // supported close() contract. Discard the closed wrapper before reopen.
        kv = null;

        MMKV reopened = MMKV.mmkvWithID(mmapID);
        assertTrue(reopened.decodeBool("value"));
        reopened.clearAll();
        reopened.close();
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

    @Test
    public void testParcelableMMKVUnencrypted() {
        Context appContext = InstrumentationRegistry.getTargetContext();
        MMKV ashmemMMKV = MMKV.mmkvWithAshmemID(appContext, "testAshmemUnencrypted", 4096, MMKV.MULTI_PROCESS_MODE, null);
        assertNotNull(ashmemMMKV);
        ashmemMMKV.encode("test_key_unencrypted", 98765);

        ParcelableMMKV original = new ParcelableMMKV(ashmemMMKV);
        Parcel parcel = Parcel.obtain();
        try {
            original.writeToParcel(parcel, 0);
            parcel.setDataPosition(0);

            ParcelableMMKV unmarshaled = ParcelableMMKV.CREATOR.createFromParcel(parcel);
            assertNotNull("Unmarshaled ParcelableMMKV should not be null", unmarshaled);

            MMKV recovered = unmarshaled.toMMKV();
            assertNotNull("Recovered MMKV should not be null", recovered);
            assertEquals(ashmemMMKV.mmapID(), recovered.mmapID());

            // Ensure the sender's Ashmem MMKV instance remains operational and its FDs are intact
            assertEquals(98765, ashmemMMKV.decodeInt("test_key_unencrypted"));
            assertTrue(ashmemMMKV.encode("sender_alive_check", true));
            assertTrue(ashmemMMKV.decodeBool("sender_alive_check"));
        } finally {
            ashmemMMKV.clearAll();
            parcel.recycle();
        }
    }

    @Test
    public void testParcelableMMKVEncrypted() {
        Context appContext = InstrumentationRegistry.getTargetContext();
        MMKV ashmemMMKV = MMKV.mmkvWithAshmemID(appContext, "testAshmemEncrypted", 4096, MMKV.MULTI_PROCESS_MODE, "MyAshmemKey");
        assertNotNull(ashmemMMKV);
        ashmemMMKV.encode("test_key_encrypted", "secret_value");

        ParcelableMMKV original = new ParcelableMMKV(ashmemMMKV);
        Parcel parcel = Parcel.obtain();
        try {
            original.writeToParcel(parcel, 0);
            parcel.setDataPosition(0);

            ParcelableMMKV unmarshaled = ParcelableMMKV.CREATOR.createFromParcel(parcel);
            assertNotNull("Unmarshaled encrypted ParcelableMMKV should not be null", unmarshaled);

            MMKV recovered = unmarshaled.toMMKV();
            assertNotNull("Recovered encrypted MMKV should not be null", recovered);
            assertEquals(ashmemMMKV.mmapID(), recovered.mmapID());

            assertEquals("secret_value", ashmemMMKV.decodeString("test_key_encrypted"));
        } finally {
            ashmemMMKV.clearAll();
            parcel.recycle();
        }
    }
}
