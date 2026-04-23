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

import org.gradle.internal.os.OperatingSystem

plugins {
    kotlin("multiplatform")
    kotlin("native.cocoapods")
    id("com.android.library")
}

// Single source of truth: read the MMKV version from gradle.properties.
val mmkvVersion: String = (project.findProperty("MMKV_VERSION") as? String) ?: "2.4.0"

// ---------------------------------------------------------------------------
// Native-library build wiring (Linux / Windows / JVM desktop)
//
// The nativeInterop/CMakeLists.txt builds:
//   * libmmkv-kmp.a           — static archive linked into Kotlin/Native klibs
//   * libmmkv-kmp.{dylib,so,dll} — shared library consumed by JNA on JVM desktop
//
// Historically consumers had to invoke CMake manually before running Gradle,
// which made `./gradlew :mmkv:build` silently misconfigure cinterop on a fresh
// checkout. We now register a Gradle Exec task per native target so CMake runs
// automatically as part of the build.
// ---------------------------------------------------------------------------

val nativeInteropDir = project.file("nativeInterop")
val nativeBuildRoot = nativeInteropDir.resolve("build")

/** Per-target CMake build directory. */
fun cmakeBuildDirForTarget(targetName: String): java.io.File =
    nativeBuildRoot.resolve(targetName)

/** The Gradle task name used to build the native library for the given target. */
fun cmakeTaskName(targetName: String): String = "cmakeBuild${targetName.replaceFirstChar { it.uppercase() }}"

/**
 * Register an Exec task that runs `cmake -S nativeInterop -B nativeInterop/build/<target>`
 * and builds the `mmkv-kmp` target. Returns the TaskProvider so callers can
 * wire `dependsOn`.
 */
fun registerCMakeTask(targetName: String, extraConfigureArgs: List<String> = emptyList()) =
    tasks.register<Exec>(cmakeTaskName(targetName)) {
        group = "build"
        description = "Build libmmkv-kmp for $targetName via CMake"
        val buildDir = cmakeBuildDirForTarget(targetName)
        inputs.file(nativeInteropDir.resolve("CMakeLists.txt"))
        inputs.file(nativeInteropDir.resolve("cinterop/mmkv.def"))
        outputs.dir(buildDir)
        doFirst { buildDir.mkdirs() }
        workingDir = nativeInteropDir
        val configureCmd = mutableListOf(
            "cmake",
            "-S", ".",
            "-B", buildDir.absolutePath,
            "-DCMAKE_BUILD_TYPE=Release",
            "-DMMKV_VERSION=v$mmkvVersion",
        ).apply { addAll(extraConfigureArgs) }
        // Split into two shell invocations: configure, then build.
        commandLine("sh", "-c", buildString {
            append(configureCmd.joinToString(" ") { "'$it'" })
            append(" && cmake --build '")
            append(buildDir.absolutePath)
            append("' --config Release")
        })
    }

