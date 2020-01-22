package com.tencent.mmkvdemo;

import android.os.Parcel;
import android.os.Parcelable;
import java.util.LinkedList;
import java.util.List;

class TestParcelable implements Parcelable {

    int iValue;
    String sValue;
    List<FakeInfo> list;
    @Override
    public void writeToParcel(Parcel parcel, int i) {
        parcel.writeInt(iValue);
        parcel.writeString(sValue);
        // parcel.writeTypedList(list);
    }

    @Override
    public int describeContents() {
        return 0;
    }

    TestParcelable(int i, String s) {
        iValue = i;
        sValue = s;
        // list = new LinkedList<>();
        // list.add(new Info("channel", 1));
        // list.add(new Info("oppo", 2));
    }

    private TestParcelable(Parcel in) {
        iValue = in.readInt();
        sValue = in.readString();
        list = in.createTypedArrayList(FakeInfo.CREATOR);
    }

    public static final Creator<TestParcelable> CREATOR = new Creator<TestParcelable>() {
        @Override
        public TestParcelable createFromParcel(Parcel parcel) {
            return new TestParcelable(parcel);
        }

        @Override
        public TestParcelable[] newArray(int i) {
            return new TestParcelable[i];
        }
    };
}
