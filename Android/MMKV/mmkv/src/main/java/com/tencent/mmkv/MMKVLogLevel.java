package com.tencent.mmkv;

/**
 * The levels of MMKV log.
 */
public enum MMKVLogLevel {
    /**
     * Debug level. Not available for release/production build.
     */
    LevelDebug,

    /**
     * Info level. The default level.
     */
    LevelInfo,

    /**
     * Warning level.
     */
    LevelWarning,

    /**
     * Error level.
     */
    LevelError,

    /**
     * Special level for disabling all logging.
     * It's highly NOT suggested to turn off logging. Makes it hard to diagnose online/production bugs.
     */
    LevelNone
}
