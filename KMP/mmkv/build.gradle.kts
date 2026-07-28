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
import org.gradle.api.tasks.bundling.Jar

plugins {
    kotlin("multiplatform")
    id("com.android.kotlin.multiplatform.library")
    id("maven-publish")
    id("signing")
}

val mmkvVersion = (findProperty("MMKV_VERSION") as? String) ?: "2.4.1"
val publishVersion = (findProperty("VERSION_NAME") as? String) ?: mmkvVersion
val baseArtifactId = (findProperty("POM_ARTIFACT_ID") as? String) ?: "mmkv-kmp"
val publishedGroup = (findProperty("GROUP") as? String) ?: "com.tencent"
val isSnapshot = publishVersion.endsWith("-SNAPSHOT")
val sonatypeUsername =
    (findProperty("SONATYPE_NEXUS_USERNAME") ?: findProperty("mavenCentralUsername")) as String?
val sonatypePassword =
    (findProperty("SONATYPE_NEXUS_PASSWORD") ?: findProperty("mavenCentralPassword")) as String?
val signingKey =
    (findProperty("SIGNING_KEY") ?: findProperty("signingInMemoryKey")) as String?
val signingPassword =
    (findProperty("SIGNING_PASSWORD") ?: findProperty("signingInMemoryKeyPassword")) as String?
val mmkvGitRepository = findProperty("MMKV_GIT_REPOSITORY") as? String
val mmkvGitTag = findProperty("MMKV_GIT_TAG") as? String
val mmkvGitBranch = findProperty("MMKV_GIT_BRANCH") as? String
val mmkvGitCommit = findProperty("MMKV_GIT_COMMIT") as? String
val mmkvForceFetch = (findProperty("MMKV_FORCE_FETCH") as? String)?.toBooleanStrictOrNull() == true
val mmkvGitRef = mmkvGitCommit?.takeIf { it.isNotBlank() }
    ?: mmkvGitBranch?.takeIf { it.isNotBlank() }
    ?: mmkvGitTag?.takeIf { it.isNotBlank() }

// Keep this project's internal coordinates distinct from the published Android
// dependency (`com.tencent:mmkv`) so same-build resolution never substitutes the
// native Android AAR with this KMP wrapper project.
group = "$publishedGroup.kmpbuild"
version = publishVersion

val nativeInteropDir = project.file("nativeInterop")
val nativeBuildRoot = nativeInteropDir.resolve("build")

fun taskSuffix(label: String): String =
    label.split('-', '_')
        .filter { it.isNotBlank() }
        .joinToString("") { token -> token.replaceFirstChar { it.uppercase() } }

fun cmakeBuildDirFor(label: String) = nativeBuildRoot.resolve(label)

fun registerCMakeBuildTask(
    label: String,
    cmakeTarget: String = "mmkv-kmp",
    extraConfigureArgs: List<String> = emptyList(),
): TaskProvider<Exec> {
    val buildDir = cmakeBuildDirFor(label)
    val configureTask = tasks.register<Exec>("cmakeConfigure${taskSuffix(label)}") {
        group = "build"
        description = "Configure CMake for $label"

        inputs.file(nativeInteropDir.resolve("CMakeLists.txt"))
        inputs.file(nativeInteropDir.resolve("cinterop/mmkv.def"))
        outputs.file(buildDir.resolve("CMakeCache.txt"))
        outputs.file(buildDir.resolve("include/MMKVBridge.h"))

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
                if (!mmkvGitRepository.isNullOrBlank()) {
                    add("-DMMKV_GIT_REPOSITORY=$mmkvGitRepository")
                }
                if (!mmkvGitRef.isNullOrBlank()) {
                    add("-DMMKV_GIT_TAG=$mmkvGitRef")
                }
                if (mmkvForceFetch) {
                    add("-DMMKV_FORCE_FETCH=ON")
                }
                addAll(extraConfigureArgs)
            }
        )
    }

    return tasks.register<Exec>("cmakeBuild${taskSuffix(label)}") {
        group = "build"
        description = "Build native MMKV artifact for $label via CMake"
        dependsOn(configureTask)

        inputs.file(nativeInteropDir.resolve("CMakeLists.txt"))
        inputs.file(nativeInteropDir.resolve("cinterop/mmkv.def"))
        outputs.file(buildDir.resolve("libmmkv-kmp.a"))
        // Let CMake perform its own incremental source check. Gradle cannot
        // reliably model FetchContent/local Core source inputs here.
        outputs.upToDateWhen { false }

        workingDir = nativeInteropDir
        commandLine(
            buildList {
                add("cmake")
                add("--build")
                add(buildDir.absolutePath)
                add("--config")
                add("Release")
                add("--target")
                add(cmakeTarget)
            }
        )
    }
}

