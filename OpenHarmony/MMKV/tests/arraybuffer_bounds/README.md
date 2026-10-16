Portable regression tests for the OpenHarmony ArrayBuffer slice checks. The tests compile the actual checked helper and
the production `encodeBytes` / `encodeArray<T>` callbacks and byte-to-ArrayBuffer converter extracted verbatim
from `native_bridge.cpp`.
N-API and MMKV are explicit test doubles; this does not replace an OpenHarmony SDK build or runtime test.

See [the OHOS build guide](../../../BUILDING.md) for the project layout, signing setup, and demo installation.
The portable harness requires Python 3 and a C++20 compiler; `prepare_callbacks.py` extracts the production callbacks.

Run the portable commands from the repository root (the directory containing `Core` and `OpenHarmony`).
Set `SCRATCH` to an existing disposable directory owned by the current session, then build:

```sh
cmake -S OpenHarmony/MMKV/tests/arraybuffer_bounds -B "${SCRATCH:?}/arraybuffer-bounds"
cmake --build "${SCRATCH:?}/arraybuffer-bounds"
```

Run the tests separately:

```sh
ctest --test-dir "${SCRATCH:?}/arraybuffer-bounds" --output-on-failure
```

ASan and UBSan are enabled by default; set `-DMMKV_TEST_SANITIZERS=OFF` only for a compiler without those sanitizers.
Coverage includes malformed numeric ranges and wrong value types, N-API failures, bounds and overflow-shaped inputs,
typed element size and actual pointer alignment, missing arguments, optional expiration dispatch, empty slices,
zero-copy pointers for valid nonempty slices, no write/preservation on rejection for all five encoders,
and empty/nonempty output ArrayBuffers (including a null backing pointer for zero bytes).

The complementary device harness is `entry/src/ohosTest/ets/test/ArrayBufferBounds.test.ets` (relative to
`OpenHarmony`). Its 40 Hypium cases exercise the real N-API callbacks and MMKV storage for all five encoders:
valid nonzero offsets, invalid numeric ranges and bounds, typed alignment, preservation after rejection,
omitted/undefined expiration, expiration-enabled writes, empty buffers, negative zero, and public typed subviews.
It creates unique stores under `filesDir/mmkv_arraybuffer_bounds` and removes only those stores after testing.
Wrong argument types, simulated N-API failures, and null backing pointers remain covered by the portable harness.

`entry/src/ohosTest/ets/test/List.test.ets` registers this suite alongside the separate Ability suite;
40 is the count selected by the `ArrayBufferBounds` class filter, not the entire test module.
The harness initializes MMKV from the ability-delegator application context. Preserve its unique-store isolation
when adding cases; never clear the demo's general storage to reset the tests.

The direct `libmmkv.so` import uses the native type package declared in `entry/oh-package.json5` at
`../MMKV/src/main/cpp/types/libmmkv`. Keep this test dependency instead of widening production APIs for malformed calls.

With DevEco's Node/Hvigor/HDC on `PATH`, `DEVECO_SDK_HOME` configured, and an existing debug signing configuration
valid for the attached physical device, build from `OpenHarmony`:

```sh
ohpm install
```

Build in a separate step:

```sh
hvigorw --mode module -p module=entry@ohosTest -p product=default -p buildMode=debug assembleHap --no-daemon
```

The demo's main module must already be installed with the same signing identity. Select the physical device's
serial explicitly, then install the test module without uninstalling or clearing the demo:

```sh
hdc -t "$DEVICE_SERIAL" install -r entry/build/default/outputs/ohosTest/entry-ohosTest-signed.hap
```

Run the selected suite separately:

```sh
hdc -t "$DEVICE_SERIAL" shell aa test -b com.tencent.mmkvdemo -m entry_test \
  -s unittest OpenHarmonyTestRunner -s class ArrayBufferBounds -s timeout 15000
```

Check the reported test count and failures; a successful HDC exit alone does not establish that the suite passed.
`OpenHarmonyTestRunner` logs `onRun end` after scheduling ability startup, before Hypium completes; this is not a
suite-completion marker. Require the selected suite's completion report and failure/error count, and update the
expected count when its cases change.
