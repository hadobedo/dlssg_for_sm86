# DLSSG-SM86 on Linux (Wine / Proton)

Frame generation — including multi-frame generation — for RTX 30-series (and RTX 20-series via
the SM75 route) on **Linux**, delivered as a ready-to-install zip for Wine and Proton.

This repository builds on two projects:

| Project | Contribution |
|---|---|
| **[sdli1995/dlssg_for_sm86](https://github.com/sdli1995/dlssg_for_sm86)** | The mod itself: a `version.dll` proxy around the DLSS-G runtime that enables frame generation on Ampere GPUs. All credit for the mod, the runtime and the GPU work goes there. |
| **[tB0nE/dlssg_for_sm86](https://github.com/tB0nE/dlssg_for_sm86)** | Diagnosed why the proxy cannot start under Wine/Proton and wrote the first forwarding-shim workaround — see [upstream issue #10](https://github.com/sdli1995/dlssg_for_sm86/issues/10). |

What this fork adds is packaging and automation: tB0nE's shim, rebuilt and functionally
verified by CI, plus release zips that combine it with the unmodified upstream runtime and
INI in one download.

## The Linux problem, in one paragraph

Upstream's `version.dll` forwards the real Windows `version.dll` API and its `DllMain`
refuses to initialize unless every export it forwards resolves on the system DLL. One of
them, `GetFileVersionInfoByHandle`, exists in no Wine build, so the game dies at startup
with `status c0000142` (`STATUS_DLL_INIT_FAILED`) before the mod's own log file is even
created. Changing Proton versions does not help — it is a gap in Wine, not in a Proton
build. The fix is a ~10 KB shim installed as the prefix's `system32\version.dll`: it
forwards the 16 exports Wine *does* implement to Wine's real implementation (kept beside it
as `version_orig.dll`) and adds the missing one as a stub. Full write-up: **[LINUX.md](linux/LINUX.md)**.

## Install

Download the newest zip from [Releases](../../releases), then, with the game closed:

```bash
unzip dlssg-sm86-wine-*.zip && cd dlssg-sm86-wine-*
./install.sh --prefix ~/.steam/steam/steamapps/compatdata/<AppID>/pfx \
             --game-dir "/path/to/Game/Binaries/Win64"
```

Set the DLL override for that prefix once — `protontricks <AppID> winecfg`, Libraries:
`version` = `native,builtin` — then launch. A `dlssg_sm86/logs/loader_*.jsonl` beside the
game executable means the startup abort is gone.

A release bundle contains:

```
dlssg-sm86-wine-<version>/
├─ game-dir/          version.dll (upstream proxy) + dlssg_sm86.ini
├─ system32/          version.dll (the shim, built from shim-source/)
├─ shim-source/       version_shim.c, version_shim.def, build.sh
├─ install.sh         file placement + backup, no wine required
├─ README.md          this file
├─ LINUX.md           diagnosis, manual steps, troubleshooting
└─ THIRD_PARTY_NOTICES.txt
```

The proxy DLL and INI are upstream's signed, unmodified files. The only thing this fork
adds to the process is the shim.

**`alternatives/` is not shipped here.** Upstream also publishes `winmm` / `dbghelp` /
`dinput8` / `dxgi` / `d3d12` proxy variants for games that do not load `version.dll`. All
five fail under Wine with the same `c0000142` error (measured: each requires exports its
Wine counterpart does not have), and the shim only covers `version.dll`. If a game does not
import `version.dll`, none of the other names will work on Linux today — get them from
upstream if you want to experiment on Windows.

## Verifying a download

Every release ships `SHA256SUMS` next to the zip. The shim itself is verified in CI before
the release is published: the export set is checked against the upstream proxy and a
headless Wine run proves that the proxy fails with `err=1114` without the shim and loads
plus forwards correctly with it.

## Releases and updating from upstream

Releases are built by [`.github/workflows/wine-release.yml`](.github/workflows/wine-release.yml),
**on demand only** — nothing is pulled from upstream automatically, and this fork's `main`
is only updated when you ask for it.

To pull a new upstream version and cut a release: **Actions → Wine/Proton release → Run
workflow**, or:

```bash
gh workflow run wine-release.yml -R <you>/dlssg_for_sm86 \
   -f ref=main          # upstream branch, tag (e.g. 0.3.0) or commit
```

Inputs: `ref` (default `main`), `sync_fork` (merge upstream `main` into this fork, default
on), `draft` (default on — inspect the artifact before publishing), `force` (release even
if nothing changed). A draft becomes public with
`gh release edit wine-<version> --repo <you>/dlssg_for_sm86 --draft=false`.

The workflow then:

1. fetches the requested upstream ref and extracts `version.dll` + `dlssg_sm86.ini` from it;
2. builds the shim, verifies its exports and runs the Wine smoke test;
3. publishes a release tagged `wine-<upstream version>` (e.g. `wine-0.3.0`, or
   `wine-0.3.0-r2` if the payload or the shim changed without a version bump);
4. records what it built in [`linux/state.json`](linux/state.json).

It skips the build when upstream's payload (`version.dll`, `dlssg_sm86.ini`) and the shim
sources are byte-identical to the last recorded release, so documentation-only upstream
commits do not produce release churn.

One-time setup in a fresh fork: enable Actions (Actions tab → "I understand my workflows…")
and allow write permissions (Settings → Actions → General → Workflow permissions → *Read and
write*). If you ever sync manually, run `git config merge.ours.driver true` first — it lets
this README win over upstream's, as declared in [`.gitattributes`](.gitattributes).

## Upstream documentation

The mod's own documentation — supported GPUs, the `dlssg_sm86.ini` keys, VRAM guidance,
Windows installation, anti-cheat/antivirus notes — lives upstream and applies here
unchanged: [README.en.md](https://github.com/sdli1995/dlssg_for_sm86/blob/main/README.en.md)
(English) / [README.md](https://github.com/sdli1995/dlssg_for_sm86/blob/main/README.md)
(中文, upstream's original; this fork's README.md is the English page you are reading).

## Credits

* **sdli1995** — author of the mod this fork packages. Its GPU assets come from
  [Coldwood1026's dlssg_for_sm75](https://github.com/Coldwood1026/dlssg_for_sm75); see
  `THIRD_PARTY_NOTICES.txt`.
* **tB0nE** — root-caused the Wine/Proton `c0000142` failure and built the original shim;
  this fork's shim is derived from that work.

Unofficial, community packaging. Nothing here is written or endorsed by the mod's author.
