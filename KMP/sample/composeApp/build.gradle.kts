plugins {
    kotlin("multiplatform")
    kotlin("native.cocoapods")
    kotlin("plugin.compose") version "2.2.20"
    id("com.android.application")
    id("org.jetbrains.compose") version "1.8.1"
}

kotlin {
    androidTarget {
        compilerOptions {
            jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_11)
        }
    }

    iosArm64()
    iosSimulatorArm64()
    iosX64()

    cocoapods {
        summary = "MMKV KMP Sample App"
        homepage = "https://github.com/Tencent/MMKV"
        version = "1.0.0"
        ios.deploymentTarget = "15.0"
        podfile = project.file("../iosApp/Podfile")

        framework {
            baseName = "ComposeApp"
            isStatic = true
        }
    }

    sourceSets {
        commonMain {
            dependencies {
                implementation(project(":mmkv"))
                implementation(compose.runtime)
                implementation(compose.foundation)
                implementation(compose.material3)
                implementation(compose.ui)
            }
        }
        androidMain {
            dependencies {
                implementation("androidx.activity:activity-compose:1.10.1")
            }
        }
    }
}

android {
    namespace = "com.tencent.mmkv.sample"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.tencent.mmkv.sample"
        minSdk = 24
        targetSdk = 35
        versionCode = 1
        versionName = "1.0.0"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}
