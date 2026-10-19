# Windows large-file copy regression

This standalone test exercises `copyFileContent(sourcePath, destinationHandle)`
with a larger destination. It checks a small copy and a copy at the 2 GiB boundary,
including the final file length and bytes at the beginning, middle, and end.

On MSVC x64, converting the source size through the 32-bit `off_t` type makes the
2 GiB offset negative before it reaches the Windows resize operation. The test
fails on the original implementation and passes when the `size_t` value reaches
the resize helper without narrowing. Calling the handle overload exercises the
existing bug independently of the path-based restore changes in PR #1697.

Run from the repository root:

```sh
cmake -S Win32/tests/copy_large_file -B build/copy_large_file -A x64
cmake --build build/copy_large_file --config Release
ctest --test-dir build/copy_large_file -C Release --output-on-failure
```

Use a separate build directory with `-A Win32` for 32-bit coverage.

The test requires sparse-file support and slightly more than 2 GiB of free space
under `%TEMP%`. It writes approximately 2 GiB through the production copy loop;
it is a correctness test, not a benchmark. It creates its own temporary directory
and removes it on success or assertion failure.
