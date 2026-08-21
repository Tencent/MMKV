#!/usr/bin/env bash

# Deterministic repository cleanup. This script uses no network or AI service.
set -euo pipefail

mode=dry-run
include_verifier=0
include_python_venv=0

usage() {
    cat <<'USAGE'
Usage: Scripts/clean-generated.sh [--dry-run | --apply] [options]

Modes:
  --dry-run                  List removable generated artifacts (default).
  --apply                    Permanently remove the listed artifacts.

Options:
  --include-vm-verifier      Also remove build/windows-vm-verify.
  --include-python-venv      Also remove Python/venv.
  -h, --help                 Show this help.

Tracked files are never removed. Local properties, signing configuration,
IDE project settings, and the VM verifier are preserved by default.
USAGE
}

while (($#)); do
    case "$1" in
        --dry-run)
            mode=dry-run
            ;;
        --apply)
            mode=apply
            ;;
        --include-vm-verifier)
            include_verifier=1
            ;;
        --include-python-venv)
            include_python_venv=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'Unknown option: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

script_dir=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
repo_root=$(git -C "$script_dir" rev-parse --show-toplevel 2>/dev/null || true)
if [[ -z "$repo_root" ]]; then
    printf 'Refusing to run: %s is not inside a Git repository.\n' "$script_dir" >&2
    exit 2
fi
repo_root=$(CDPATH= cd -- "$repo_root" && pwd -P)
if [[ "$script_dir" != "$repo_root/Scripts" ]]; then
    printf 'Refusing to run: place this script in the repository Scripts directory.\n' >&2
    printf 'Script directory: %s\nExpected directory: %s\n' "$script_dir" "$repo_root/Scripts" >&2
    exit 2
fi

# Keep this list explicit. Do not replace it with a broad git clean or a
# recursive search for every directory named "build".
candidates=(
    'Win32/.vs'
    'Win32/Debug'
    'Win32/Release'
    'Win32/x64'
    'Win32/Win32Demo/Debug'
    'Win32/Win32Demo/x64'
    'Win32/Win32DemoProcess/Debug'
    'Win32/Win32DemoProcess/x64'
    'iOS/MMKVDemo/DerivedData'
    'KMP/.gradle'
    'KMP/.kotlin'
    'KMP/KMP'
    'KMP/build'
    'KMP/mmkv/build'
    'KMP/mmkv/nativeInterop/build'
    'KMP/sample/composeApp/build'
    'KMP/sample/iosApp/Pods'
    'Android/MMKV/.gradle'
    'Android/MMKV/build'
    'Android/MMKV/mmkv/.cxx'
    'Android/MMKV/mmkv/.gradle'
    'Android/MMKV/mmkv/build'
    'Android/MMKV/mmkvannotation/build'
    'Android/MMKV/mmkvdemo/.cxx'
    'Android/MMKV/mmkvdemo/build'
    'flutter/build'
    'flutter/mmkv/.dart_tool'
    'flutter/mmkv/build'
    'flutter/mmkv/example/.dart_tool'
    'flutter/mmkv/example/build'
    'flutter/mmkv/example/ios/DerivedData'
    'flutter/mmkv/example/ios/build'
    'flutter/mmkv/example/ios/.symlinks'
    'flutter/mmkv/example/ios/Flutter/ephemeral/Packages'
    'flutter/mmkv/example/macos/DerivedData'
    'flutter/mmkv/example/macos/Flutter/ephemeral'
    'flutter/mmkv/example/linux/flutter/ephemeral'
    'flutter/mmkv/example/windows/flutter/ephemeral'
    'flutter/mmkv_android/.dart_tool'
    'flutter/mmkv_android/android/build'
    'flutter/mmkv_android/build'
    'flutter/mmkv_ios/.dart_tool'
    'flutter/mmkv_ios/darwin/mmkv_ios/.build'
    'flutter/mmkv_linux/.dart_tool'
    'flutter/mmkv_ohos/.dart_tool'
    'flutter/mmkv_ohos/ohos/build'
    'flutter/mmkv_ohos/ohos/oh_modules'
    'flutter/mmkv_platform_interface/.dart_tool'
    'flutter/mmkv_win32/.dart_tool'
    'POSIX/build'
    'POSIX/cmake-build-debug'
    'POSIX/cmake-build-release'
    'POSIX/golang/CMakeFiles'
    'POSIX/golang/Core'
    'POSIX/golang/build'
    'POSIX/golang/tencent.com'
    'POSIX/golang/CMakeCache.txt'
    'POSIX/golang/Makefile'
    'POSIX/golang/cmake_install.cmake'
    'POSIX/golang/install_manifest.txt'
    'POSIX/golang/libmmkv.a'
    'Python/build'
    'Python/cmake-build-debug'
    'Python/dist'
    'Python/mmkv.egg-info'
    'Core/CMakeFiles'
    'Core/Debug'
    'Core/Release'
    'Core/build'
    'Core/include'
    'Core/x64'
    'Core/CMakeCache.txt'
    'Core/cmake_install.cmake'
    'Core/core.vcxproj.user'
    'OpenHarmony/.hvigor'
    'OpenHarmony/MMKV/.cxx'
    'OpenHarmony/MMKV/build'
    'OpenHarmony/MMKV/oh_modules'
    'OpenHarmony/entry/.cxx'
    'OpenHarmony/entry/build'
    'OpenHarmony/entry/oh_modules'
    'OpenHarmony/oh_modules'
)

