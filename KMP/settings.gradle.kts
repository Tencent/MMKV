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

dependencyResolutionManagement {
    repositories {
        if (useMavenLocal) {
            mavenLocal()
        }
        google()
        mavenCentral()
    }
}

include(":mmkv")
// Sample app is kept in the repository, but the published v2.4.1 KMP package is Android+iOS only.
// include(":sample:composeApp")
