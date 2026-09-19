# Running this mod on Linux (Wine / Proton)

This fork packages the upstream mod together with the compatibility fix that makes it
initialize under Wine/Proton, plus the CI that builds and verifies the result and polls
upstream for new releases. See
[`../README.md`](https://github.com/hadobedo/dlssg_for_sm86/blob/main/README.md) for the
project overview.

Everything about the mod itself — what it does, supported GPUs, INI keys, VRAM guidance,
Windows installation — lives upstream and applies unchanged:
[README.en.md](https://github.com/sdli1995/dlssg_for_sm86/blob/main/README.en.md).

## The problem

With upstream **0.3.0 through 0.3.3**, installing the mod's `version.dll` in a Wine/Proton
prefix made the game die at startup. The proxy is a *forwarder*: it re-exports the real
Windows `version.dll` API and passes calls through to the system `version.dll`. Its `DllMain`
verified that **every** export it forwards could be resolved on that system DLL, and one of
them — `GetFileVersionInfoByHandle` — does not exist in any Wine build (checked across
CachyOS Proton, GE-Proton, Valve Proton 8/9/10/Experimental, upstream WineHQ and a
staging-tkg build). `GetProcAddress` returned NULL, `DllMain` returned FALSE, and the loader
reported:

```
Loaded ...\bin\x64\VERSION.dll ... native
Loaded C:\windows\system32\version.dll ... builtin
err:module:loader_init "VERSION.dll" failed to initialize, aborting
Initializing dlls for ...\<Game>.exe failed, status c0000142
```

`c0000142` is `STATUS_DLL_INIT_FAILED`. No `dlssg_sm86/logs` directory was ever created,
because the proxy aborted before its own logger opened. Switching Proton versions did not
help: it is a gap in Wine, not in a particular Proton build.

**Upstream 0.3.4 fixed this** — its proxy initializes under Wine without any shim. The shim
is therefore required only for 0.3.0–0.3.3, and is verified harmless on 0.3.4+, so every
bundle still ships it. CI measures which case applies per release and records it as
`shim_required` in `state.json`; the release notes state it too.

## The fix

`system32/version.dll` in the prefix is replaced by a small shim
(`linux/version_shim.c` here, `shim-source/` in a release bundle, ~10 KB, built by CI):

* the 16 exports Wine does implement are forwarded to Wine's real implementation, which the
  installer keeps beside the shim as `version_orig.dll`;
* `GetFileVersionInfoByHandle` is added as a stub returning FALSE. The proxy of 0.3.0–0.3.3
  only checks that the export *exists*, so the stub is all that is needed to get past the
  check.

This fixes the proxy's initialization; it does not add frame generation. The DLL is linked
with `-nostdlib`, so it imports nothing but `KERNEL32` and does not depend on any CRT/UCRT
DLL existing in the prefix. The shim only ever runs inside the one prefix it is installed
into, and two file deletions undo it (`install.sh --uninstall`, or restore
`version_orig.dll`).

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
The two destinations are different files with different jobs: `game-dir/version.dll` is the
mod and is what actually enables frame generation; `system32/version.dll` is the shim that
lets the proxy initialize. **On 0.3.4+ the `system32` replacement is optional** (upstream
fixed the check) and `--no-shim` skips it entirely; without that flag `install.sh` performs
it either way, which CI verifies is harmless.

Then set the DLL override for that prefix — **once**:

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

Recent Wine already prefers an application-directory `version.dll` over its builtin for a
non-KnownDLL like this one, so the proxy often loads without the override; setting it anyway
is recommended, since it removes any dependence on the build's default and is what the older
Wine/Proton builds where the abort was diagnosed need.

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
releases, and the shim only covers `version.dll`. The 0.3.0–0.3.3 copies abort under Wine
with error 1114 at load, because each demands exports its Wine counterpart does not provide.
Upstream 0.3.4 relaxed the same check the `version.dll` proxy used, so as of 0.3.4 they load
(measured with a headless `LoadLibrary` under Wine 11.17) — but this fork verifies nothing
beyond that for them. If your game does not import `version.dll`, take them from upstream's
release.

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
copies the upstream proxy into a "game directory", and checks that the proxy either fails
with error 1114 without the shim (0.3.0–0.3.3) or initializes on its own (0.3.4+), and that
it loads plus forwards correctly with the shim installed in either case. It ends with
`SMOKE_NEEDS_SHIM=yes|no`, which is the check CI runs before publishing a release. Point
`PROXY=` at another copy of the proxy to test a different build.

## CI and releases

[`.github/workflows/wine-release.yml`](https://github.com/hadobedo/dlssg_for_sm86/blob/main/.github/workflows/wine-release.yml)
polls upstream **every 6 hours** and publishes a release when the runtime changed; it also
runs on demand (`workflow_dispatch`, default `ref=latest-release`). Each run:

1. resolves the newest upstream GitHub release (or the pinned `ref`) and extracts
   `version.dll` + `dlssg_sm86.ini` from it;
2. optionally merges upstream `main` into this fork's `main`;
3. builds the shim, verifies it covers every export the upstream proxy requires, and runs the
   Wine smoke test, recording whether the shim is required for that version;
4. publishes a release tagged `wine-<upstream version>` (`wine-0.3.4`, or `wine-0.3.4-r2`
   when the payload or shim changed without a version bump);
5. records what it built — upstream ref, commit, payload hash, shim hashes, shim requirement —
   in `linux/state.json`.

Runs are idempotent: if the payload hash and the shim source hash both match the recorded
state, the build is skipped (use `force` to override), so documentation-only upstream commits
produce no release churn. A scheduled run with nothing to build refreshes
`linux/heartbeat.json` at most once a week: GitHub disables scheduled workflows after 60 days
without repository activity, and the heartbeat prevents an idle fork from silently going deaf
to upstream releases.