if ((include_python_venv)); then
    candidates+=('Python/venv')
fi

# Root build children are temporary by convention, except for the persistent
# Windows verification transport that supports the VMware guest workflow.
if [[ -d "$repo_root/build" ]]; then
    while IFS= read -r -d '' build_child; do
        build_name=${build_child##*/}
        if [[ "$build_name" == 'windows-vm-verify' ]] && ((!include_verifier)); then
            continue
        fi
        candidates+=("build/$build_name")
    done < <(find "$repo_root/build" -mindepth 1 -maxdepth 1 -print0)
elif ((include_verifier)); then
    candidates+=('build/windows-vm-verify')
fi

human_kb() {
    awk -v kib="$1" 'BEGIN {
        split("KiB MiB GiB TiB", unit, " ")
        value = kib + 0
        unit_index = 1
        while (value >= 1024 && unit_index < 4) {
            value /= 1024
            unit_index++
        }
        if (unit_index == 1) printf "%d %s", value, unit[unit_index]
        else printf "%.1f %s", value, unit[unit_index]
    }'
}

safe_candidates=()
total_kb=0
for relative_path in "${candidates[@]}"; do
    case "/$relative_path/" in
        /*/../*|/*/./*)
            printf 'Skipping unsafe path: %s\n' "$relative_path" >&2
            continue
            ;;
    esac

    target="$repo_root/$relative_path"
    if [[ ! -e "$target" && ! -L "$target" ]]; then
        continue
    fi

    tracked=$(git -C "$repo_root" ls-files -- "$relative_path")
    if [[ -n "$tracked" ]]; then
        printf 'Skipping path containing tracked files: %s\n' "$relative_path" >&2
        continue
    fi

    size_kb=$(du -sk "$target" | awk '{print $1}')
    total_kb=$((total_kb + size_kb))
    safe_candidates+=("$relative_path")
    printf '%10s  %s\n' "$(human_kb "$size_kb")" "$relative_path"
done

if ((${#safe_candidates[@]} == 0)); then
    printf 'No listed generated artifacts need cleanup.\n'
    exit 0
fi

printf '%10s  TOTAL\n' "$(human_kb "$total_kb")"
if [[ "$mode" == dry-run ]]; then
    printf 'Dry run only. Re-run with --apply to permanently delete these artifacts.\n'
    exit 0
fi

before_kb=$(du -sk "$repo_root" | awk '{print $1}')
for relative_path in "${safe_candidates[@]}"; do
    # Recheck immediately before deletion in case the index changed after the
    # initial scan.
    if [[ -n "$(git -C "$repo_root" ls-files -- "$relative_path")" ]]; then
        printf 'Refusing to remove newly tracked path: %s\n' "$relative_path" >&2
        exit 1
    fi
    rm -rf -- "$repo_root/$relative_path"
done
after_kb=$(du -sk "$repo_root" | awk '{print $1}')
freed_kb=$((before_kb - after_kb))
if ((freed_kb < 0)); then
    freed_kb=0
fi

printf 'Cleanup complete. Reclaimed %s; repository now uses %s.\n' \
    "$(human_kb "$freed_kb")" "$(human_kb "$after_kb")"
