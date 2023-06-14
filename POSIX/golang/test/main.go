package main

import (
	"fmt"
	//"log"
	"math"
	"time"

	"tencent.com/mmkv"
)

func main() {
	// init MMKV with root dir and log redirecting
	mmkv.InitializeMMKVWithLogLevelAndHandler("/tmp/mmkv", mmkv.MMKVLogInfo, logHandler)

	// you can set log redirecting
	// mmkv.RegisterLogHandler(logHandler)

	// you can set error handler
	mmkv.RegisterErrorHandler(errorHandler)
	// you can get notify content change by other process (not in realtime)
	mmkv.RegisterContentChangeHandler(contentChangeNotify)

	functionalTest()
	testReKey()

	testMMKV("test/Encrypt", "cryptKey", false)
	testBackup()
	testRestore()
	testAutoExpire()
}

func functionalTest() {
	kv := mmkv.DefaultMMKV()
	fmt.Println("actual size:", kv.ActualSize())
	fmt.Println("total size:", kv.TotalSize())

	kv.SetBool(true, "bool")
	fmt.Println("bool =", kv.GetBool("bool"))

	kv.SetInt32(math.MinInt32, "int32")
	fmt.Println("int32 =", kv.GetInt32("int32"))

	kv.SetUInt32(math.MaxUint32, "uint32")
	fmt.Println("uint32 =", kv.GetUInt32("uint32"))

	kv.SetInt64(math.MinInt64, "int64")
	fmt.Println("int64 =", kv.GetInt64("int64"))

	kv.SetUInt64(math.MaxUint64, "uint64")
	fmt.Println("uint64 =", kv.GetUInt64("uint64"))

	kv.SetFloat32(math.MaxFloat32, "float32")
	fmt.Println("float32 =", kv.GetFloat32("float32"))

	kv.SetFloat64(math.MaxFloat64, "float64")
	fmt.Println("float64 =", kv.GetFloat64("float64"))

	kv.SetString("Hello world, 你好 from MMKV!", "string")
	fmt.Println("string =", kv.GetString("string"))

	// much more efficient
	buffer := kv.GetStringBuffer("string")
	fmt.Println("string(buffer) =", buffer.StringView())
	// must call Destroy() after usage
	buffer.Destroy()

	kv.SetBytes([]byte("Hello world, 你好 from MMKV 以及 bytes!"), "bytes")
	fmt.Println("bytes =", string(kv.GetBytes("bytes")))

	// much more efficient
	buffer = kv.GetBytesBuffer("bytes")
	fmt.Println("bytes(buffer) =", string(buffer.ByteSliceView()))
	// must call Destroy() after usage
	buffer.Destroy()

	fmt.Println("contains \"bool\"? ", kv.Contains("bool"))
	kv.RemoveKey("bool")
	fmt.Println("after remove, contains \"bool\"? ", kv.Contains("bool"))

	kv.RemoveKeys([]string{"int32", "int64"})
	fmt.Println("count =", kv.Count(), ", all keys:", kv.AllKeys())

	kv.Trim()
	kv.ClearMemoryCache()
	fmt.Println("all keys:", kv.AllKeys())
	kv.ClearAll()
	fmt.Println("all keys:", kv.AllKeys())
	kv.Sync(true)
	kv.Close()
}

func testMMKV(mmapID string, cryptKey string, decodeOnly bool) mmkv.MMKV {
	kv := mmkv.MMKVWithIDAndModeAndCryptKey(mmapID, mmkv.MMKV_SINGLE_PROCESS, cryptKey)

	if !decodeOnly {
		kv.SetBool(true, "bool")
	}
	fmt.Println("bool =", kv.GetBool("bool"))

	if !decodeOnly {
		kv.SetInt32(math.MinInt32, "int32")
	}
	fmt.Println("int32 =", kv.GetInt32("int32"))

	if !decodeOnly {
		kv.SetUInt32(math.MaxUint32, "uint32")
	}
	fmt.Println("uint32 =", kv.GetUInt32("uint32"))

	if !decodeOnly {
		kv.SetInt64(math.MinInt64, "int64")
	}
	fmt.Println("int64 =", kv.GetInt64("int64"))

	if !decodeOnly {
		kv.SetUInt64(math.MaxUint64, "uint64")
	}
	fmt.Println("uint64 =", kv.GetUInt64("uint64"))

	if !decodeOnly {
		kv.SetFloat32(math.MaxFloat32, "float32")
	}
	fmt.Println("float32 =", kv.GetFloat32("float32"))

	if !decodeOnly {
		kv.SetFloat64(math.MaxFloat64, "float64")
	}
	fmt.Println("float64 =", kv.GetFloat64("float64"))

	if !decodeOnly {
		kv.SetString("Hello world, 你好 from MMKV!", "string")
	}
	fmt.Println("string =", kv.GetString("string"))

	if !decodeOnly {
		kv.SetBytes([]byte("Hello world, 你好 from MMKV 以及 bytes!"), "bytes")
	}
	fmt.Println("bytes =", string(kv.GetBytes("bytes")))

	fmt.Println("contains \"bool\"? ", kv.Contains("bool"))
	kv.RemoveKey("bool")
	fmt.Println("after remove, contains \"bool\"? ", kv.Contains("bool"))

	kv.RemoveKeys([]string{"int32", "int64"})
	fmt.Println("all keys:", kv.AllKeys())

	return kv
}

