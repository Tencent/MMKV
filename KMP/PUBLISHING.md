# Publishing MMKV Kotlin Multiplatform

Publish from macOS so all Android and iOS target artifacts are produced by the
same build.

## 1. Release preflight

Confirm that these versions match:

```text
Core/MMKVPredef.h                         v2.4.1
Android/MMKV/gradle.properties            2.4.1
KMP/gradle.properties                     2.4.1
MMKV*.podspec                             2.4.1
```

The release tag `v2.4.1` must exist before publishing because the standalone
KMP native build fallback resolves MMKV Core from that tag.

## 2. Publish the Android dependency locally

The KMP Android target depends on `com.tencent:mmkv:2.4.1`.

```bash
cd Android/MMKV
./gradlew :mmkv:clean :mmkv:publishDefaultCppReleasePublicationToMavenLocal
```

## 3. Test every KMP publication locally

```bash
cd ../../KMP
./gradlew :mmkv:clean \
  :mmkv:linkDebugTestIosArm64 \
  :mmkv:publishAllPublicationsToLocalTestRepository \
  -PMMKV_USE_MAVEN_LOCAL=true
```

Before release, also install an external consumer app that uses
`com.tencent:mmkv-kmp:2.4.1` on a physical iOS device and run the smoke
operations from `MMKVSmokeTest`.

Verify that `mmkv/build/local-maven/com/tencent` contains:

```text
mmkv-kmp
mmkv-kmp-android
mmkv-kmp-iosarm64
mmkv-kmp-iossimulatorarm64
mmkv-kmp-iosx64
```

Each publication must have its main artifact, POM, Gradle module metadata,
sources JAR, javadoc JAR, and checksums. Before the Central upload, also verify
that release signatures are generated.

## 4. Configure credentials

The KMP build reads these Gradle project properties:

```text
SONATYPE_NEXUS_USERNAME
SONATYPE_NEXUS_PASSWORD
SIGNING_KEY
SIGNING_PASSWORD (when the private key is passphrase-protected)
```

Use `ORG_GRADLE_PROJECT_<name>` environment variables or user-level
`~/.gradle/gradle.properties`. Do not commit credentials or signing keys.

The Android build uses:

```text
REPOSITORY_USERNAME
REPOSITORY_PASSWORD
```

and the standard Gradle signing properties.

## 5. Upload Android and KMP artifacts

```bash
cd Android/MMKV
./gradlew :mmkv:publishAllPublicationsToMavenCentralRepository

cd ../../KMP
./gradlew :mmkv:publishAllPublicationsToSonatypeRepository \
  -PMMKV_USE_MAVEN_LOCAL=true
```

The KMP command publishes the root metadata artifact and all target-specific
artifacts. Do not publish them individually.

## 6. Finalize the Central Portal deployment

This repository currently uploads through Sonatype's Portal OSSRH Staging API
compatibility endpoint. After all files are uploaded, finalize the namespace's
default repository:

```bash
curl -u "$ORG_GRADLE_PROJECT_SONATYPE_NEXUS_USERNAME:$ORG_GRADLE_PROJECT_SONATYPE_NEXUS_PASSWORD" \
  -X POST \
  "https://ossrh-staging-api.central.sonatype.com/manual/upload/defaultRepository/com.tencent"
```

If credentials are stored in `~/.gradle/gradle.properties` instead of
environment variables, substitute the corresponding Central Portal username
and token in the `curl` command.

Validate the deployment in Central Portal before selecting **Publish**.
Released Maven Central coordinates are immutable.

## 7. Verify as an external consumer

After Maven Central synchronization, test a clean project without
`mavenLocal()`:

```kotlin
commonMain.dependencies {
    implementation("com.tencent:mmkv-kmp:2.4.1")
}
```

Build Android, `iosArm64`, `iosSimulatorArm64`, and `iosX64` with
`--refresh-dependencies`.
