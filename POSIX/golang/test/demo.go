package main

import (
  "fmt"
  "tencent.com/mmkv"
)

func main() {
  mmkv.InitializeMMKV("/tmp/mmkv")

  kv := mmkv.MMKVWithID("test")
  kv.SetBool(true, "bool")
  fmt.Println("bool =", kv.GetBool("bool"))
}
