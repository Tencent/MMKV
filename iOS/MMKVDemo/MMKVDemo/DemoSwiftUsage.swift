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


@objc class DemoSwiftUsage : NSObject {
	func testSwiftFunctionality() {
		let mmkv = MMKV(id: "testSwift")!;
		
		mmkv.setBool(true, forKey: "bool");
		print("Swift: bool = \(mmkv.getBoolForKey("bool"))");
		
		mmkv.setInt32(-1024, forKey: "int32");
		print("Swift: int32 = \(mmkv.getInt32ForKey("int32"))");
		
		mmkv.setUInt32(UInt32.max, forKey: "uint32");
		print("Swift: uint32 = \(mmkv.getUInt32(forKey: "uint32"))");
		
		mmkv.setInt64(Int64.min, forKey: "int64");
		print("Swift: int64 = \(mmkv.getInt64ForKey("int64"))");
		
		mmkv.setUInt64(UInt64.max, forKey: "uint64");
		print("Swift: uint64 = \(mmkv.getUInt64(forKey:"uint64"))");
		
		mmkv.setFloat(-3.1415926, forKey: "float");
		print("Swift: float = \(mmkv.getFloatForKey("float"))");
		
		mmkv.setDouble(Double.infinity, forKey: "double");
		print("Swift: double = \(mmkv.getDoubleForKey("double"))");
		
		mmkv.setObject("Hello from Swift", forKey: "string");
		print("Swift: string = \(String(describing: mmkv.getObjectOf(NSString.self, forKey:"string")))");
		
		mmkv.setObject(NSDate(), forKey: "date");
		let date = mmkv.getObjectOf(NSDate.self, forKey:"date") as! Date;
		print("Swift: date = \(date.description(with: .current))");
		
		mmkv.setObject("Hello from Swift".data(using: .utf8), forKey: "data");
		let data = mmkv.getObjectOf(NSData.self, forKey:"data");
		let str = String(data: data as! Data, encoding: .utf8);
		print("Swift: data = \(String(describing: str))");
		
		mmkv.removeValue(forKey: "bool");
		print("Swift: after delete bool = \(mmkv.getBoolForKey("bool"))");
	}
}
