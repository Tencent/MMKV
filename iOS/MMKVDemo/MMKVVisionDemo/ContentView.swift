//
//  ContentView.swift
//  MMKVVisionDemo
//
//  Created by Cyril Bonaccini on 15/02/2024.
//  Copyright © 2024 Lingol. All rights reserved.
//

import SwiftUI
import MMKV

struct ContentView: View {
    var body: some View {
        VStack {
            Text("Hello, world!")
            Button("Press Me") {
                guard let mmkv = MMKV(mmapID: "testSwift", mode: MMKVMode.singleProcess) else {
                    return
                }

                mmkv.set(true, forKey: "bool")
                print("Swift: bool = \(mmkv.bool(forKey: "bool"))")

                mmkv.set(Int32(-1024), forKey: "int32")
                print("Swift: int32 = \(mmkv.int32(forKey: "int32"))")

                mmkv.set(UInt32.max, forKey: "uint32")
                print("Swift: uint32 = \(mmkv.uint32(forKey: "uint32"))")

                mmkv.set(Int64.min, forKey: "int64")
                print("Swift: int64 = \(mmkv.int64(forKey: "int64"))")

                mmkv.set(UInt64.max, forKey: "uint64")
                print("Swift: uint64 = \(mmkv.uint64(forKey: "uint64"))")

                mmkv.set(Float(-3.1415926), forKey: "float")
                print("Swift: float = \(mmkv.float(forKey: "float"))")

                mmkv.set(Double.infinity, forKey: "double")
                print("Swift: double = \(mmkv.double(forKey: "double"))")
                
                mmkv.set("Hello from Swift", forKey: "string")
                print("Swift: string = \(mmkv.string(forKey: "string") ?? "")")
                
                mmkv.set(NSDate(), forKey: "date")
                let date = mmkv.date(forKey: "date")
                print("Swift: date = \(date?.description(with: .current) ?? "null")")
                
                mmkv.set("Hello from Swift".data(using: .utf8) ?? Data(), forKey: "data")
                let data = mmkv.data(forKey: "data")
                let str = String(data: data ?? Data(), encoding: .utf8) ?? ""
                print("Swift: data = \(str)")

                let arr = [1, 0, 2, 4]
                if let objArr = arr as NSArray? {
                    mmkv.set(objArr, forKey:"array")
                    let result = mmkv.object(of: NSArray.self, forKey: "array");
                    print("Swift: array = \(result as! NSArray)")
                }

                mmkv.removeValue(forKey: "bool")
                print("Swift: after delete bool = \(mmkv.bool(forKey: "bool"))")
            }
        }
        .padding()
    }
}

#Preview(windowStyle: .automatic) {
    ContentView()
}
