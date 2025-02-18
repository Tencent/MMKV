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
