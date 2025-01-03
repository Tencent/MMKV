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
