package com.tencent.mmkv;

import android.os.Parcel;
import android.os.ParcelFileDescriptor;
import android.os.Parcelable;

import java.io.IOException;

public final class ParcelableMMKV implements Parcelable {
    private int m_fd = -1;
    private int m_metaFD = -1;

    public ParcelableMMKV(MMKV mmkv) {
        m_fd = mmkv.ashmemFD();
        m_metaFD = mmkv.ashmemMetaFD();
    }

    private ParcelableMMKV(int fd, int metaFD) {
        m_fd = fd;
        m_metaFD = metaFD;
    }

    public MMKV ToMMKV() {
        if (m_fd >=0 && m_metaFD >= 0) {
            return MMKV.mmkvWithAshmemFD(m_fd, m_metaFD);
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
            ParcelFileDescriptor fd = ParcelFileDescriptor.fromFd(m_fd);
            ParcelFileDescriptor metaFD = ParcelFileDescriptor.fromFd(m_metaFD);
            flags = flags | Parcelable.PARCELABLE_WRITE_RETURN_VALUE;
            fd.writeToParcel(dest, flags);
            metaFD.writeToParcel(dest, flags);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public static final Parcelable.Creator<ParcelableMMKV> CREATOR = new Parcelable.Creator<ParcelableMMKV>() {
        @Override
        public ParcelableMMKV createFromParcel(Parcel source) {
            ParcelFileDescriptor fd = ParcelFileDescriptor.CREATOR.createFromParcel(source);
            ParcelFileDescriptor metaFD = ParcelFileDescriptor.CREATOR.createFromParcel(source);
            if (fd != null && metaFD != null) {
                return new ParcelableMMKV(fd.detachFd(), metaFD.detachFd());
            }
            return null;
        }

        @Override
        public ParcelableMMKV[] newArray(int size) {
            return new ParcelableMMKV[size];
        }
    };
}
