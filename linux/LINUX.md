# Running this mod on Linux (Wine / Proton)

This fork packages the upstream mod together with the compatibility fix that makes it
initialize under Wine/Proton, plus the CI that builds and verifies the result. See
[`../README.md`](https://github.com/hadobedo/dlssg_for_sm86/blob/main/README.md) for the
project overview.

Everything about the mod itself — what it does, supported GPUs, INI keys, VRAM guidance,
Windows installation — lives upstream and applies unchanged:
[README.en.md](https://github.com/sdli1995/dlssg_for_sm86/blob/main/README.en.md).

## The problem

Install the mod's `version.dll` in a Wine/Proton prefix and the game dies at startup. The
proxy is a *forwarder*: it re-exports the real Windows `version.dll` API and passes calls
through to the system `version.dll`. Its `DllMain` verifies that **every** export it
forwards can be resolved on that system DLL, and one of them —
`GetFileVersionInfoByHandle` — does not exist in any Wine build (checked across CachyOS
Proton, GE-Proton, Valve Proton 8/9/10/Experimental, upstream WineHQ and a staging-tkg
build). `GetProcAddress` returns NULL, `DllMain` returns FALSE, and the loader reports:

```
Loaded ...\bin\x64\VERSION.dll ... native
Loaded C:\windows\system32\version.dll ... builtin
err:module:loader_init "VERSION.dll" failed to initialize, aborting
Initializing dlls for ...\<Game>.exe failed, status c0000142
```

`c0000142` is `STATUS_DLL_INIT_FAILED`. No `dlssg_sm86/logs` directory is ever created,
because the proxy aborts before its own logger opens. Switching Proton versions does not
help: it is a gap in Wine, not in a particular Proton build.

## The fix

`system32/version.dll` in the prefix is replaced by a small shim
(`linux/version_shim.c` here, `shim-source/` in a release bundle, ~10 KB, built by CI):

* the 16 exports Wine does implement are forwarded to Wine's real implementation, which the
  installer keeps beside the shim as `version_orig.dll`;
* `GetFileVersionInfoByHandle` is added as a stub returning FALSE. The proxy only checks that
  the export *exists*, so the stub is all that is needed to get past the check.

The DLL is linked with `-nostdlib`, so it imports nothing but `KERNEL32` and does not depend
on any CRT/UCRT DLL existing in the prefix.

Nothing outside the one game prefix is touched, and two file deletions undo it
(`install.sh --uninstall`, or restore `version_orig.dll`).

## Install

Use a release bundle:

```
dlssg-sm86-wine-<version>/
├─ game-dir/          version.dll (upstream proxy) + dlssg_sm86.ini
├─ system32/          version.dll (the shim)
├─ shim-source/       version_shim.c, version_shim.def, build.sh
├─ install.sh         file placement + backups, no wine required
├─ README.md          project overview
├─ LINUX.md           this file
└─ THIRD_PARTY_NOTICES.txt
```

With the game closed:

```bash
./install.sh --prefix ~/.steam/steam/steamapps/compatdata/<AppID>/pfx \
             --game-dir "/path/to/Game/Binaries/Win64"
```

`--prefix` is the folder that contains `drive_c`. `--game-dir` (optional) copies the proxy
DLL and the INI next to the rendering executable, backing up anything it would overwrite.

Then set the DLL override for that prefix — **once** — or Wine's builtin `version.dll` wins
and the proxy is never loaded:

```
protontricks <AppID> winecfg      ->  Libraries: version = native,builtin
```

or, if a `wine` binary is available:

```bash
WINEPREFIX=<prefix> wineserver -k
WINEPREFIX=<prefix> wine reg add 'HKCU\Software\Wine\DllOverrides' \
    /v version /d 'native,builtin' /f
```

Stop the prefix first: a running `wineserver` holds the registry in memory and can overwrite
a manual edit when it exits.

### Manual install (same four steps)

1. In `<prefix>/drive_c/windows/system32`: `cp -L version.dll version_orig.dll` (the original
   is often a symlink into the Proton install; `-L` copies the real bytes), then
   `rm version.dll && cp <bundle>/system32/version.dll .`
2. Copy `<bundle>/game-dir/version.dll` and `dlssg_sm86.ini` beside the game's rendering EXE.
3. Set the override as above.
4. Launch.

## Verify

If your Proton build has an opt-in native CUDA bridge (CachyOS Proton needs
`PROTON_NVIDIA_NVCUDA=1`), make sure it is set, otherwise the driver-connection step fails
even with the shim in place.

A `dlssg_sm86/logs/loader_*.jsonl` next to the game executable means the abort is gone:

| Log entry | Meaning |
|---|---|
| the file exists at all | fixed — the `c0000142` abort is gone |
| `"event":"runtime_redirect"` | the proxy took over from the game's own runtime |
| `"event":"ngx_driver_connected"` | NVIDIA driver reached |
| `"event":"evaluate"` repeating | live frame generation, once enabled in-game |

Set `Logging.Level=2` (or `3`) in `dlssg_sm86.ini` for more detail, and `1` again for normal
use.

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Still `c0000142` after installing the shim | The shim is not being loaded. Check that `system32/version.dll` is the compiled PE file and not a leftover symlink (`file version.dll` should say `PE32+ executable`, not `symbolic link`), and that the override is `version = native,builtin`. |
| `version_orig.dll` missing | Wine's real `version.dll` was not kept. Re-run the installer, or `cp -L` it back yourself. |
| Another export logs "not found" | Upstream added an export the shim does not cover. CI fails loudly for this case (`linux/verify-exports.py`); add a forwarder or stub to `version_shim.c`/`version_shim.def` and re-run. |
| No frame-generation option appears in-game | Game-specific issue that also occurs on native Windows for some titles. Not something the shim addresses. |
| `ngx_driver_connected` never appears | The CUDA driver is not reachable from the game process: check the Proton build's CUDA-bridge launch option, and that the game is not sandboxed away from the NVIDIA libraries. |
| Frame generation works on 2X but the game stutters | VRAM pressure; see the upstream README's VRAM guidance. Unrelated to Wine. |

## What is not covered

**The `alternatives/` proxies (`winmm`, `dbghelp`, `dinput8`, `dxgi`, `d3d12`).** Upstream
ships them for games that do not import `version.dll`. They are not included in these
releases, because they fail on Linux for exactly the same reason: measured under Wine,
each one aborts with `err=1114` at load, since it demands exports that its Wine counterpart
(`winmm`, `dbghelp`, …) does not provide. Fixing them would mean a separate shim per proxy
name, covering 6 to 252 exports each, and none of that has been tested. If your game does
not import `version.dll`, this fork cannot help today.

**Frame generation itself.** The shim only gets the proxy past `DllMain`. Driver
negotiation, the CUDA bridge and the actual generated frames depend on your Proton build,
driver and game, as they do on Windows.

## Building and testing the shim

```bash
sudo apt-get install mingw-w64 wine64 python3-pefile    # Debian/Ubuntu
./linux/build.sh                                        # -> linux/out/version.dll
python3 linux/verify-exports.py version.dll linux/out/version.dll
./linux/smoke-test.sh
```

`smoke-test.sh` runs headlessly, with no GPU or game: it creates a throwaway Wine prefix,
copies the upstream proxy into a "game directory", and checks that it fails with error 1114
without the shim and loads plus forwards correctly with it. That is the same check CI runs
before publishing a release. Point `PROXY=` at another copy of the proxy to test a different
build.

## CI and releases

[`.github/workflows/wine-release.yml`](https://github.com/hadobedo/dlssg_for_sm86/blob/main/.github/workflows/wine-release.yml)
runs on demand only (`workflow_dispatch`) — nothing is pulled from upstream and no release is
created until it is started. Given a `ref` (default: upstream `main`), it:

1. fetches that upstream ref and extracts `version.dll` + `dlssg_sm86.ini` from it;
2. optionally merges upstream `main` into this fork's `main`;
3. builds the shim, verifies it covers every export the upstream proxy requires, and runs the
   Wine smoke test;
4. publishes a release tagged `wine-<upstream version>` (`wine-0.3.0`, or `wine-0.3.0-r2`
   when the payload or shim changed without a version bump);
5. records what it built — upstream ref, commit, payload hash, shim hashes — in
   `linux/state.json`.

Runs are idempotent: if the payload hash and the shim source hash both match the recorded
state, the build is skipped (use `force` to override). Documentation-only upstream commits
therefore produce no release churn.
