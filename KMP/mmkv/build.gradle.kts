/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2026 THL A29 Limited, a Tencent company.
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

plugins {
    kotlin("multiplatform")
    kotlin("native.cocoapods")
    id("com.android.library")
}

kotlin {
    // Android target
    androidTarget {
        compilerOptions {
            jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_11)
        }
    }

    // JVM desktop target
    jvm("desktop") {
        compilerOptions {
            jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_11)
        }
    }

    // iOS targets
    iosArm64()
    iosSimulatorArm64()
    iosX64()

    // macOS targets
    macosArm64()
    macosX64()

    // Linux & Windows native desktop targets
    linuxX64()
    mingwX64()

    // Use the default hierarchy template which automatically creates
    // appleMain, iosMain, macosMain, etc. intermediate source sets.
    // We add custom groups for darwinMain and nativeDesktopMain.
    applyDefaultHierarchyTemplate {
        common {
            group("darwin") {
                group("ios")
                group("macos")
            }
            group("nativeDesktop") {
                withLinuxX64()
                withMingwX64()
            }
        }
    }

    // Configure cinterop for native desktop targets (Linux & Windows)
    val nativeDesktopTargets = listOf(linuxX64(), mingwX64())
    val cinteropDir = project.file("nativeInterop/cinterop").absolutePath
    val nativeLibDir = project.file("nativeInterop/build").absolutePath
    nativeDesktopTargets.forEach { target ->
        target.compilations.getByName("main") {
            cinterops {
                val mmkv by creating {
                    defFile("nativeInterop/cinterop/mmkv.def")
                    compilerOpts("-I$cinteropDir")
                    includeDirs(cinteropDir)
                }
            }
        }
        target.binaries.all {
            linkerOpts("-L$nativeLibDir", "-lmmkv-kmp")
        }
    }

    // CocoaPods integration for Darwin platforms
    cocoapods {
        summary = "MMKV Kotlin Multiplatform wrapper"
        homepage = "https://github.com/Tencent/MMKV"
        version = "2.4.0"
        ios.deploymentTarget = "13.0"
        osx.deploymentTarget = "10.15"

        pod("MMKV") {
            version = "2.4.0"
        }
    }

    // Source set hierarchy
    sourceSets {
        androidMain {
            dependencies {
                implementation("com.tencent:mmkv:2.4.0")
            }
        }
        val desktopMain by getting {
            dependencies {
                implementation("net.java.dev.jna:jna:5.17.0")
            }
        }
    }
}

android {
    namespace = "com.tencent.mmkv.kmp"
    compileSdk = 35

    defaultConfig {
        minSdk = 23
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}
