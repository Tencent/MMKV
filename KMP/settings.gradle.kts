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

include(":mmkv")
// Xcode's generated CocoaPods script passes platform/arch/configuration, but
// does not forward the MMKV_INCLUDE_SAMPLE flag used during pod installation.
val isCocoaPodsBuild = providers.gradleProperty("kotlin.native.cocoapods.platform").isPresent
if (providers.gradleProperty("MMKV_INCLUDE_SAMPLE").orNull == "true" || isCocoaPodsBuild) {
    include(":sample:composeApp")
}
