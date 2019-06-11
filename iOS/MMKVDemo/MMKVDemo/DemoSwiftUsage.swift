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

@propertyWrapper
class OptionalMMKVProperty<T> {
	let key: String
	let defaultValue: T?
	let mmkv: MMKV

	init(key: String, defaultValue: T? = nil, mmkvID: String? = nil) {
		self.key = key
		self.defaultValue = defaultValue
		if mmkvID != nil {
			self.mmkv = MMKV.init(mmapID: mmkvID!)!
		} else {
			self.mmkv = MMKV.default()
		}
	}
	
	var value : T? {
		get {
			switch T.self {
			case is String.Type:
				return self.mmkv.string(forKey: self.key, defaultValue:self.defaultValue as? String) as! T?
			case is Data.Type:
				return self.mmkv.data(forKey: self.key, defaultValue:self.defaultValue as? Data) as! T?
			case is Date.Type:
				return self.mmkv.date(forKey: self.key, defaultValue:self.defaultValue as? Date) as! T?
			case is Bool.Type:
				return self.mmkv.bool(forKey: self.key, defaultValue: self.defaultValue as? Bool ?? false) as? T;
			case is Int32.Type:
				return self.mmkv.int32(forKey: self.key, defaultValue: self.defaultValue as? Int32 ?? 0) as? T;
			case is UInt32.Type:
				return self.mmkv.uint32(forKey: self.key, defaultValue: self.defaultValue as? UInt32 ?? 0) as? T;
			case is Int64.Type:
				return self.mmkv.int64(forKey: self.key, defaultValue: self.defaultValue as? Int64 ?? 0) as? T;
			case is UInt64.Type:
				return self.mmkv.uint64(forKey: self.key, defaultValue: self.defaultValue as? UInt64 ?? 0) as? T;
			case is Float.Type:
				return self.mmkv.float(forKey: self.key, defaultValue: self.defaultValue as? Float ?? 0) as? T;
			case is Double.Type:
				return self.mmkv.double(forKey: self.key, defaultValue: self.defaultValue as? Double ?? 0) as? T;
			case is (NSCoding & NSObjectProtocol).Type:
				return self.mmkv.object(of: T.self as! AnyClass, forKey: self.key) as? T ?? defaultValue;
			default:
				print("type not supported: \(T.self)")
				return defaultValue;
			}
		}
		set {
			//self.mmkv.set(newValue, forKey: self.key)
			switch newValue {
			case let str as NSString?:
				self.mmkv.set(str, forKey: self.key)
			case let data as NSData?:
				self.mmkv.set(data, forKey: self.key)
			case let date as NSDate?:
				self.mmkv.set(date, forKey: self.key)
			case let bool as Bool:
				self.mmkv.set(bool, forKey: self.key)
			case let int as Int32:
				self.mmkv.set(int, forKey: self.key)
			case let uint as UInt32:
				self.mmkv.set(uint, forKey: self.key)
			case let int64 as Int64:
				self.mmkv.set(int64, forKey: self.key)
			case let uint64 as UInt64:
				self.mmkv.set(uint64, forKey: self.key)
			case let float as Float:
				self.mmkv.set(float, forKey: self.key)
			case let double as Double:
				self.mmkv.set(double, forKey: self.key)
			case let uint64 as UInt64:
				self.mmkv.set(uint64, forKey: self.key)
			case let obj as (NSCoding & NSObjectProtocol)?:
				self.mmkv.set(obj, forKey: self.key)
			default:
				print("type not supported: \(T.self)")
			}
		}
	}
}

@propertyWrapper
class MMKVProperty<T> {
	let key: String
	let defaultValue: T
	let mmkv: MMKV
	
	init(key: String, defaultValue: T, mmkvID: String? = nil) {
		self.key = key
		self.defaultValue = defaultValue
		if mmkvID != nil {
			self.mmkv = MMKV.init(mmapID: mmkvID!)!
		} else {
			self.mmkv = MMKV.default()
		}
	}
	