fun publicationArtifactId(publicationName: String): String = when (publicationName) {
    "kotlinMultiplatform" -> baseArtifactId
    "android", "androidRelease" -> "$baseArtifactId-android"
    else -> "$baseArtifactId-${publicationName.lowercase()}"
}

fun pomName(publicationName: String): String = when (publicationName) {
    "kotlinMultiplatform" -> "MMKV Kotlin Multiplatform"
    else -> "MMKV Kotlin Multiplatform ($publicationName)"
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

val javadocJar = tasks.register<Jar>("javadocJar") {
    archiveClassifier.set("javadoc")
    from(rootProject.file("README.md"))
}

val verifySonatypePublication = tasks.register("verifySonatypePublication") {
    group = "publishing"
    description = "Fail early when Maven Central credentials or release signing are missing."
    doLast {
        check(!sonatypeUsername.isNullOrBlank()) {
            "Missing SONATYPE_NEXUS_USERNAME (or mavenCentralUsername)."
        }
        check(!sonatypePassword.isNullOrBlank()) {
            "Missing SONATYPE_NEXUS_PASSWORD (or mavenCentralPassword)."
        }
        if (!isSnapshot) {
            check(!signingKey.isNullOrBlank()) {
                "Missing SIGNING_KEY (or signingInMemoryKey) for a release publication."
            }
        }
    }
}

kotlin {
    withSourcesJar()

    android {
        namespace = "com.tencent.mmkv.kmp"
        compileSdk = 35
        minSdk = 23
        compilerOptions {
            jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_11)
        }
    }

    // Minimal KMP support for v2.4.1: Android + iOS only.
    val darwinTargets = listOf(
        iosArm64(),
        iosSimulatorArm64(),
        iosX64(),
    )

    applyDefaultHierarchyTemplate {
        common {
            // Keep Darwin source in one shared source set while publishing only iOS targets for v2.4.1.
            group("darwin") {
                group("ios")
            }
        }
    }

    val darwinBuildSettings = mapOf(
        "iosArm64" to listOf("iOS", "iphoneos", "arm64", "13.0"),
        "iosSimulatorArm64" to listOf("iOS", "iphonesimulator", "arm64", "13.0"),
        "iosX64" to listOf("iOS", "iphonesimulator", "x86_64", "13.0"),
    )
    darwinTargets.forEach { target ->
        val label = target.targetName
        val settings = darwinBuildSettings.getValue(label)
        val cmakeTask = registerCMakeBuildTask(
            label = label,
            extraConfigureArgs = listOf(
                "-DCMAKE_SYSTEM_NAME=${settings[0]}",
                "-DCMAKE_OSX_SYSROOT=${settings[1]}",
                "-DCMAKE_OSX_ARCHITECTURES=${settings[2]}",
                "-DCMAKE_OSX_DEPLOYMENT_TARGET=${settings[3]}",
                "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
            ),
        )
        val buildDir = cmakeBuildDirFor(label)
        val generatedIncludeDir = buildDir.resolve("include")

        target.compilations.getByName("main") {
            cinterops {
                val mmkv by creating {
                    defFile("nativeInterop/cinterop/mmkv.def")
                    compilerOpts("-I${generatedIncludeDir.absolutePath}")
                    includeDirs(generatedIncludeDir)
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
    }
}


publishing {
    publications.withType<MavenPublication>().configureEach {
        groupId = publishedGroup
        artifactId = publicationArtifactId(name)
        artifact(javadocJar)
        configurePom(name)
    }

    repositories {
        maven {
            name = "localTest"
            url = uri(layout.buildDirectory.dir("local-maven"))
        }

        val releaseRepo = findProperty("RELEASE_REPOSITORY_URL") as? String
        val snapshotRepo = findProperty("SNAPSHOT_REPOSITORY_URL") as? String
        val repoUrl = if (isSnapshot) snapshotRepo else releaseRepo
        if (!repoUrl.isNullOrBlank()) {
            maven {
                name = "sonatype"
                url = uri(repoUrl)
                credentials {
                    username = sonatypeUsername
                    password = sonatypePassword
                }
            }
        }
    }
}

tasks.matching {
    it.name.startsWith("publish") && it.name.endsWith("ToSonatypeRepository")
}.configureEach {
    dependsOn(verifySonatypePublication)
}

signing {
    if (!signingKey.isNullOrBlank()) {
        useInMemoryPgpKeys(signingKey, signingPassword)
        sign(publishing.publications)
    }
}
