package com.tencent.mmkv;

public enum MMKVLogLevel {
    LevelDebug, // not available for release/production build
    LevelInfo,  // default level
    LevelWarning,
    LevelError,
    LevelNone // special level used to disable all log messages
}
