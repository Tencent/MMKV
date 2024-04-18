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

package com.tencent.mmkv;

import android.os.Parcel;
import android.os.ParcelFileDescriptor;
import android.os.Parcelable;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.jetbrains.annotations.Contract;

import java.io.IOException;

/**
 * A helper class for MMKV based on Anonymous Shared Memory. {@link MMKV#mmkvWithAshmemID}
 */
public final class ParcelableMMKV implements Parcelable {
    private final String mmapID;
    private int ashmemFD = -1;
    private int ashmemMetaFD = -1;
    private String cryptKey = null;

    public ParcelableMMKV(@NonNull MMKV mmkv) {
        mmapID = mmkv.mmapID();
        ashmemFD = mmkv.ashmemFD();
        ashmemMetaFD = mmkv.ashmemMetaFD();
        cryptKey = mmkv.cryptKey();
    }

    private ParcelableMMKV(String id, int fd, int metaFD, String key) {
        mmapID = id;
        ashmemFD = fd;
        ashmemMetaFD = metaFD;
        cryptKey = key;
    }

    @Nullable
    public MMKV toMMKV() {
        if (ashmemFD >= 0 && ashmemMetaFD >= 0) {
            return MMKV.mmkvWithAshmemFD(mmapID, ashmemFD, ashmemMetaFD, cryptKey);
        }
        return null;
    }

    @Override
    public int describeContents() {
        return CONTENTS_FILE_DESCRIPTOR;
    }

    @Override
    public void writeToParcel(@NonNull Parcel dest, int flags) {
        try {
            dest.writeString(mmapID);
            ParcelFileDescriptor fd = ParcelFileDescriptor.fromFd(ashmemFD);
            ParcelFileDescriptor metaFD = ParcelFileDescriptor.fromFd(ashmemMetaFD);
            flags = flags | Parcelable.PARCELABLE_WRITE_RETURN_VALUE;
            fd.writeToParcel(dest, flags);
            metaFD.writeToParcel(dest, flags);
            if (cryptKey != null) {
                dest.writeString(cryptKey);
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public static final Parcelable.Creator<ParcelableMMKV> CREATOR = new Parcelable.Creator<ParcelableMMKV>() {
        @Nullable
        @Override
        public ParcelableMMKV createFromParcel(@NonNull Parcel source) {
            String mmapID = source.readString();
            ParcelFileDescriptor fd = ParcelFileDescriptor.CREATOR.createFromParcel(source);
            ParcelFileDescriptor metaFD = ParcelFileDescriptor.CREATOR.createFromParcel(source);
            String cryptKey = source.readString();
            if (fd != null && metaFD != null) {
                return new ParcelableMMKV(mmapID, fd.detachFd(), metaFD.detachFd(), cryptKey);
            }
            return null;
        }

        @NonNull
        @Contract(value = "_ -> new", pure = true)
        @Override
        public ParcelableMMKV[] newArray(int size) {
            return new ParcelableMMKV[size];
        }
    };
}
