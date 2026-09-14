#!/usr/bin/env bash
# Headless Wine regression test for the version.dll shim.
#
#   ./linux/smoke-test.sh [shim.dll]        # default: linux/out/version.dll
#
# Two runs in a throwaway 64-bit prefix, using the upstream proxy from the repo
# root as the "game directory" version.dll:
#   1. control -- without the shim the proxy must fail with Win32 error 1114
#      (ERROR_DLL_INIT_FAILED), which is the bug being fixed;
#   2. fixed   -- with the shim in system32 the proxy must load and its
#      exports must forward to Wine's real version.dll.
#
# No GPU, driver or game is needed: the proxy's DllMain never gets that far,
# and this test only exercises the loader path that used to abort.
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(dirname -- "$here")
shim=${1:-$here/out/version.dll}
proxy=${PROXY:-$repo/version.dll}

[[ -f "$shim" ]]  || { echo "error: $shim not found; run linux/build.sh first" >&2; exit 1; }
[[ -f "$proxy" ]] || { echo "error: upstream proxy not found at $proxy" >&2; exit 1; }

cc=${CC:-x86_64-w64-mingw32-gcc}
command -v "$cc" >/dev/null || { echo "error: $cc not found; install mingw-w64" >&2; exit 1; }

wine_bin=$(command -v wine || command -v wine-stable || command -v wine64 || true)
[[ -n "$wine_bin" ]] || { echo "error: wine not found; install wine (wine64 is enough)" >&2; exit 1; }

work=$(mktemp -d)
cleanup() {
    command -v wineserver >/dev/null && WINEPREFIX="$work/pfx" wineserver -k >/dev/null 2>&1 || true
    rm -rf -- "$work"
}
trap cleanup EXIT

export WINEPREFIX="$work/pfx" WINEARCH=win64 WINEDEBUG=-all
mkdir -p "$work/gamedir"
cp -- "$proxy" "$work/gamedir/version.dll"
"$cc" -O2 -o "$work/gamedir/loadtest.exe" "$here/loadtest.c" -lkernel32

echo "== wine $("$wine_bin" --version 2>/dev/null || echo '?') -- creating a fresh prefix =="
"$wine_bin" wineboot -u >/dev/null 2>&1 || true
# The proxy is only preferred over Wine's builtin if the prefix says so, which
# is exactly what the install instructions require.
"$wine_bin" reg add 'HKCU\Software\Wine\DllOverrides' /v version /d 'native,builtin' /f >/dev/null

run() { ( cd "$work/gamedir" && "$wine_bin" loadtest.exe 2>/dev/null ); }

echo "== 1/2 control: proxy alone, expected to fail with err=1114 =="
if out=$(run); then rc=0; else rc=$?; fi
printf '%s\n' "$out"
if (( rc == 0 )); then
    cat >&2 <<'EOF'
error: the control run passed, so this test can no longer detect the bug.
       Most likely Wine now implements GetFileVersionInfoByHandle, in which
       case the shim may no longer be needed at all -- verify before releasing.
EOF
    exit 1
fi
case "$out" in
    *"err=1114"*) ;;
    *) echo "error: expected DLL init failure (err=1114) without the shim, got the above" >&2; exit 1 ;;
esac

echo "== 2/2 fixed: shim installed into system32 =="
sys32="$WINEPREFIX/drive_c/windows/system32"
cp -L -- "$sys32/version.dll" "$sys32/version_orig.dll"
cp -- "$shim" "$sys32/version.dll"
if out=$(run); then rc=0; else rc=$?; fi
printf '%s\n' "$out"
if (( rc != 0 )); then
    echo "error: the proxy still fails to load with the shim installed (exit $rc)" >&2
    exit 1
fi
case "$out" in
    *PASS*) ;;
    *) echo "error: the proxy loaded but the export/forward assertions failed" >&2; exit 1 ;;
esac

echo "OK: proxy initializes under Wine and its exports forward to Wine's version.dll"
