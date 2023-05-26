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

import Foundation

class DemoSwiftUsage : NSObject {
	@objc func testSwiftFunctionality() {

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

    @objc func testSwiftAutoExpire() {
        guard let mmkv = MMKV(mmapID: "testAutoExpire") else { return }

        mmkv.enableAutoKeyExpire(expiredInSeconds: MMKVExpireDuration.never.rawValue)
        mmkv.set(true, forKey: "key", expireDuration: MMKVExpireDuration.never.rawValue)
    }
}
