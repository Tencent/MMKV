/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2020 THL A29 Limited, a Tencent company.
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

package mmkv

import (
	"testing"
	"time"
	//     "regexp"
)

func TestVersionEmpty(t *testing.T) {
	msg := Version()
	if msg == "" {
		t.Fatalf("Version() == \"\"")
	}
}

func TestGetExpireTimeForKey(t *testing.T) {
	rootDir := t.TempDir()
	InitializeMMKVWithLogLevel(rootDir, MMKVLogNone)
	kv := MMKVWithIDAndConfig("expire_time", Config{RootPath: rootDir})
	defer kv.Close()

	if !kv.SetString("value", "disabled") {
		t.Fatal("SetString failed")
	}
	if got := kv.GetExpireTimeForKey("disabled"); got != 0 {
		t.Fatalf("disabled expiration = %d, want 0", got)
	}
	if got := kv.GetExpireTimeForKey("missing"); got != 0 {
		t.Fatalf("missing key = %d, want 0", got)
	}
	if !kv.EnableAutoKeyExpire(MMKV_Expire_Never) || !kv.SetStringExpire("value", "never", 0) {
		t.Fatal("enabling expiration or SetStringExpire failed")
	}
	for _, key := range []string{"never", "missing", ""} {
		if got := kv.GetExpireTimeForKey(key); got != 0 {
			t.Fatalf("GetExpireTimeForKey(%q) = %d, want 0", key, got)
		}
	}

	before := uint32(time.Now().Unix())
	if !kv.SetStringExpire("value", "finite", 60) {
		t.Fatal("SetStringExpire failed")
	}
	after := uint32(time.Now().Unix())
	if got := kv.GetExpireTimeForKey("finite"); got < before+60 || got > after+60 {
		t.Fatalf("finite expiration = %d, want [%d, %d]", got, before+60, after+60)
	}
	const maxUint32 = ^uint32(0)
	if !kv.SetStringExpire("value", "saturated", maxUint32) {
		t.Fatal("SetStringExpire failed")
	}
	if got := kv.GetExpireTimeForKey("saturated"); got != maxUint32 {
		t.Fatalf("saturated expiration = %d, want %d", got, maxUint32)
	}

	if !kv.SetStringExpire("value", "expired", 1) {
		t.Fatal("SetStringExpire failed")
	}
	expires := kv.GetExpireTimeForKey("expired")
	time.Sleep(time.Until(time.Unix(int64(expires), 0)) + 100*time.Millisecond)
	// Inspect before any contains/read operation can purge the entry.
	for i := 0; i < 2; i++ {
		if got := kv.GetExpireTimeForKey("expired"); got != expires {
			t.Fatalf("expired entry = %d, want %d before purge", got, expires)
		}
	}
	if got := kv.GetString("expired"); got != "" {
		t.Fatalf("GetString(expired) = %q, want empty", got)
	}
	if got := kv.GetExpireTimeForKey("expired"); got != 0 {
		t.Fatalf("purged entry = %d, want 0", got)
	}
}
