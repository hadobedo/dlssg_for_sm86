#!/usr/bin/env bash
# Install (or remove) the Wine/Proton compatibility shim for this mod.
#
#   ./install.sh --prefix <wine-prefix> [--game-dir <dir>] [--uninstall]
#
#   --prefix     Wine/Proton prefix directory: the folder that contains
#                drive_c, e.g.
#                ~/.steam/steam/steamapps/compatdata/<AppID>/pfx
#   --game-dir   Optional: copy the mod's version.dll and dlssg_sm86.ini into
#                the game's rendering-EXE folder.
#   --uninstall  Put the original version.dll back and delete the shim.
#
# The script only touches files; it never runs wine. The one thing it cannot do
# for you is the DLL override, which has to be set per prefix:
#   protontricks <AppID> winecfg   ->  Libraries: version = native,builtin
# or, with a wine binary available:
#   WINEPREFIX=<prefix> wine reg add \
#     'HKCU\Software\Wine\DllOverrides' /v version /d 'native,builtin' /f
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
prefix='' game_dir='' uninstall=false

usage() { sed -n '2,18p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }

while [[ $# -gt 0 ]]; do
    case $1 in
        --prefix)    prefix=${2:-}; shift 2 ;;
        --game-dir)  game_dir=${2:-}; shift 2 ;;
        --uninstall) uninstall=true; shift ;;
        -h|--help)   usage 0 ;;
        *) echo "error: unknown argument: $1" >&2; usage 1 ;;
    esac
done

[[ -n $prefix ]] || { echo "error: --prefix is required" >&2; usage 1; }
prefix=${prefix%/}
[[ -d $prefix/drive_c ]] || { echo "error: $prefix does not look like a Wine prefix (no drive_c)" >&2; exit 1; }

sys32=$prefix/drive_c/windows/system32
[[ -f $sys32/version.dll ]] || { echo "error: $sys32/version.dll not found" >&2; exit 1; }

if $uninstall; then
    if [[ -f $sys32/version_orig.dll ]]; then
        rm -f -- "$sys32/version.dll"
        mv -- "$sys32/version_orig.dll" "$sys32/version.dll"
        echo "restored $sys32/version.dll"
    else
        echo "nothing to undo: $sys32/version_orig.dll not present"
    fi
    cat <<EOF

Remove the DLL override when no other game in this prefix needs it:
  Libraries: version -> (blank), or delete HKCU\\Software\\Wine\\DllOverrides\\version
Also delete version.dll and dlssg_sm86.ini from the game folder if you copied them.
EOF
    exit 0
fi

first_file() { local f; for f in "$@"; do [[ -f $f ]] && { printf '%s' "$f"; return 0; }; done; return 1; }

# Works both in a release bundle (game-dir/, system32/) and in a source
# checkout (repo root, linux/out/ after build.sh).
mod_dll=$(first_file "$here/game-dir/version.dll" "$here/../version.dll" "$here/version.dll") \
    || { echo "error: cannot find the mod's version.dll next to $here" >&2; exit 1; }
mod_dir=$(dirname -- "$mod_dll")
shim=$(first_file "$here/system32/version.dll" "$here/out/version.dll") \
    || { echo "error: cannot find the shim; run linux/build.sh first" >&2; exit 1; }

if [[ ! -f $sys32/version_orig.dll ]]; then
    # Wine's version.dll is often a symlink into the Proton install; -L copies
    # the real bytes, which is what the shim loads at runtime.
    cp -L -- "$sys32/version.dll" "$sys32/version_orig.dll"
    echo "kept Wine's implementation as $sys32/version_orig.dll"
fi
cp -- "$shim" "$sys32/version.dll"
echo "installed the shim as $sys32/version.dll"

if [[ -n $game_dir ]]; then
    [[ -d $game_dir ]] || { echo "error: --game-dir $game_dir is not a directory" >&2; exit 1; }
    stamp=$(date +%Y%m%d-%H%M%S)
    for f in version.dll dlssg_sm86.ini; do
        src=$mod_dir/$f
        [[ -f $src ]] || continue
        if [[ -f $game_dir/$f ]] && ! cmp -s -- "$src" "$game_dir/$f"; then
            mv -- "$game_dir/$f" "$game_dir/$f.bak-$stamp"
            echo "backed up existing $f to $f.bak-$stamp"
        fi
        cp -- "$src" "$game_dir/$f"
        echo "copied $f to $game_dir"
    done
fi

cat <<EOF

Next: set the DLL override for this prefix (once), or the proxy's builtin
version.dll wins and frame generation will not load:
  protontricks <AppID> winecfg     -> Libraries: version = native,builtin

Then start the game and check <render-exe-dir>/dlssg_sm86/logs/ for a
loader_*.jsonl that reports "runtime_redirect".
EOF
