package com.tencent.mmkv;

/**
 * Inter-process content change notification.
 * Triggered by any method call, such as getXXX() or setXXX() or {@link MMKV#checkContentChangedByOuterProcess()}.
 */
public interface MMKVContentChangeNotification {
    /**
     * Inter-process content change notification.
     * Triggered by any method call, such as getXXX() or setXXX() or {@link MMKV#checkContentChangedByOuterProcess()}.
     * @param mmapID The unique ID of the changed MMKV instance.
     */
    void onContentChangedByOuterProcess(String mmapID);
}