	var value : T {
		get {
			switch T.self {
			case is String.Type:
				return self.mmkv.string(forKey: self.key, defaultValue:(self.defaultValue as! String)) as! T
			case is Data.Type:
				return self.mmkv.data(forKey: self.key, defaultValue:(self.defaultValue as! Data)) as! T
			case is Date.Type:
				return self.mmkv.date(forKey: self.key, defaultValue:(self.defaultValue as! Date)) as! T
			case is Bool.Type:
				return self.mmkv.bool(forKey: self.key, defaultValue: self.defaultValue as! Bool) as! T
			case is Int32.Type:
				return self.mmkv.int32(forKey: self.key, defaultValue: self.defaultValue as! Int32) as! T
			case is UInt32.Type:
				return self.mmkv.uint32(forKey: self.key, defaultValue: self.defaultValue as! UInt32) as! T
			case is Int64.Type:
				return self.mmkv.int64(forKey: self.key, defaultValue: self.defaultValue as! Int64) as! T
			case is UInt64.Type:
				return self.mmkv.uint64(forKey: self.key, defaultValue: self.defaultValue as! UInt64) as! T
			case is Float.Type:
				return self.mmkv.float(forKey: self.key, defaultValue: self.defaultValue as! Float) as! T
			case is Double.Type:
				return self.mmkv.double(forKey: self.key, defaultValue: self.defaultValue as! Double) as! T
			case is (NSCoding & NSObjectProtocol).Type:
				return self.mmkv.object(of: T.self as! AnyClass, forKey: self.key) as? T ?? self.defaultValue
			default:
				print("type not supported: \(T.self)")
				return defaultValue
			}
		}
		set {
			//self.mmkv.set(newValue, forKey: self.key)
			switch newValue {
			case let str as NSString:
				self.mmkv.set(str, forKey: self.key)
			case let data as NSData:
				self.mmkv.set(data, forKey: self.key)
			case let date as NSDate:
				self.mmkv.set(date, forKey: self.key)
			case let bool as Bool:
				self.mmkv.set(bool, forKey: self.key)
			case let int as Int32:
				self.mmkv.set(int, forKey: self.key)
			case let uint as UInt32:
				self.mmkv.set(uint, forKey: self.key)
			case let int64 as Int64:
				self.mmkv.set(int64, forKey: self.key)
			case let uint64 as UInt64:
				self.mmkv.set(uint64, forKey: self.key)
			case let float as Float:
				self.mmkv.set(float, forKey: self.key)
			case let double as Double:
				self.mmkv.set(double, forKey: self.key)
			case let uint64 as UInt64:
				self.mmkv.set(uint64, forKey: self.key)
			case let obj as (NSCoding & NSObjectProtocol):
				self.mmkv.set(obj, forKey: self.key)
			default:
				print("type not supported: \(T.self)")
			}
		}
	}
}

class DemoPropertyWrapper {
	@MMKVProperty(key: "int32", defaultValue: 0)
	var int32: Int32
	
	@OptionalMMKVProperty(key: "str")
	var str: String
}

class DemoSwiftUsage : NSObject {
	@objc func testSwiftFunctionality() {

        guard let mmkv = MMKV(mmapID: "testSwift") else {
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

        mmkv.removeValue(forKey: "bool")
        print("Swift: after delete bool = \(mmkv.bool(forKey: "bool"))")
	}
	
	@objc func testSwiftPropertyWrapper() {
		let demo = DemoPropertyWrapper()
		demo.int32 = 1024
		demo.str = "Hello, Swift 5.1 with property wrapper"
		print("SwiftPropertyWrapper: demo.int32=\(String(describing: demo.int32)), demo.str=\(String(describing: demo.str))")

		demo.int32 = 0
		demo.str = nil
		print("SwiftPropertyWrapper: demo.int32=\(String(describing: demo.int32)), demo.str=\(String(describing: demo.str))")
	}
}
