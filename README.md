# DLSSG-SM86 on Linux (Wine / Proton)

Frame generation for RTX 30-series cards, and for RTX 20-series through the SM75 path, on
Linux. This repository packages the mod for Wine and Proton and publishes a zip for each
upstream release.

The mod is [sdli1995/dlssg_for_sm86](https://github.com/sdli1995/dlssg_for_sm86). Its README
covers supported GPUs, the ini settings, VRAM guidance and Windows installation, and all of
that applies here.

## What is in the zip

```
dlssg-sm86-proton-<version>/
  game-dir/     version.dll, dlssg_sm86.ini and upstream's notices
  system32/     version.dll, the shim
  install.sh    optional installer
```

`SHA256SUMS` is a separate download next to the zip.

## Install

Two files named `version.dll` go in two different places, and they do different jobs.

1. Copy `game-dir/version.dll` and `game-dir/dlssg_sm86.ini` into the folder that holds the
   game's rendering EXE. This is the mod, and it is what enables frame generation.
2. Copy `system32/version.dll` into the Proton prefix at
   `<prefix>/drive_c/windows/system32/`. Rename the `version.dll` already there to
   `version_orig.dll` first so you can put it back. Upstream 0.3.0 to 0.3.3 needs this shim;
   0.3.4 and later start without it.
3. In the same prefix, set the DLL override once. Run `protontricks <AppID> winecfg`, open
   Libraries, and set `version` to `native,builtin`.
4. Start the game. If a `dlssg_sm86/logs/loader_*.jsonl` file appears next to the game EXE,
   the startup crash is gone.

### Using the script instead

If you know the prefix id, which for a Steam game is the AppID, `install.sh` does steps 1
and 2:

```bash
./install.sh --appid 1234567 --game-dir "/path/to/Game/Binaries/Win64"
```

It looks for the prefix under the usual Steam and Flatpak locations. If yours lives
somewhere else, pass `--prefix` with the folder that contains `drive_c`. Add `--no-shim` to
copy only the mod and leave `system32` alone, which is enough on 0.3.4 and later. Run
`./install.sh --help` for the rest, and `./install.sh --uninstall` to put the original
`version.dll` back.

## Why the shim exists

Upstream's `version.dll` forwards the `version.dll` API to the copy in `system32`, and up to
0.3.3 its `DllMain` refused to start unless every export it forwards was present there. Wine
does not implement `GetFileVersionInfoByHandle`, so the game exited with `status c0000142`
before the mod could write its log.

The shim is about 10 KB. It forwards the 16 exports Wine does have to `version_orig.dll` and
adds the missing one as a stub that returns FALSE. A headless Wine run checks both cases on
each release: without the shim the proxy either fails with `err=1114` or starts on its own,
and with the shim installed it starts and forwards.

Games that do not import `version.dll` need one of upstream's other proxies: `winmm`,
`dbghelp`, `dinput8`, `dxgi` or `d3d12`. This fork ships only the `version.dll` shim.

## Releases

[`.github/workflows/proton-release.yml`](.github/workflows/proton-release.yml) checks upstream
every six hours and publishes a release tagged `proton-<upstream version>` when the runtime or
the installer changes. You can also run it by hand from the Actions tab. When upstream ships
a new version, the workflow syncs this repo, builds the shim, runs the checks and uploads the
zip. A rebuild for a version that is already out replaces that release.

[`linux/state.json`](linux/state.json) records the upstream commit, the payload hash and the
shim hashes from the last release. The workflow skips the build when they match.

## Building the shim

```bash
sudo apt install mingw-w64 wine python3-pefile    # Debian and Ubuntu
./linux/build.sh
python3 linux/verify-exports.py version.dll linux/out/version.dll
./linux/smoke-test.sh
```

`smoke-test.sh` makes a throwaway Wine prefix and runs the loader check. Point `PROXY=` at
another copy of the upstream proxy to test a different build.

## Credits

sdli1995 wrote the mod. tB0nE worked out why it crashed under Wine in
[issue #10](https://github.com/sdli1995/dlssg_for_sm86/issues/10) and wrote the first shim,
which the one here is derived from. Unofficial packaging, not endorsed by the mod's author.