kotlin {
    // ---- Android ----
    androidTarget {
        compilerOptions {
            jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_11)
        }
    }

    // ---- JVM desktop ----
    jvm("desktop") {
        compilerOptions {
            jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_11)
        }
    }

    // ---- iOS ----
    iosArm64()
    iosSimulatorArm64()
    iosX64()

    // ---- macOS ----
    macosArm64()
    macosX64()

    // ---- Linux & Windows native desktop ----
    linuxX64()
    linuxArm64()
    mingwX64()

    // Intermediate source sets:
    //   darwinMain = iosMain + macosMain (CocoaPods MMKV framework)
    //   nativeDesktopMain = linuxX64 + linuxArm64 + mingwX64 (C bridge via cinterop)
    applyDefaultHierarchyTemplate {
        common {
            group("darwin") {
                group("ios")
                group("macos")
            }
            group("nativeDesktop") {
                withLinuxX64()
                withLinuxArm64()
                withMingwX64()
            }
        }
    }

    // ---- Native desktop cinterop + link wiring ----
    val nativeDesktopTargets = listOf(linuxX64(), linuxArm64(), mingwX64())
    nativeDesktopTargets.forEach { target ->
        val targetName = target.targetName
        val cmakeTask = registerCMakeTask(targetName)
        val buildDir = cmakeBuildDirForTarget(targetName)

        target.compilations.getByName("main") {
            cinterops {
                val mmkv by creating {
                    defFile("nativeInterop/cinterop/mmkv.def")
                    // Header directory is only known after CMake runs (it's
                    // written to cbridge_include_dir.txt). Resolve it lazily:
                    // this closure is invoked during task execution, after
                    // cmakeBuildTask has produced the file.
                    val includeHintFile = buildDir.resolve("cbridge_include_dir.txt")
                    compilerOpts(
                        providers.provider {
                            if (includeHintFile.exists()) {
                                listOf("-I${includeHintFile.readText().trim()}")
                            } else {
                                emptyList()
                            }
                        }.get()
                    )
                }
            }
        }
        target.binaries.all {
            linkerOpts("-L${buildDir.absolutePath}", "-lmmkv-kmp")
        }

        // Make every cinterop / link / compile task depend on the CMake build
        // so `./gradlew :mmkv:build` Just Works.
        tasks.matching { it.name.contains(targetName, ignoreCase = true) }.configureEach {
            if (name.startsWith("cinterop") ||
                name.startsWith("link") ||
                name.startsWith("compileKotlin")
            ) {
                dependsOn(cmakeTask)
            }
        }
    }

    // ---- CocoaPods (Darwin) ----
    cocoapods {
        summary = "MMKV Kotlin Multiplatform wrapper"
        homepage = "https://github.com/Tencent/MMKV"
        version = mmkvVersion
        ios.deploymentTarget = "13.0"
        osx.deploymentTarget = "10.15"

        pod("MMKV") {
            version = mmkvVersion
        }
    }

    // ---- Source sets ----
    sourceSets {
        commonTest {
            dependencies {
                implementation(kotlin("test"))
            }
        }
        androidMain {
            dependencies {
                implementation("com.tencent:mmkv:$mmkvVersion")
            }
        }
        val desktopMain by getting {
            dependencies {
                implementation("net.java.dev.jna:jna:5.17.0")
            }
        }
    }
}

// ---- JVM desktop: build the shared library for the host and bundle it into
//      the JAR under resources/native/<os-arch>/ so consumers can extract it
//      at runtime. Runtime extraction itself is out of scope for this pass
//      (documented in KMP/README.md) — we bundle the binary so it ships, and
//      consumers still set java.library.path until extraction lands. ----
val hostTargetName: String = when {
    OperatingSystem.current().isMacOsX -> "macos"
    OperatingSystem.current().isLinux -> "linux"
    OperatingSystem.current().isWindows -> "windows"
    else -> "unknown"
}
val hostCmakeTask = registerCMakeTask("desktop${hostTargetName.replaceFirstChar { it.uppercase() }}Shared")

tasks.named("desktopProcessResources") {
    dependsOn(hostCmakeTask)
    doLast {
        val hostBuildDir = cmakeBuildDirForTarget("desktop${hostTargetName.replaceFirstChar { it.uppercase() }}Shared")
        val libName = when {
            OperatingSystem.current().isMacOsX -> "libmmkv-kmp.dylib"
            OperatingSystem.current().isLinux -> "libmmkv-kmp.so"
            OperatingSystem.current().isWindows -> "mmkv-kmp.dll"
            else -> return@doLast
        }
        val lib = hostBuildDir.resolve(libName)
        if (!lib.exists()) {
            logger.warn("Native shared library $lib not found after CMake build; skipping resource bundling.")
            return@doLast
        }
        val outputDir = layout.buildDirectory
            .dir("processedResources/desktop/main/native/$hostTargetName-${System.getProperty("os.arch")}")
            .get().asFile
        outputDir.mkdirs()
        lib.copyTo(outputDir.resolve(libName), overwrite = true)
    }
}

// Point the JVM-desktop test task at the freshly-built host shared library so
// tests can load it via JNA without the user setting -Djna.library.path.
tasks.withType<Test>().configureEach {
    if (name == "desktopTest") {
        dependsOn(hostCmakeTask)
        val hostBuildDir = cmakeBuildDirForTarget("desktop${hostTargetName.replaceFirstChar { it.uppercase() }}Shared")
        systemProperty("jna.library.path", hostBuildDir.absolutePath)
    }
}

android {
    namespace = "com.tencent.mmkv.kmp"
    compileSdk = 35

    defaultConfig {
        minSdk = 23
        consumerProguardFiles("consumer-rules.pro")
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}
