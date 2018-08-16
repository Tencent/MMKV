package com.tencent.mmkv;

import android.os.Parcel;
import android.os.ParcelFileDescriptor;
import android.os.Parcelable;

import java.io.IOException;

public final class ParcelableMMKV implements Parcelable {
    private int ashmemFD = -1;
    private int ashmemMetaFD = -1;
    private String cryptKey = null;

    public ParcelableMMKV(MMKV mmkv) {
        ashmemFD = mmkv.ashmemFD();
        ashmemMetaFD = mmkv.ashmemMetaFD();
        cryptKey = mmkv.cryptKey();
    }

    private ParcelableMMKV(int fd, int metaFD, String key) {
        ashmemFD = fd;
        ashmemMetaFD = metaFD;
        cryptKey = key;
    }

    public MMKV toMMKV() {
        if (ashmemFD >= 0 && ashmemMetaFD >= 0) {
            return MMKV.mmkvWithAshmemFD(ashmemFD, ashmemMetaFD, cryptKey);
        }
        return null;
    }

    @Override
    public int describeContents() {
        return CONTENTS_FILE_DESCRIPTOR;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        try {
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

    public static final Parcelable.Creator<ParcelableMMKV> CREATOR =
        new Parcelable.Creator<ParcelableMMKV>() {
            @Override
            public ParcelableMMKV createFromParcel(Parcel source) {
                ParcelFileDescriptor fd = ParcelFileDescriptor.CREATOR.createFromParcel(source);
                ParcelFileDescriptor metaFD = ParcelFileDescriptor.CREATOR.createFromParcel(source);
                String cryptKey = source.readString();
                if (fd != null && metaFD != null) {
                    return new ParcelableMMKV(fd.detachFd(), metaFD.detachFd(), cryptKey);
                }
                return null;
            }

            @Override
            public ParcelableMMKV[] newArray(int size) {
                return new ParcelableMMKV[size];
            }
        };
}
