/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2025 THL A29 Limited, a Tencent company.
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

package com.tencent.mmkv;

import static com.tencent.mmkv.MMKV.SINGLE_PROCESS_MODE;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * a facade wraps custom root directory
 */
public final class NameSpace {
    private final String rootDir;

    // make it internal
    NameSpace(String dir) {
        rootDir = dir;
    }

    /**
     * @return The root folder of this NameSpace.
     */
    public String getRootDir() {
        return rootDir;
    }

    /**
     * Create an MMKV instance with an unique ID (in single-process mode).
     *
     * @param mmapID The unique ID of the MMKV instance.
     * @throws RuntimeException if there's an runtime error.
     */
    @NonNull
    public MMKV mmkvWithID(String mmapID) throws RuntimeException {
        long handle = MMKV.getMMKVWithID(mmapID, SINGLE_PROCESS_MODE, null, rootDir, 0);
        return MMKV.checkProcessMode(handle, mmapID, SINGLE_PROCESS_MODE);
    }

    /**
     * Create an MMKV instance in single-process or multi-process mode.
     *
     * @param mmapID The unique ID of the MMKV instance.
     * @param mode   The process mode of the MMKV instance, defaults to {@link #SINGLE_PROCESS_MODE}.
     * @throws RuntimeException if there's an runtime error.
     */
    @NonNull
    public MMKV mmkvWithID(String mmapID, int mode) throws RuntimeException {
        long handle = MMKV.getMMKVWithID(mmapID, mode, null, rootDir, 0);
        return MMKV.checkProcessMode(handle, mmapID, mode);
    }

    /**
     * Create an MMKV instance in single-process or multi-process mode.
     *
     * @param mmapID The unique ID of the MMKV instance.
     * @param mode   The process mode of the MMKV instance, defaults to {@link #SINGLE_PROCESS_MODE}.
     * @param expectedCapacity The file size you expected when opening or creating file
     * @throws RuntimeException if there's an runtime error.
     */
    @NonNull
    public MMKV mmkvWithID(String mmapID, int mode, long expectedCapacity) throws RuntimeException {
        long handle = MMKV.getMMKVWithID(mmapID, mode, null, rootDir, expectedCapacity);
        return MMKV.checkProcessMode(handle, mmapID, mode);
    }

    /**
     * Create an MMKV instance in customize process mode, with an encryption key.
     *
     * @param mmapID   The unique ID of the MMKV instance.
     * @param mode     The process mode of the MMKV instance, defaults to {@link #SINGLE_PROCESS_MODE}.
     * @param cryptKey The encryption key of the MMKV instance (no more than 16 bytes).
     * @throws RuntimeException if there's an runtime error.
     */
    @NonNull
    public MMKV mmkvWithID(String mmapID, int mode, @Nullable String cryptKey) throws RuntimeException {
        long handle = MMKV.getMMKVWithID(mmapID, mode, cryptKey, rootDir, 0);
        return MMKV.checkProcessMode(handle, mmapID, mode);
    }

    /**
     * Create an MMKV instance with customize settings all in one.
     *
     * @param mmapID   The unique ID of the MMKV instance.
     * @param mode     The process mode of the MMKV instance, defaults to {@link #SINGLE_PROCESS_MODE}.
     * @param cryptKey The encryption key of the MMKV instance (no more than 16 bytes).
     * @param expectedCapacity The file size you expected when opening or creating file
     * @throws RuntimeException if there's an runtime error.
     */
    @NonNull
    public MMKV mmkvWithID(String mmapID, int mode, @Nullable String cryptKey, long expectedCapacity)
            throws RuntimeException {
        long handle = MMKV.getMMKVWithID(mmapID, mode, cryptKey, rootDir, expectedCapacity);
        return MMKV.checkProcessMode(handle, mmapID, mode);
    }

    /**
     * backup one MMKV instance to dstDir
     *
     * @param mmapID   the MMKV ID to backup
     * @param dstDir   the backup destination directory
     */
    public boolean backupOneToDirectory(String mmapID, String dstDir) {
        return MMKV.backupOneToDirectory(mmapID, dstDir, rootDir);
    }

    /**
     * restore one MMKV instance from srcDir
     *
     * @param mmapID   the MMKV ID to restore
     * @param srcDir   the restore source directory
     */
    public boolean restoreOneMMKVFromDirectory(String mmapID, String srcDir) {
        return MMKV.restoreOneMMKVFromDirectory(mmapID, srcDir, rootDir);
    }

    // TODO: we can't have these functionality because we can't know for sure
    //  that each instance inside NameSpace has been upgraded successfully or not.
    //  The workaround is to manually call backup/restore on each instance of NameSpace.
//    /**
//     * backup all MMKV instance to dstDir
//     *
//     * @param dstDir the backup destination directory
//     * @return count of MMKV successfully backuped
//     */
//    public long backupAllToDirectory(String dstDir) {
//        return MMKV.backupAllToDirectory(dstDir, rootDir);
//    }
//
//    /**
//     * restore all MMKV instance from srcDir
//     *
//     * @param srcDir the restore source directory
//     * @return count of MMKV successfully restored
//     */
//    public long restoreAllFromDirectory(String srcDir) {
//        return MMKV.restoreAllFromDirectory(srcDir, rootDir);
//    }

    /**
     * Check whether the MMKV file is valid or not.
     * Note: Don't use this to check the existence of the instance, the result is undefined on nonexistent files.
     */
    public boolean isFileValid(String mmapID) {
        return MMKV.isFileValid(mmapID, rootDir);
    }

    /**
     * remove the storage of the MMKV, including the data file & meta file (.crc)
     * Note: the existing instance (if any) will be closed & destroyed
     *
     * @param mmapID   The unique ID of the MMKV instance.
     */
    public boolean removeStorage(String mmapID) {
        return MMKV.removeStorage(mmapID, rootDir);
    }

    /**
     * check existence of the MMKV file
     * @param mmapID   The unique ID of the MMKV instance.
     */
    public boolean checkExist(String mmapID) {
        return MMKV.checkExist(mmapID, rootDir);
    }
}
