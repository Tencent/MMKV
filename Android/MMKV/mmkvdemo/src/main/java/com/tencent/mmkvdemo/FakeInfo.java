package com.tencent.mmkvdemo;

import android.os.Parcel;
import android.os.Parcelable;
import androidx.annotation.NonNull;

public class FakeInfo implements Parcelable {
    public String channelId;

    public int position;
    public FakeInfo() {
    }

    @NonNull
    @Override
    public String toString() {
        return "fake channelID = " + channelId + ", position = " + position;
    }

    protected FakeInfo(Parcel in) {
        channelId = in.readString();
        position = in.readInt();
    }

    public static final Creator<FakeInfo> CREATOR = new Creator<FakeInfo>() {
        @Override
        public FakeInfo createFromParcel(Parcel in) {
            return new FakeInfo(in);
        }

        @Override
        public FakeInfo[] newArray(int size) {
            return new FakeInfo[size];
        }
    };

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeString(channelId);
        dest.writeInt(position);
    }
}
