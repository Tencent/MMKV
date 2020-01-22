package com.tencent.mmkvdemo;

import android.os.Parcel;
import android.os.Parcelable;
import androidx.annotation.NonNull;

class Info implements Parcelable {
    public String channelId;

    public int position;
    public Info(String id, int pos) {
        channelId = id;
        position = pos;
    }

    @NonNull
    @Override
    public String toString() {
        return "channelID = " + channelId + ", position = " + position;
    }

    protected Info(Parcel in) {
        channelId = in.readString();
        position = in.readInt();
    }

    public static final Creator<Info> CREATOR = new Creator<Info>() {
        @Override
        public Info createFromParcel(Parcel in) {
            return new Info(in);
        }

        @Override
        public Info[] newArray(int size) {
            return new Info[size];
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
