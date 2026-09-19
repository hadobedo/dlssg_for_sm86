Wine/Proton build of the DLSSG-SM86 `version.dll` proxy: upstream's runtime plus the shim
that gets it past Wine's loader.

- upstream version @VERSION@, commit @SHA7@
- shim: @SHIM_NOTE@
- payload hash: `@PAYLOAD@`
- sources hash: `@SOURCE_SHA@`

## Install

1. Unzip the bundle and enter the folder.
2. Copy `game-dir/version.dll` and `game-dir/dlssg_sm86.ini` next to the game's rendering
   EXE. This is the mod, and it is what enables frame generation.
3. If the shim is listed as required above, copy `system32/version.dll` into
   `<prefix>/drive_c/windows/system32/`, after renaming the `version.dll` already there to
   `version_orig.dll`.
4. Set the DLL override once in that prefix: `protontricks <AppID> winecfg`, Libraries,
   `version` = `native,builtin`. Then start the game.

`install.sh` does steps 2 and 3 for you:

    ./install.sh --appid <AppID> --game-dir "<rendering EXE folder>"

Add `--no-shim` to skip step 3, or `--prefix <path>` if the prefix is not in a usual Steam
location. `SHA256SUMS` is uploaded next to the zip.

The mod is [sdli1995/dlssg_for_sm86](https://github.com/@UPSTREAM_REPO@). tB0nE diagnosed the
Wine failure in [issue #10](https://github.com/@UPSTREAM_REPO@/issues/10), and the shim comes
from that work. This release was built by
[the release workflow](https://github.com/@REPO@/blob/main/.github/workflows/proton-release.yml).
Unofficial packaging, not endorsed by the mod's author. Upstream's
`THIRD_PARTY_NOTICES.txt` is kept inside `game-dir/` and has the terms for the runtime.
