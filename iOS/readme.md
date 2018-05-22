# MMKV——iOS 下基于 mmap 的高性能通用 key-value 组件
MMKV 是基于 mmap 内存映射的 key-value 组件，底层序列化/反序列化使用 protobuf 实现，性能高，稳定性强。从2015年中至今，在 iOS 微信上使用已有近 3 年，其性能和稳定性经过了时间的验证。

## MMKV 使用
### 快速上手
MMKV 提供一个全局的实例，可以直接使用：

```
	MMKV* mmkv = [MMKV defaultMMKV];
	
	[mmkv setBool:YES forKey:@"bool"];
	NSLog(@"bool:%d", [mmkv getBoolForKey:@"bool"]);
	
	[mmkv setInt32:-1024 forKey:@"int32"];
	NSLog(@"int32:%d", [mmkv getInt32ForKey:@"int32"]);
	
	[mmkv setInt64:std::numeric_limits<int64_t>::min() forKey:@"int64"];
	NSLog(@"int64:%lld", [mmkv getInt64ForKey:@"int64"]);
	
	[mmkv setFloat:-3.1415926 forKey:@"float"];
	NSLog(@"float:%f", [mmkv getFloatForKey:@"float"]);
	
	[mmkv setObject:@"hello, mmkv" forKey:@"string"];
	NSLog(@"string:%@", [mmkv getObjectOfClass:NSString.class forKey:@"string"]);
	
	[mmkv setObject:[NSDate date] forKey:@"date"];
	NSLog(@"date:%@", [mmkv getObjectOfClass:NSDate.class forKey:@"date"]);
	
	NSData* data = [@"hello, mmkv again and again" dataUsingEncoding:NSUTF8StringEncoding];
	[mmkv setObject:data forKey:@"data"];
	data = [mmkv getObjectOfClass:NSData.class forKey:@"data"];
	NSLog(@"data:%@", [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);

```

可以看到，MMKV 在使用上还是比较简单的。如果不同业务需要区别存储，也可以单独创建自己的实例：

```
	MMKV* mmkv = [MMKV mmkvWithID:@"MyID"];
	[mmkv setBool:NO forKey:@"bool"];
	……
```

### 支持的数据类型
#### 支持以下 C 语语言基础类型：bool、int32、int64、uint32、uint64、float、double
#### 支持以下 ObjC 类型：NSString、NSData、NSDate

## MMKV 性能
写了个简单的测试，将 MMKV、NSUserDefaults 的性能进行对比，具体代码见MMKVDemo。

![image](http://imgcache.oa.com/photos/31601/3b5a30acb12158f3a3de668452b4bac0.jpg)

测试环境：iPhone X 256G, iOS 11.2.6，单位：ms。

另外，在测试中发现，NSUserDefaults 在几次测试，会有一次比较耗时的操作，怀疑是触发了数据重整写入。对比之下，MMKV即使触发数据重整，也保持了性能的稳定高效。
