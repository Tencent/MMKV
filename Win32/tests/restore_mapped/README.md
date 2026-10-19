# Windows restore regression test

Regression test for the Win32 restore path in `Core/MemoryFile_Win32.cpp`.

`MMKV::restoreOneFromDirectory()` restores a destination that has no cached instance by copying
the backup file over the destination file. On Windows, opening that destination with
`CREATE_ALWAYS` fails with `ERROR_USER_MAPPED_FILE` (1224) while another process keeps the
destination mapped, even when the backup and the destination have exactly the same size. The
restore must therefore open the destination without truncating it, and only shrink it when the
copy leaves a larger file.

The test executable starts itself again with `--hold-mapped`; the second process opens the
destination instance and keeps it mapped until the first process has finished restoring. The
restoring process has no cached destination instance, so it takes the by-file-path restore path.
The test covers three cases:

1. an un-mapped destination larger than the backup is truncated to the backup size;
2. an equal-size destination that another process keeps mapped is restored by both
   `restoreOneFromDirectory()` and `restoreAllFromDirectory()`;
3. a mapped destination larger than the backup cannot be truncated (Windows limitation), so the
   restore keeps its size but still copies the payload and CRC meta, and readers see the backup
   payload.

## Build and run

Run from the repository root:

```sh
cmake -S Win32/tests/restore_mapped -B build/restore_mapped -A x64
cmake --build build/restore_mapped --config Release
ctest --test-dir build/restore_mapped -C Release --output-on-failure
```

Use `-A Win32` for a 32-bit build. The test creates its files under `%TEMP%` and removes them
afterwards. Pass a scratch directory as the first argument to use another location; the test
creates and removes only its own subdirectory inside it.

The test is expected to fail on a tree where `copyFileContent(const MMKVPath_t &, const
MMKVPath_t &)` opens the destination with `OpenFlag::Truncate`: `restoreOneFromDirectory()`
returns `false` and the run reports `restoreOneFromDirectory over a mapped equal-size
destination`. It passes once the destination is opened without truncation.

This repository has no CI for Windows builds or tests, so this test is run manually.
