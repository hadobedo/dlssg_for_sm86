#!/usr/bin/env bash
# Build the Wine/Proton compatibility shim: linux/out/version.dll
#
#   ./linux/build.sh [outdir]      # default outdir: linux/out
#
# Requires x86_64-w64-mingw32-gcc (mingw-w64), e.g. on Debian/Ubuntu:
#   sudo apt-get install mingw-w64
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
out=${1:-$here/out}
cc=${CC:-x86_64-w64-mingw32-gcc}

command -v "$cc" >/dev/null || {
    echo "error: $cc not found; install mingw-w64" >&2
    exit 1
}

mkdir -p "$out"

# -nostdlib + our own DllMainCRTStartup (in version_shim.c): the resulting DLL
# imports only KERNEL32, so it does not depend on any CRT/UCRT DLL being
# present in the Wine prefix. -fno-stack-protector keeps hardened toolchains
# (e.g. Fedora's mingw) from pulling in a CRT symbol for __stack_chk_fail.
"$cc" -shared -O2 -fno-stack-protector -nostdlib \
    -Wl,-e,DllMainCRTStartup -Wl,--subsystem,windows \
    -o "$out/version.dll" \
    "$here/version_shim.c" "$here/version_shim.def" -lkernel32

echo "built $out/version.dll ($(stat -c %s "$out/version.dll") bytes)"
