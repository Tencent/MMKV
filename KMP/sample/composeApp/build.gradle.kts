plugins {
    kotlin("multiplatform")
    kotlin("plugin.compose") version "2.3.21"
    id("com.android.application")
    id("org.jetbrains.compose") version "1.11.0"
}

kotlin {
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

    listOf(
        iosArm64(),
        iosSimulatorArm64(),
    ).forEach { target ->
        target.binaries.framework {
            baseName = "ComposeApp"
            isStatic = true
        }
    }

    sourceSets {
        commonMain {
            dependencies {
                implementation(project(":mmkv"))
                implementation("org.jetbrains.compose.runtime:runtime:1.11.0")
                implementation("org.jetbrains.compose.foundation:foundation:1.11.0")
                implementation("org.jetbrains.compose.material3:material3:1.11.0-alpha07")
                implementation("org.jetbrains.compose.ui:ui:1.11.0")
            }
        }
        androidMain {
            dependencies {
                implementation("androidx.activity:activity-compose:1.13.0")
            }
        }
        val desktopMain by getting {
            dependencies {
                implementation(compose.desktop.currentOs)
            }
        }
    }
}

compose.desktop {
    application {
        mainClass = "com.tencent.mmkv.sample.MainKt"
        nativeDistributions {
            packageName = "MMKV KMP Sample"
            packageVersion = "1.0.0"
        }
    }
}

android {
    namespace = "com.tencent.mmkv.sample"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.tencent.mmkv.sample"
        minSdk = 24
        targetSdk = 36
        versionCode = 1
        versionName = "1.0.0"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}
