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
Use one workspace-local repository for both the Android dependency and the KMP
publications. The commands below keep Gradle's project caches under the ignored
KMP build directory without changing `GRADLE_USER_HOME`.

```bash
cd <MMKV checkout>
LOCAL_REPO="$PWD/KMP/build/local-maven"

KMP/gradlew -p KMP :mmkv:clean \
  --project-cache-dir "$PWD/KMP/build/gradle-project-cache"

Android/MMKV/gradlew -p Android/MMKV \
  :mmkv:clean \
  :mmkv:publishDefaultCppReleasePublicationToMavenLocal \
  -Dmaven.repo.local="$LOCAL_REPO" \
  --project-cache-dir "$PWD/KMP/build/android-project-cache"
```

## 3. Test every KMP publication locally

```bash
KMP/gradlew -p KMP \
  :mmkv:assembleAndroidDeviceTest \
  :mmkv:iosX64Test \
  :mmkv:linkDebugTestIosArm64 \
  :mmkv:linkDebugTestIosSimulatorArm64 \
  :mmkv:publishAllPublicationsToLocalTestRepository \
  -PMMKV_LOCAL_REPOSITORY="$LOCAL_REPO" \
  --project-cache-dir "$PWD/KMP/build/gradle-project-cache"
```

Run the common smoke suite on a connected Android device:

```bash
adb devices -l
KMP/gradlew -p KMP :mmkv:connectedAndroidDeviceTest \
  -PMMKV_LOCAL_REPOSITORY="$LOCAL_REPO" \
  --project-cache-dir "$PWD/KMP/build/gradle-project-cache"
```

Before release, also install the sample as an external consumer of
`com.tencent:mmkv-kmp:2.4.1` on physical Android and iOS devices. Opting into
the sample with `MMKV_USE_PUBLISHED=true` disables its unpublished desktop
target and replaces `project(":mmkv")` with the root KMP coordinate:

```bash
KMP/gradlew -p KMP :sample:composeApp:installDebug \
  -PMMKV_INCLUDE_SAMPLE=true \
  -PMMKV_USE_PUBLISHED=true \
  -PMMKV_LOCAL_REPOSITORY="$LOCAL_REPO" \
  --project-cache-dir "$PWD/KMP/build/gradle-project-cache"
```

For iOS, refresh the local sample workspace and prepare the framework before
invoking Xcode. Supplying the Xcode environment explicitly lets Gradle sync the
Compose resources without creating a project-root `.gradle` directory:

```bash
KMP/gradlew -p KMP :sample:composeApp:podInstall \
  -PMMKV_INCLUDE_SAMPLE=true \
  -PMMKV_USE_PUBLISHED=true \
  -PMMKV_LOCAL_REPOSITORY="$LOCAL_REPO" \
  --project-cache-dir "$PWD/KMP/build/gradle-project-cache"

ARCHS=arm64 \
PLATFORM_NAME=iphoneos \
SDK_NAME=iphoneos \
CONFIGURATION=Debug \
TARGET_BUILD_DIR="$PWD/KMP/build/ios-device-framework" \
UNLOCALIZED_RESOURCES_FOLDER_PATH=compose-resources \
KMP/gradlew -p KMP :sample:composeApp:syncFramework \
  -Pkotlin.native.cocoapods.platform=iphoneos \
  -Pkotlin.native.cocoapods.archs=arm64 \
  -Pkotlin.native.cocoapods.configuration=Debug \
  -PMMKV_INCLUDE_SAMPLE=true \
  -PMMKV_USE_PUBLISHED=true \
  -PMMKV_LOCAL_REPOSITORY="$LOCAL_REPO" \
  --project-cache-dir "$PWD/KMP/build/gradle-project-cache"

IOS_DEVICE_UDID="<ID shown by xcodebuild for the connected iPhone>"
xcodebuild \
  -workspace KMP/sample/iosApp/iosApp.xcworkspace \
  -scheme iosApp \
  -configuration Debug \
  -destination "id=$IOS_DEVICE_UDID" \
  -derivedDataPath KMP/build/ios-device-derived \
  OVERRIDE_KOTLIN_BUILD_IDE_SUPPORTED=YES \
  build

xcrun devicectl device install app \
  --device "$IOS_DEVICE_UDID" \
  KMP/build/ios-device-derived/Build/Products/Debug-iphoneos/iosApp.app
xcrun devicectl device process launch \
  --device "$IOS_DEVICE_UDID" \
  --terminate-existing \
  --console \
  com.tencent.mmkvdemo
```

Both sample apps run `verifyMMKVConsumer()` during startup. A successful launch
checks the native version, key/value round trips, namespace access, and
idempotent wrapper close behavior before rendering the UI.

Verify that `KMP/build/local-maven/com/tencent` contains:

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

It also accepts the Android publication credential names
`REPOSITORY_USERNAME` / `REPOSITORY_PASSWORD` and Gradle's file-based signing
properties (`signing.keyId`, `signing.password`, and
`signing.secretKeyRingFile`).

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

Both aggregate publication tasks automatically perform the required Central
Portal handoff after the Maven-style uploads complete. This is necessary
because Gradle's `maven-publish` plugin sends independent `PUT` requests and
does not otherwise signal the end of a deployment.

The handoff defaults to:

```text
POST /manual/upload/defaultRepository/com.tencent?publishing_type=user_managed
```

`user_managed` transfers the deployment to Central Portal for final review.
Override it only when desired:

```bash
# Transfer and automatically release after validation:
-PCENTRAL_PORTAL_PUBLISHING_TYPE=automatic

# Transfer only; continue through the Publisher API:
-PCENTRAL_PORTAL_PUBLISHING_TYPE=portal_api
```

Useful test/configuration properties:

```text
CENTRAL_PORTAL_NAMESPACE=com.tencent
CENTRAL_PORTAL_STAGING_API_URL=https://ossrh-staging-api.central.sonatype.com
CENTRAL_PORTAL_DRY_RUN=true
```

The handoff is attached only to the aggregate `publish`/repository publication
tasks. Publishing a single publication intentionally does not finalize the
implicit repository.

## 6. Verify as an external consumer

After Maven Central synchronization, test a clean project without
`mavenLocal()`:

```kotlin
commonMain.dependencies {
    implementation("com.tencent:mmkv-kmp:2.4.1")
}
```

Build Android, `iosArm64`, `iosSimulatorArm64`, and `iosX64` with
`--refresh-dependencies`.
