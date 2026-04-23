# MMKV KMP — Android consumer ProGuard/R8 rules
#
# The KMP Android target delegates to the published `com.tencent:mmkv` library,
# which uses JNI and private native methods called from C++. R8 would strip
# those names in minified builds, causing UnsatisfiedLinkError / NoSuchMethodError
# at runtime. We mirror the upstream Android MMKV rules and add one block to
# keep the KMP wrapper's handler adapters reachable from reflection / JNI.

# Keep all native methods, their classes and any classes in their descriptors.
# Mirrored from Android/MMKV/mmkv/proguard-rules.pro.
-keepclasseswithmembers,includedescriptorclasses class com.tencent.mmkv.** {
    native <methods>;
    long nativeHandle;
    private static *** onMMKVCRCCheckFail(***);
    private static *** onMMKVFileLengthError(***);
    private static *** mmkvLogImp(...);
    private static *** onContentChangedByOuterProcess(***);
    private static *** onMMKVContentLoadSuccessfully(***);
}

# Keep the KMP wrapper's public API and handler adapter classes.
# MMKV.android.kt currently uses reflection to reach private native methods
# (isExpirationEnabled/isEncryptionEnabled/isCompareBeforeSetEnabled) on
# com.tencent.mmkv.MMKV — the upstream rule block above already covers those.
-keep,includedescriptorclasses class com.tencent.mmkv.kmp.** { *; }
