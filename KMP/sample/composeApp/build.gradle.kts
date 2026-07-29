plugins {
    kotlin("multiplatform")
    kotlin("native.cocoapods")
    kotlin("plugin.compose") version "2.2.20"
    id("com.android.application")
    id("org.jetbrains.compose") version "1.8.1"
}

val usePublishedMMKV = providers.gradleProperty("MMKV_USE_PUBLISHED").orNull == "true"
val mmkvVersion = providers.gradleProperty("VERSION_NAME").getOrElse("2.4.1")

kotlin {
    androidTarget {
        compilerOptions {
            jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_11)
        }
    }

    if (!usePublishedMMKV) {
        // Desktop is available only when the sample consumes the in-tree project.
        jvm("desktop") {
            compilerOptions {
                jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_11)
            }
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
                if (usePublishedMMKV) {
                    implementation("com.tencent:mmkv-kmp:$mmkvVersion")
                } else {
                    implementation(project(":mmkv"))
                }
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
        if (!usePublishedMMKV) {
            val desktopMain by getting {
                dependencies {
                    implementation(compose.desktop.currentOs)
                }
            }
        }
    }
}

if (!usePublishedMMKV) {
    compose.desktop {
        application {
            mainClass = "com.tencent.mmkv.sample.MainKt"
            nativeDistributions {
                packageName = "MMKV KMP Sample"
                packageVersion = "1.0.0"
            }
            jvmArgs("-Djna.library.path=${project(":mmkv").file("nativeInterop/build").absolutePath}")
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
