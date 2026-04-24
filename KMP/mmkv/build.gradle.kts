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

import org.gradle.api.publish.maven.MavenPublication
import org.gradle.api.tasks.Exec
import org.gradle.api.tasks.Sync
import org.gradle.api.tasks.bundling.Jar

plugins {
    kotlin("multiplatform")
    kotlin("native.cocoapods")
    id("com.android.kotlin.multiplatform.library")
    id("maven-publish")
    id("signing")
}

val mmkvVersion = (findProperty("MMKV_VERSION") as? String) ?: "2.4.0"
val publishVersion = (findProperty("VERSION_NAME") as? String) ?: mmkvVersion
val baseArtifactId = (findProperty("POM_ARTIFACT_ID") as? String) ?: "mmkv-kmp"
val isSnapshot = publishVersion.endsWith("-SNAPSHOT")
val publishedGroup = (findProperty("GROUP") as? String) ?: "com.tencent"

// Keep the build's internal coordinates distinct from the published Android
// dependency (`com.tencent:mmkv`) so Gradle doesn't substitute that AAR with
// this wrapper project during same-build resolution.
group = "$publishedGroup.kmpbuild"
version = publishVersion

val nativeInteropDir = project.file("nativeInterop")
val nativeBuildRoot = nativeInteropDir.resolve("build")
val localCBridgeDir = rootProject.projectDir.parentFile.resolve("Core/cbridge")

fun taskSuffix(label: String): String =
    label.split('-', '_')
        .filter { it.isNotBlank() }
        .joinToString("") { token -> token.replaceFirstChar { it.uppercase() } }

fun cmakeBuildDirFor(label: String) = nativeBuildRoot.resolve(label)

fun registerCMakeBuildTask(
    label: String,
    cmakeTarget: String? = null,
    extraConfigureArgs: List<String> = emptyList(),
): TaskProvider<Exec> {
    val buildDir = cmakeBuildDirFor(label)
    // Configure-step task. Writes the CMake cache into `buildDir`. Kept
    // separate from the build step so we can run both via proper Exec
    // tasks instead of calling `project.exec { }` from within a doFirst —
    // the latter is deprecated and slated for removal in Gradle 9.
    val configureTask = tasks.register<Exec>("cmakeConfigure${taskSuffix(label)}") {
        group = "build"
        description = "Configure CMake for $label"

        inputs.file(nativeInteropDir.resolve("CMakeLists.txt"))
        inputs.file(nativeInteropDir.resolve("cinterop/mmkv.def"))
        outputs.file(buildDir.resolve("CMakeCache.txt"))

        doFirst { buildDir.mkdirs() }

        workingDir = nativeInteropDir
        commandLine(
            buildList {
                add("cmake")
                add("-S")
                add(".")
                add("-B")
                add(buildDir.absolutePath)
                add("-DCMAKE_BUILD_TYPE=Release")
                add("-DMMKV_VERSION=v$mmkvVersion")
                addAll(extraConfigureArgs)
            }
        )
    }

    return tasks.register<Exec>("cmakeBuild${taskSuffix(label)}") {
        group = "build"
        description = "Build native MMKV artifacts for $label via CMake"
        dependsOn(configureTask)

        inputs.file(nativeInteropDir.resolve("CMakeLists.txt"))
        inputs.file(nativeInteropDir.resolve("cinterop/mmkv.def"))
        outputs.dir(buildDir)

        workingDir = nativeInteropDir
        commandLine(
            buildList {
                add("cmake")
                add("--build")
                add(buildDir.absolutePath)
                add("--config")
                add("Release")
                if (cmakeTarget != null) {
                    add("--target")
                    add(cmakeTarget)
                }
            }
        )
    }
}

fun hostOsId(): String {
    val osName = System.getProperty("os.name").lowercase()
    return when {
        "mac" in osName || "darwin" in osName -> "macos"
        "win" in osName -> "windows"
        "linux" in osName -> "linux"
        else -> "unknown"
    }
}

fun hostArchId(): String {
    val rawArch = System.getProperty("os.arch").lowercase()
    return when (rawArch) {
        "x86_64", "amd64" -> "x86_64"
        "aarch64", "arm64" -> "arm64"
        else -> rawArch.replace(Regex("[^a-z0-9_]"), "_")
    }
}

