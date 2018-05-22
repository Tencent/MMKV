# MMKV——下基于 mmap 的高性能通用 key-value 组件
MMKV 是基于 mmap 内存映射的 key-value 组件，底层序列化/反序列化使用 protobuf 实现，性能高，稳定性强。从2015年中至今，在 iOS 微信上使用已有近 3 年，其性能和稳定性经过了时间的验证。近期也已移植到 Android 平台，一并开源。


## MMKV 源起
在微信客户端的日常运营中，时不时就会爆发特殊文字引起系统的 crash，[参考文章](http://km.oa.com/articles/show/357120)，文章里面设计的技术方案是在关键代码前后进行计数器的加减，通过检查计数器的异常，来发现引起闪退的异常文字。在会话列表、会话界面等有大量 cell 的地方，希望新加的计时器不会影响滑动性能；另外这些计数器还要永久存储下来——因为闪退随时可能发生。这就需要一个性能非常高的通用 key-value 存储组件，我们考察了 SharedPreferences、NSUserDefaults、SQLite 等常见组件，发现都没能满足如此苛刻的性能要求。考虑到这个防 crash 方案最主要的诉求还是实时写入，而 mmap 内存映射文件刚好满足这种需求，我们尝试通过它来实现一套 key-value 组件。

## MMKV 原理
### 内存准备
通过 mmap 内存映射文件，提供一段可供随时写入的内存块，App 只管往里面写数据，由操作系统负责将内存回写到文件，不必担心 crash 导致数据丢失。

### 数据组织
数据序列化方面我们选用 protobuf 协议，pb 在性能和空间占用上都有不错的表现。考虑到我们要提供的是通用 kv 组件，key 可以限定是 string 字符串类型，value 则多种多样（int/bool/double 等）。要做到通用的话，考虑将 value 通过 protobuf 协议序列化成统一的内存块（buffer），然后就可以将这些 KV 对象序列化到内存中。

```
message KV {
	string key = 1;
	buffer value = 2;
}

-(BOOL)setInt32:(int32_t)value forKey:(NSString*)key {
	auto data = PBEncode(value);
	return [self setData:data forKey:key];
}

-(BOOL)setData:(NSData*)data forKey:(NSString*)key {
	auto kv = KV { key, data };
	auto buf = PBEncode(kv);
	return [self write:buf];
}

```

### 写入优化
标准 protobuf 不提供增量更新的能力，每次写入都必须全量写入。考虑到主要使用场景是频繁地进行写入更新，我们需要有增量更新的能力。得益于之前实现 [PBCoding 组件](http://km.oa.com/articles/view/197684)的经验，直接将增量 kv 对象序列化后，append 到内存末尾；这样同一个 key 会有新旧若干份数据，最新的数据在最后；那么只需在程序启动第一次打开 mmkv 时，不断用后读入的 value 替换之前的值，就可以保证数据是最新有效的。

### 空间增长
使用 append 实现增量更新带来了一个新的问题，就是不断 append 的话，文件大小会增长得不可控。例如同一个 key 不断更新的话，是可能耗尽几百 M 甚至上 G 空间，而事实上整个 kv 文件就这一个 key，不到 1k 空间就存得下。这明显是不可取的。我们需要在性能和空间上做个折中：以内存 pagesize 为单位申请空间，在空间用尽之前都是 append 模式；当 append 到文件末尾时，进行文件重整、key 排重，尝试序列化保存排重结果；排重后空间还是不够用的话，将文件扩大一倍，直到空间足够。

```
-(BOOL)append:(NSData*)data {
	if (space >= data.length) {
		append(fd, data);
	} else {
		newData = unique(m_allKV);
		if (total_space >= newData.length) {
			write(fd, newData);
		} else {
			while (total_space < newData.length) {
				total_space *= 2;
			}
			ftruncate(fd, total_space);
			write(fd, newData);
		}
	}
}
```

### 数据有效性
考虑到文件系统、操作系统都有一定的不稳定性，我们另外增加了 crc 校验，对无效数据进行甄别。在 iOS 微信现网环境上，我们观察到有平均约 70万日次的数据校验不通过。

## iOS、Android 上手指南
* [iOS 版](iOS/)
* [Android 版](Android/)

## MMKV 性能
写了个简单的测试，将 MMKV、NSUserDefaults 的性能进行对比，具体代码见MMKVDemo。

* iOS 性能对比

![image](http://imgcache.oa.com/photos/31601/3b5a30acb12158f3a3de668452b4bac0.jpg)

测试环境：iPhone X 256G, iOS 11.2.6，单位：ms。另外，在测试中发现，NSUserDefaults 在几次测试，会有一次比较耗时的操作，怀疑是触发了数据重整写入。对比之下，MMKV即使触发数据重整，也保持了性能的稳定高效。

* Android 性能对比

![image](http://imgcache.oa.com/photos/31601/o_bb215f7fd392321793e6564128d4bd03.jpg)

测试环境：Pixel 2 XL 64G, Android 8.1.0，单位：ms。每组测试分别循环写入和读取 1000 次；SharedPreferences 使用 apply() 同步数据；SQLite 打开 WAL 选项。
