# MMKV——基于 mmap 的高性能通用 key-value 组件
MMKV 是基于 mmap 内存映射的 key-value 组件，底层序列化/反序列化使用 protobuf 实现，性能高，稳定性强。从 2015 年中至今，在 iOS 微信上使用已有近 3 年，其性能和稳定性经过了时间的验证。近期也已移植到 Android 平台，一并开源。


## MMKV 源起
在微信客户端的日常运营中，时不时就会爆发特殊文字引起系统的 crash，[参考文章](https://mp.weixin.qq.com/s?__biz=MzAwNDY1ODY2OQ==&mid=2649286826&idx=1&sn=35601cb1156617aa235b7fd4b085bfc4)，文章里面设计的技术方案是在关键代码前后进行计数器的加减，通过检查计数器的异常，来发现引起闪退的异常文字。在会话列表、会话界面等有大量 cell 的地方，希望新加的计时器不会影响滑动性能；另外这些计数器还要永久存储下来——因为闪退随时可能发生。这就需要一个性能非常高的通用 key-value 存储组件，我们考察了 SharedPreferences、NSUserDefaults、SQLite 等常见组件，发现都没能满足如此苛刻的性能要求。考虑到这个防 crash 方案最主要的诉求还是实时写入，而 mmap 内存映射文件刚好满足这种需求，我们尝试通过它来实现一套 key-value 组件。

## MMKV 原理
* **内存准备**  
通过 mmap 内存映射文件，提供一段可供随时写入的内存块，App 只管往里面写数据，由操作系统负责将内存回写到文件，不必担心 crash 导致数据丢失。
* **数据组织**  
数据序列化方面我们选用 protobuf 协议，pb 在性能和空间占用上都有不错的表现。
* **写入优化**  
考虑到主要使用场景是频繁地进行写入更新，我们需要有增量更新的能力。我们考虑将增量 kv 对象序列化后，append 到内存末尾。
* **空间增长**  
使用 append 实现增量更新带来了一个新的问题，就是不断 append 的话，文件大小会增长得不可控。我们需要在性能和空间上做个折中。

更详细的设计原理参考 [MMKV 原理](https://github.com/Tencent/MMKV/wiki/design)。

## iOS 指南
### 安装引入
推荐使用 CocoaPods：

  1. 安装 [CocoaPods](https://guides.CocoaPods.org/using/getting-started.html)；
  2. 打开命令行, `cd` 到你的项目工程目录, 输入 `pod repo update` 让 CocoaPods 感知最新的 MMKV 版本；
  3. 打开 Podfile, 添加 `pod 'MMKV'` 到你的 app target 里面；
  4. 在命令行输入 `pod install`；
  5. 用 Xcode 打开由 CocoaPods 自动生成的 `.xcworkspace` 文件；
  6. 添加头文件 `#import <MMKV/MMKV.h>`，就可以愉快地开始你的 MMKV 之旅了。

更多安装指引参考 [iOS Setup](https://github.com/Tencent/MMKV/wiki/iOS_setup_cn)。

### 快速上手
MMKV 的使用非常简单，无需任何配置，所有变更立马生效，无需调用 `synchronize`:

```objective-c
MMKV *mmkv = [MMKV defaultMMKV];
    
[mmkv setBool:YES forKey:@"bool"];
BOOL bValue = [mmkv getBoolForKey:@"bool"];
    
[mmkv setInt32:-1024 forKey:@"int32"];
int32_t iValue = [mmkv getInt32ForKey:@"int32"];
    
[mmkv setObject:@"hello, mmkv" forKey:@"string"];
NSString *str = [mmkv getObjectOfClass:NSString.class forKey:@"string"];
```

更详细的使用教程参考 [iOS Tutorial](https://github.com/Tencent/MMKV/wiki/iOS_tutorial_cn)。

### 性能对比
循环写入随机的`int` 1w 次，我们有如下性能对比：  
![](https://github.com/Tencent/MMKV/wiki/assets/profile_mini.jpg)  
更详细的性能对比参考 [iOS Benchmark](https://github.com/Tencent/MMKV/wiki/iOS_benchmark_cn)。

## Android 指南
### 安装引入
推荐使用 Maven：

```gradle
dependencies {
    implementation 'com.tencent:mmkv:1.0.12'
    // replace "1.0.12" with any available version
}
```

更多安装指引参考 [Android Setup](https://github.com/Tencent/MMKV/wiki/android_setup_cn)。

### 快速上手
MMKV 的使用非常简单，所有变更立马生效，无需调用 `sync`、`apply`。
在 App 启动时初始化 MMKV，设定 MMKV 的根目录（files/mmkv/），例如在 MainActivity 里：

```
protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    String rootDir = MMKV.initialize(this);
    System.out.println("mmkv root: " + rootDir);
    //……
}
```

MMKV 提供一个全局的实例，可以直接使用：

```Java
import com.tencent.mmkv.MMKV;
//……

MMKV kv = MMKV.defaultMMKV();

kv.encode("bool", true);
boolean bValue = kv.decodeBool("bool");

kv.encode("int", Integer.MIN_VALUE);
int iValue = kv.decodeInt("int");

kv.encode("string", "Hello from mmkv");
String str = kv.decodeString("string");
```
MMKV 支持**多进程访问**，更详细的用法参考 [Android Tutorial](https://github.com/Tencent/MMKV/wiki/android_tutorial_cn)。

### 性能对比
循环写入随机的`int` 1k 次，我们有如下性能对比：  
![](https://github.com/Tencent/MMKV/wiki/assets/profile_android_mini.jpg)  
更详细的性能对比参考 [Android Benchmark](https://github.com/Tencent/MMKV/wiki/android_benchmark_cn)。

## License
MMKV 以 BSD 3-Clause 证书开源，详情参见 [LICENSE.TXT](https://github.com/Tencent/MMKV/blob/master/LICENSE.TXT)。

## 版本历史
具体版本历史请参看 [CHANGELOG.md](./CHANGELOG.md)。

## 参与贡献如果你有兴趣参与贡献，可以参考 [CONTRIBUTING.md](https://github.com/Tencent/MMKV/blob/master/CONTRIBUTING.md)。[腾讯开源激励计划](https://opensource.tencent.com/contribution) 鼓励开发者的参与和贡献，期待你的加入。
## 问题 & 反馈
常见问题参见 [FAQ](https://github.com/Tencent/MMKV/wiki/FAQ_cn)，欢迎提 [issues](https://github.com/Tencent/MMKV/issues) 提问反馈。