fun hostDesktopLibraryName(): String = when (hostOsId()) {
    "macos" -> "libmmkv-kmp.dylib"
    "linux" -> "libmmkv-kmp.so"
    "windows" -> "mmkv-kmp.dll"
    else -> error("Unsupported desktop host: ${System.getProperty("os.name")}")
}

val hostDesktopId = "${hostOsId()}-${hostArchId()}"
val hostPublishableNativeDesktopPublications = when (hostOsId()) {
    "linux" -> when (hostArchId()) {
        "x86_64" -> setOf("LinuxX64")
        "arm64" -> setOf("LinuxArm64")
        else -> emptySet()
    }
    "windows" -> when (hostArchId()) {
        "x86_64" -> setOf("MingwX64")
        else -> emptySet()
    }
    else -> emptySet()
}
val nativeDesktopPublications = setOf("LinuxX64", "LinuxArm64", "MingwX64")
val disabledNativeDesktopPublications = nativeDesktopPublications - hostPublishableNativeDesktopPublications
val disabledNativeDesktopTargetNames = disabledNativeDesktopPublications.map {
    it.replaceFirstChar { char -> char.lowercase() }
}

fun publicationArtifactId(publicationName: String): String = when (publicationName) {
    "kotlinMultiplatform" -> baseArtifactId
    "android", "androidRelease" -> "$baseArtifactId-android"
    "desktopNative" -> "$baseArtifactId-desktop-native-$hostDesktopId"
    else -> "$baseArtifactId-${publicationName.lowercase()}"
}

fun pomName(publicationName: String): String = when (publicationName) {
    "kotlinMultiplatform" -> "MMKV Kotlin Multiplatform"
    "desktopNative" -> "MMKV Kotlin Multiplatform Desktop Native Runtime"
    else -> "MMKV Kotlin Multiplatform (${publicationName})"
}

fun MavenPublication.configurePom(publicationName: String) {
    pom {
        name.set(pomName(publicationName))
        description.set((findProperty("POM_DESCRIPTION") as? String) ?: "Kotlin Multiplatform wrapper for MMKV")
        url.set((findProperty("POM_URL") as? String) ?: "https://github.com/Tencent/MMKV")
        licenses {
            license {
                name.set((findProperty("POM_LICENCE_NAME") as? String) ?: "BSD 3-Clause License")
                url.set((findProperty("POM_LICENCE_URL") as? String) ?: "https://opensource.org/licenses/BSD-3-Clause")
            }
        }
        developers {
            developer {
                id.set((findProperty("POM_DEVELOPER_ID") as? String) ?: "tencent")
                name.set((findProperty("POM_DEVELOPER_NAME") as? String) ?: "Tencent")
            }
        }
        scm {
            url.set((findProperty("POM_SCM_URL") as? String) ?: "https://github.com/Tencent/MMKV")
            connection.set((findProperty("POM_SCM_CONNECTION") as? String) ?: "scm:git:git://github.com/Tencent/MMKV.git")
            developerConnection.set((findProperty("POM_SCM_DEV_CONNECTION") as? String) ?: "scm:git:ssh://git@github.com/Tencent/MMKV.git")
        }
    }
}
val hostDesktopBuildLabel = "desktop-${hostDesktopId}"
val hostDesktopBuildTask = registerCMakeBuildTask(hostDesktopBuildLabel, cmakeTarget = "mmkv-kmp-shared")
val hostDesktopBuildDir = cmakeBuildDirFor(hostDesktopBuildLabel)
val hostDesktopLibrary = hostDesktopBuildDir.resolve(hostDesktopLibraryName())

val desktopNativeResourceDir = layout.buildDirectory.dir("generated/desktopNativeResources/$hostDesktopId")
val syncHostDesktopNative = tasks.register<Sync>("syncHostDesktopNative") {
    dependsOn(hostDesktopBuildTask)
    from(hostDesktopLibrary)
    into(desktopNativeResourceDir.map { it.dir("native/$hostDesktopId") })
}

val javadocJar = tasks.register<Jar>("javadocJar") {
    archiveClassifier.set("javadoc")
}

val desktopNativeJar = tasks.register<Jar>("desktopNativeJar") {
    dependsOn(hostDesktopBuildTask)
    archiveClassifier.set("desktop-native-$hostDesktopId")
    from(hostDesktopLibrary) {
        into("native/$hostDesktopId")
    }
}

