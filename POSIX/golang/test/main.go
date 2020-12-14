package main

import (
  "fmt"
  //"log"
  "math"

  "tencent.com/mmkv"
)

func main() {
  mmkv.InitializeMMKV("/tmp/mmkv")

  functionalTest()
  testReKey()
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

  kv.SetString("Hello world, from MMKV!", "string")
  fmt.Println("string =", kv.GetString("string"))

  kv.SetBytes([]byte("Hello world, from MMKV with bytes!"), "bytes")
  fmt.Println("bytes =", string(kv.GetBytes("bytes")))

  fmt.Println("contains \"bool\"? ", kv.Contains("bool"))
  kv.RemoveKey("bool")
  fmt.Println("after remove, contains \"bool\"? ", kv.Contains("bool"))

  kv.RemoveKeys([]string {"int32", "int64"})
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
    kv.SetString("Hello world, from MMKV!", "string")
  }
  fmt.Println("string =", kv.GetString("string"))

  if !decodeOnly {
    kv.SetBytes([]byte("Hello world, from MMKV with bytes!"), "bytes")
  }
  fmt.Println("bytes =", string(kv.GetBytes("bytes")))

  fmt.Println("contains \"bool\"? ", kv.Contains("bool"))
  kv.RemoveKey("bool")
  fmt.Println("after remove, contains \"bool\"? ", kv.Contains("bool"))

  kv.RemoveKeys([]string {"int32", "int64"})
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

