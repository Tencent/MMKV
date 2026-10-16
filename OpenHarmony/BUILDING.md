# Building and debugging MMKV for OHOS

Run the commands below from `OpenHarmony`, with DevEco's Node, Hvigor, OHPM,
and HDC on `PATH` and `DEVECO_SDK_HOME` pointing to the installed SDK root.
Use the project's profiles and the installed tools' help when versions change.

## Project configuration

| Setting | Value / source |
| --- | --- |
| Product | `default` in `build-profile.json5` |
| Library module | `MMKV` (HAR) |
| App build target | `entry@default` |
| Test build target | `entry@ohosTest` |
| Runtime test module | `entry_test` in `entry/src/ohosTest/module.json5` |
| Bundle | `com.tencent.mmkvdemo` in `AppScope/app.json5` |
| Main ability | `EntryAbility` in `entry/src/main/module.json5` |

The current profile declares compatible SDK `5.0.0(12)` and target SDK
`6.0.0(20)`. These are different from the attached device's API level.
`MMKV/build-profile.json5` selects arm64-v8a and x86_64, static OHOS STL, and
unstripped native symbols. Its CMake build uses C++20 and links Core,
`libace_napi.z.so`, and `libhilog_ndk.z.so` into `libmmkv.so`.

## Build and signing

Install dependencies when needed:

```sh
ohpm install
```

Build the library without signing:

```sh
hvigorw --mode module -p module=MMKV@default -p product=default \
  -p buildMode=debug -p enableSignTask=false assembleHar --no-daemon
```

Build the demo without signing:

```sh
hvigorw --mode module -p module=entry@default -p product=default \
  -p buildMode=debug -p enableSignTask=false assembleHap --no-daemon
```

The root `hvigorfile.ts` imports `sign-config.json` and overrides the signing
material from that file. The empty fields in `build-profile.json5` do not describe
the active identity. Keep signing material private and outside version control.

`enableSignTask=false` disables signing tasks but does not bypass this static
import. If the signing file is missing, configuration fails before compilation.
For a disposable unsigned build without changing the working tree:

1. Copy the current `Core/` and `OpenHarmony/` source trees into a new scratch
   directory at the same relative level. Copying only `OpenHarmony` breaks the
   CMake reference to Core; archiving HEAD would omit uncommitted source changes.
2. Exclude signing material, `sign-config.json`, `.ohpmrc`, `.idea`, `.hvigor`,
   `oh_modules`, and generated `build` directories.
3. Replace only the copy's `OpenHarmony/hvigorfile.ts` with:

   ```ts
   import { appTasks } from '@ohos/hvigor-ohos-plugin';
   export default { system: appTasks, plugins: [] };
   ```

4. Install dependencies in the copy, then run the unsigned build command there.
   This checks compilation with the signing import removed; it does not verify
   the original signed-build configuration.

For an authorized signed build, use the existing signing identity/profile and
omit `-p enableSignTask=false`. Confirm the profile covers the selected physical
device and matches the installed application's identity.

## Install and launch the demo

Select the intended physical device with `hdc list targets -v` and set
`DEVICE_SERIAL` to its exact transport serial. After a successful signed build:

```sh
hdc -t "$DEVICE_SERIAL" install -r \
  entry/build/default/outputs/default/entry-default-signed.hap
```

Confirm the artifact path against the build output. Replacement installation
preserves existing app data; stop on a signing mismatch rather than uninstalling
the app to work around it.

Launch separately when the demo itself is needed:

```sh
hdc -t "$DEVICE_SERIAL" shell aa start \
  -b com.tencent.mmkvdemo -m entry -a EntryAbility
```

`EntryAbility.onCreate()` calls `testNameSpace()` and writes normal demo stores.
Installing the main module is sufficient for preparing the test module; do not
launch the normal demo just to run its isolated tests.

## Tests and native debugging

See the [ArrayBuffer harness guide](MMKV/tests/arraybuffer_bounds/README.md) for
the portable tests, device build/install commands, runner, selected case count,
and test storage cleanup. Keep suite-specific details there.

For native crashes, retain `libmmkv.so` and symbols from the matching build and
collect the relevant application logs/stack. `MMKV/consumer-rules.txt` preserves
exported bridge/API names. The entry app's release obfuscation and the library's
consumer rules are separate settings: changing library obfuscation alone may not
fix a release-only consumer failure.