kotlin {
    withSourcesJar()

    android {
        namespace = "com.tencent.mmkv.kmp"
        compileSdk = 35
        minSdk = 23
        withHostTest {}
        withDeviceTest {}
        compilerOptions {
            jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_11)
        }
        optimization {
            consumerKeepRules.apply {
                publish = true
                file("consumer-rules.pro")
            }
        }
    }

    jvm("desktop") {
        compilerOptions {
            jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_11)
        }
    }

    iosArm64()
    iosSimulatorArm64()
    iosX64()

    macosArm64()
    macosX64()

    linuxX64()
    linuxArm64()
    mingwX64()

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

    val nativeDesktopTargets = listOf(linuxX64(), linuxArm64(), mingwX64())
    nativeDesktopTargets.forEach { target ->
        val label = target.targetName
        val cmakeTask = registerCMakeBuildTask(label)
        val buildDir = cmakeBuildDirFor(label)
        val includeHintFile = buildDir.resolve("cbridge_include_dir.txt")

        target.compilations.getByName("main") {
            cinterops {
                val mmkv by creating {
                    defFile("nativeInterop/cinterop/mmkv.def")
                    when {
                        localCBridgeDir.exists() -> {
                            compilerOpts("-I${localCBridgeDir.absolutePath}")
                            includeDirs(localCBridgeDir)
                        }
                        includeHintFile.exists() -> {
                            val includeDir = includeHintFile.readText().trim()
                            compilerOpts("-I$includeDir")
                            includeDirs(includeDir)
                        }
                        else -> {
                            logger.warn("Unable to resolve MMKV cbridge headers for ${target.targetName} during configuration.")
                        }
                    }
                    extraOpts("-libraryPath", buildDir.absolutePath)
                }
            }
        }

        tasks.matching { task ->
            task.name.contains(target.targetName, ignoreCase = true) &&
                task.name.startsWith("cinterop")
        }.configureEach {
            dependsOn(cmakeTask)
        }
    }

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
            resources.srcDir(desktopNativeResourceDir)
        }

        val desktopTest by getting {
            dependencies {
                implementation(kotlin("test"))
            }
        }

        val androidHostTest by getting {
            dependencies {
                implementation(kotlin("test"))
            }
        }

        val androidDeviceTest by getting {
            dependencies {
                implementation(kotlin("test"))
                implementation("androidx.test:core:1.7.0")
                implementation("androidx.test:runner:1.7.0")
            }
        }
    }
}

tasks.named("desktopProcessResources") {
    dependsOn(syncHostDesktopNative)
}

tasks.matching { it.name == "desktopTest" }.configureEach {
    dependsOn(syncHostDesktopNative)
}

tasks.configureEach {
    if (disabledNativeDesktopTargetNames.any { targetName -> name.contains(targetName, ignoreCase = true) }) {
        enabled = false
    }
}

publishing {
    publications.withType<MavenPublication>().configureEach {
        groupId = publishedGroup
        artifactId = publicationArtifactId(name)
        if (name != "desktopNative") {
            artifact(javadocJar)
        }
        configurePom(name)
    }

    publications.create<MavenPublication>("desktopNative") {
        groupId = publishedGroup
        version = project.version.toString()
        artifact(desktopNativeJar)
        configurePom(name)
    }

    repositories {
        val releaseRepo = findProperty("RELEASE_REPOSITORY_URL") as? String
        val snapshotRepo = findProperty("SNAPSHOT_REPOSITORY_URL") as? String
        val repoUrl = if (isSnapshot) snapshotRepo else releaseRepo
        if (!repoUrl.isNullOrBlank()) {
            maven {
                name = "sonatype"
                url = uri(repoUrl)
                credentials {
                    username = (findProperty("SONATYPE_NEXUS_USERNAME") ?: findProperty("mavenCentralUsername")) as String?
                    password = (findProperty("SONATYPE_NEXUS_PASSWORD") ?: findProperty("mavenCentralPassword")) as String?
                }
            }
        }
    }
}

signing {
    val signingKey = (findProperty("SIGNING_KEY") ?: findProperty("signingInMemoryKey")) as String?
    val signingPassword = (findProperty("SIGNING_PASSWORD") ?: findProperty("signingInMemoryKeyPassword")) as String?
    if (!signingKey.isNullOrBlank()) {
        useInMemoryPgpKeys(signingKey, signingPassword)
        sign(publishing.publications)
    }
}
