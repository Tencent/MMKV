# MMKV for Android
MMKV 是基于 mmap 内存映射的 key-value 组件，底层序列化/反序列化使用 protobuf 实现，性能高，稳定性强。目前已经移植到 Android 平台，并[内部开源](http://git.code.oa.com/wechat-team/mmkv)，欢迎 star、提issue和mr。


## MMKV 源起与原理
在微信客户端的日常运营中，时不时就会爆发特殊文字引起操作系统的 crash，[参考文章](http://km.oa.com/articles/show/357120)，文章里面设计的技术方案是在关键代码前后进行计数器的加减，通过检查计数器的异常，来发现引起闪退的异常文字。在会话列表、会话界面等有大量cell的地方，希望新加的计时器不会影响滑动性能；另外这些计数器还要永久存储下来——因为闪退随时可能发生。这就需要一个性能非常高的通用 key-value 存储组件，我们考察了 Android 平台的 SharedPreferences、SQLite 等常见组件，发现都没能满足如此苛刻的性能要求。于是我们考虑将 iOS 的 MMKV 组件移植到 Android 平台。MMKV 具体实现原理参考前文《[MMKV——iOS 下基于 mmap 的高性能通用 key-value 组件](http://km.oa.com/group/mmios/articles/show/334155)》。


## MMKV 使用
### 快速上手
* MMKV 已托管到公司内部 Maven 库，可以直接使用。在 App 的 build.gradle 里加上依赖：

```
repositories {
    maven {
        url "http://maven.oa.com/nexus/content/repositories/thirdparty"
    }
}
dependencies {
    implementation 'com.tencent:mmkv:1.0.3'
}
```

* 在 App 启动时初始化 MMKV，设定 MMKV 的根目录（files/mmkv/），例如在 MainActivity 里：

```
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        String rootDir = MMKV.initialize(this);
        System.out.println("mmkv root: " + rootDir);
        ……
	}
```

* MMKV 提供一个全局的实例，可以直接使用：

```
import com.tencent.mmkv.MMKV;
...
        MMKV kv = MMKV.defaultMMKV();

        kv.encode("bool", true);
        System.out.println("bool: " + kv.decodeBool("bool"));

        kv.encode("int", Integer.MIN_VALUE);
        System.out.println("int: " + kv.decodeInt("int"));

        kv.encode("long", Long.MAX_VALUE);
        System.out.println("long: " + kv.decodeLong("long"));

        kv.encode("float", -3.14f);
        System.out.println("float: " + kv.decodeFloat("float"));

        kv.encode("double", Double.MIN_VALUE);
        System.out.println("double: " + kv.decodeDouble("double"));

        kv.encode("string", "Hello from mmkv");
        System.out.println("string: " + kv.decodeString("string"));

        byte[] bytes = {'m', 'm', 'k', 'v'};
        kv.encode("bytes", bytes);
        System.out.println("bytes: " + new String(kv.decodeBytes("bytes")));

        kv.removeValueForKey("bool");
        System.out.println("bool: " + kv.decodeBool("bool"));
        
        kv.removeValuesForKeys(new String[]{"int", "long"});
        System.out.println("allKeys: " + Arrays.toString(kv.allKeys()));
```

可以看到，MMKV 在使用上还是比较简单的。

* 如果不同业务需要区别存储，也可以单独创建自己的实例：

```
        MMKV* mmkv = MMKV.mmkvWithID("MyID");
        mmkv.encode("bool", true);
        ……
```

### 支持的数据类型
* 支持以下 Java 语言基础类型：

```boolean、int、long、float、double、byte[]
```
* 支持以下 Java 类和容器：

```String、Set<String>
```

### SharedPreferences 迁移
* MMKV 提供了 importFromSharedPreferences() 函数，可以比较方便地迁移数据过来。
* MMKV 还额外实现了一遍 SharedPreferences、SharedPreferences.Editor 这两个 interface，在迁移的时候只需两三行代码即可，其他 CRUD 操作代码都不用改。

```
    private void testImportSharedPreferences() {
//      SharedPreferences preferences = getSharedPreferences("myData", MODE_PRIVATE);
        MMKV preferences = MMKV.mmkvWithID("myData");
        // 迁移旧数据
        {
            SharedPreferences old_man = getSharedPreferences("myData", MODE_PRIVATE);
            preferences.importFromSharedPreferences(old_man);
            old_man.edit().clear().commit();
        }
        // 跟以前用法一样
        SharedPreferences.Editor editor = preferences.edit();
        editor.putBoolean("bool", true);
        editor.putInt("int", Integer.MIN_VALUE);
        editor.putLong("long", Long.MAX_VALUE);
        editor.putFloat("float", -3.14f);
        editor.putString("string", "hello, imported");
        HashSet<String> set = new HashSet<String>();
        set.add("W"); set.add("e"); set.add("C"); set.add("h"); set.add("a"); set.add("t");
        editor.putStringSet("string-set", set);
        editor.commit();
	}
```

## MMKV 性能
写了个简单的测试，将 MMKV、SharedPreferences、SQLite 的性能进行对比，具体代码见 [git repo](http://git.code.oa.com/wechat-team/mmkv)。

![image](http://imgcache.oa.com/photos/31601/o_bb215f7fd392321793e6564128d4bd03.jpg)

测试环境：Pixel 2 XL 64G, Android 8.1.0，单位：ms。每组测试分别循环写入和读取 1000 次；SharedPreferences 使用 apply() 同步数据；SQLite 打开 WAL 选项。
