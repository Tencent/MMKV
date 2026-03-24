package com.tencent.mmkv.sample

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.tencent.mmkv.kmp.MMKV

@Composable
fun App() {
    var results by remember { mutableStateOf<List<String>>(emptyList()) }
    val demo = remember { MMKVDemo() }

    MaterialTheme {
        Scaffold(
            topBar = {
                @OptIn(ExperimentalMaterial3Api::class)
                TopAppBar(
                    title = {
                        Column {
                            Text("MMKV KMP Sample")
                            Text(
                                "MMKV Version: ${MMKV.version()}",
                                fontSize = 12.sp,
                            )
                        }
                    },
                    colors = TopAppBarDefaults.topAppBarColors(
                        containerColor = MaterialTheme.colorScheme.primaryContainer,
                    )
                )
            }
        ) { padding ->
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(padding)
                    .padding(horizontal = 16.dp, vertical = 8.dp),
            ) {
                // Test buttons row
                Column(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(6.dp),
                ) {
                    Button(
                        onClick = { results = demo.functionalTest() },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text("Functional Test")
                    }
                    Button(
                        onClick = { results = demo.testReKey() },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text("Encryption Test")
                    }
                    Button(
                        onClick = { results = demo.testAutoExpire() },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text("Auto Expiration Test")
                    }
                    Button(
                        onClick = { results = demo.testCompareBeforeSet() },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text("Compare Before Set Test")
                    }
                    Button(
                        onClick = { results = demo.testBackupAndRestore() },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text("Backup & Restore Test")
                    }
                    Button(
                        onClick = { results = demo.testRemoveStorageAndCheckExist() },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text("Remove Storage & Check Exist")
                    }
                    Button(
                        onClick = { results = demo.testNameSpace() },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text("NameSpace Test")
                    }
                }

                Spacer(modifier = Modifier.height(8.dp))

                // Results output
                if (results.isNotEmpty()) {
                    Card(
                        modifier = Modifier
                            .fillMaxWidth()
                            .weight(1f),
                    ) {
                        Column(
                            modifier = Modifier
                                .padding(12.dp)
                                .verticalScroll(rememberScrollState())
                        ) {
                            results.forEach { line ->
                                Text(
                                    text = line,
                                    fontSize = 12.sp,
                                    fontFamily = FontFamily.Monospace,
                                    modifier = Modifier.padding(vertical = 1.dp)
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}
