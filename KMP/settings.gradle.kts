rootProject.name = "MMKV-KMP"

pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

@Suppress("UnstableApiUsage")
val useMavenLocal = providers.gradleProperty("MMKV_USE_MAVEN_LOCAL").orNull == "true"
val localRepository = providers.gradleProperty("MMKV_LOCAL_REPOSITORY").orNull

dependencyResolutionManagement {
    repositories {
        if (!localRepository.isNullOrBlank()) {
            maven(localRepository)
        }
        if (useMavenLocal) {
            mavenLocal()
        }
        google()
        mavenCentral()
    }
}

// Repository builds should exercise the Android implementation from this checkout while
// retaining the published coordinates in mmkv/build.gradle.kts and generated metadata.
// Opt out when explicitly validating compatibility with the published Android artifact.
val usePublishedAndroidArtifact =
    providers.gradleProperty("MMKV_USE_PUBLISHED_ANDROID_ARTIFACT").orNull == "true"
if (!usePublishedAndroidArtifact) {
    includeBuild("../Android/MMKV") {
        dependencySubstitution {
            substitute(module("com.tencent:mmkv")).using(project(":mmkv"))
        }
    }
}

include(":mmkv")
if (providers.gradleProperty("MMKV_INCLUDE_SAMPLE").orNull == "true") {
    include(":sample:composeApp")
}
