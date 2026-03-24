plugins {
    kotlin("multiplatform")
}

kotlin {
    val mmkvLibDir = project(":mmkv").file("nativeInterop/build").absolutePath

    // Linux & Windows native desktop targets
    linuxX64 {
        binaries {
            executable {
                entryPoint = "com.tencent.mmkv.sample.main"
                linkerOpts("-L$mmkvLibDir", "-lmmkv-kmp", "-lstdc++", "-lz")
            }
        }
    }
    mingwX64 {
        binaries {
            executable {
                entryPoint = "com.tencent.mmkv.sample.main"
                linkerOpts("-L$mmkvLibDir", "-lmmkv-kmp", "-lstdc++")
            }
        }
    }

    applyDefaultHierarchyTemplate {
        common {
            group("nativeDesktop") {
                withLinuxX64()
                withMingwX64()
            }
        }
    }

    sourceSets {
        commonMain {
            dependencies {
                implementation(project(":mmkv"))
            }
        }
    }
}
