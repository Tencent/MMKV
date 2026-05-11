rootProject.name = "MMKV-KMP"

pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

@Suppress("UnstableApiUsage")
dependencyResolutionManagement {
    repositories {
        google()
        mavenCentral()
    }
}

include(":mmkv")
include(":sample:composeApp")

val mmkvAndroidSource = providers.gradleProperty("MMKV_ANDROID_SOURCE").orNull?.lowercase()
val localAndroidBuild = file("../Android/MMKV")
if (mmkvAndroidSource == "local" || (mmkvAndroidSource == null && localAndroidBuild.resolve("settings.gradle").exists())) {
    includeBuild(localAndroidBuild) {
        dependencySubstitution {
            substitute(module("com.tencent:mmkv")).using(project(":mmkv"))
        }
    }
}
