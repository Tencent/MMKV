package com.tencent.mmkv.sample

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import com.tencent.mmkv.kmp.MMKV
import com.tencent.mmkv.kmp.initialize

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Android-specific initialization (requires Context)
        MMKV.initialize(this)

        setContent {
            App()
        }
    }
}
