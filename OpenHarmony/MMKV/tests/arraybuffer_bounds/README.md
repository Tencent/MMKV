Portable regression tests for the OpenHarmony ArrayBuffer slice checks. The tests compile the actual checked helper and
the production `encodeBytes` / `encodeArray<T>` callbacks and byte-to-ArrayBuffer converter extracted verbatim
from `native_bridge.cpp`.
N-API and MMKV are explicit test doubles; this does not replace an OpenHarmony SDK build or runtime test.

Build in a disposable directory and run separately:

```sh
cmake -S OpenHarmony/MMKV/tests/arraybuffer_bounds -B "$SCRATCH/arraybuffer-bounds"
cmake --build "$SCRATCH/arraybuffer-bounds"
ctest --test-dir "$SCRATCH/arraybuffer-bounds" --output-on-failure
```

ASan and UBSan are enabled by default; set `-DMMKV_TEST_SANITIZERS=OFF` only for a compiler without those sanitizers.
Coverage includes malformed numeric ranges and wrong value types, N-API failures, bounds and overflow-shaped inputs,
typed element size and actual pointer alignment, missing arguments, optional expiration dispatch, empty slices,
zero-copy pointers for valid nonempty slices, no write/preservation on rejection for all five encoders,
and empty/nonempty output ArrayBuffers (including a null backing pointer for zero bytes).