func testReKey() {
	mmapID := "testAES_reKey1"
	kv := testMMKV(mmapID, "", false)
	if kv == nil {
		return
	}

	kv.ReKey("Key_seq_1")
	kv.ClearMemoryCache()
	testMMKV(mmapID, "Key_seq_1", true)

	kv.ReKey("Key_seq_2")
	kv.ClearMemoryCache()
	testMMKV(mmapID, "Key_seq_2", true)

	kv.ReKey("")
	kv.ClearMemoryCache()
	testMMKV(mmapID, "", true)
}

func testBackup() {
	rootDir := "/tmp/mmkv_backup"
	mmapID := "test/Encrypt"
	ret := mmkv.BackupOneToDirectory(mmapID, rootDir)
	fmt.Println("backup one return: ", ret)

	count := mmkv.BackupAllToDirectory(rootDir)
	fmt.Println("backup all count: ", count)
}

func testRestore() {
	rootDir := "/tmp/mmkv_backup"
	mmapID := "test/Encrypt"
	aesKey := "cryptKey"
	kv := mmkv.MMKVWithIDAndModeAndCryptKey(mmapID, mmkv.MMKV_SINGLE_PROCESS, aesKey)
	kv.SetString("string value before restore", "test_restore_key")
	fmt.Println("before restore [", kv.MMAP_ID(), "] allKeys: ", kv.AllKeys())

	ret := mmkv.RestoreOneFromDirectory(mmapID, rootDir)
	fmt.Println("restore one return: ", ret)
	if ret {
		fmt.Println("after restore [", kv.MMAP_ID(), "] allKeys: ", kv.AllKeys())
	}

	count := mmkv.RestoreAllFromDirectory(rootDir)
	fmt.Println("restore all count: ", count)
	if count > 0 {
		backupMMKV := mmkv.MMKVWithIDAndModeAndCryptKey(mmapID, mmkv.MMKV_SINGLE_PROCESS, aesKey)
		fmt.Println("check on restore [", backupMMKV.MMAP_ID(), "] allKeys: ", backupMMKV.AllKeys())

		backupMMKV = mmkv.MMKVWithID("testAES_reKey1")
		fmt.Println("check on restore [", backupMMKV.MMAP_ID(), "] allKeys: ", backupMMKV.AllKeys())

		backupMMKV = mmkv.DefaultMMKV()
		fmt.Println("check on restore [", backupMMKV.MMAP_ID(), "] allKeys: ", backupMMKV.AllKeys())
	}
}

func testAutoExpire()  {
	kv := mmkv.MMKVWithID("testAutoExpire")
	kv.ClearAll()
	kv.Trim()
	kv.DisableAutoKeyExpire()

	kv.SetBool(true, "auto_expire_key_1")
	kv.EnableAutoKeyExpire(1)
	kv.SetBoolExpire(true, "never_expire_key_1", mmkv.MMKV_Expire_Never)

	time.Sleep(2 * time.Second)
	fmt.Println("contains auto_expire_key_1:", kv.Contains("auto_expire_key_1"))
	fmt.Println("contains never_expire_key_1:", kv.Contains("never_expire_key_1"))

	kv.RemoveKey("never_expire_key_1")
	kv.EnableAutoKeyExpire(mmkv.MMKV_Expire_Never)
	kv.SetBool(true, "never_expire_key_1")
	kv.SetBoolExpire(true, "auto_expire_key_1", 1)
	time.Sleep(2 * time.Second)
	fmt.Println("contains never_expire_key_1:", kv.Contains("never_expire_key_1"))
	fmt.Println("contains auto_expire_key_1:", kv.Contains("auto_expire_key_1"))
}

func logHandler(level int, file string, line int, function string, message string) {
	var levelStr string
	switch level {
	case mmkv.MMKVLogDebug:
		levelStr = "[D]"
	case mmkv.MMKVLogInfo:
		levelStr = "[I]"
	case mmkv.MMKVLogWarning:
		levelStr = "[W]"
	case mmkv.MMKVLogError:
		levelStr = "[E]"
	default:
		levelStr = "[N]"
	}
	fmt.Printf("Redirect %v <%v:%v::%v> %v\n", levelStr, file, line, function, message)
}

func errorHandler(mmapID string, error int) int {
	var errorDesc string
	if error == mmkv.MMKVCRCCheckFail {
		errorDesc = "CRC-Error"
	} else {
		errorDesc = "File-Length-Error"
	}
	fmt.Println(mmapID, "has error type:", errorDesc)

	return mmkv.OnErrorRecover
}

func contentChangeNotify(mmapID string) {
	fmt.Println(mmapID, "content changed by other process")
}
