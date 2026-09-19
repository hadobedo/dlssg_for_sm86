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
INI in one download. It polls upstream for new releases and, when the runtime changes,
builds and publishes the matching zip on its own.

## The Linux problem, in one paragraph

Upstream's `version.dll` forwards the real Windows `version.dll` API and its `DllMain`
refuses to initialize unless every export it forwards resolves on the system DLL. One of
them, `GetFileVersionInfoByHandle`, exists in no Wine build, so with upstream 0.3.0–0.3.3 the
game died at startup with `status c0000142` (`STATUS_DLL_INIT_FAILED`) before the mod's own
log file was even created. Changing Proton versions did not help — it is a gap in Wine, not
in a Proton build. The fix is a ~10 KB shim installed as the prefix's `system32\version.dll`:
it forwards the 16 exports Wine *does* implement to Wine's real implementation (kept beside
it as `version_orig.dll`) and adds the missing one as a stub.

Upstream fixed this themselves in **0.3.4**, whose proxy initializes under Wine with no shim
at all. The shim is still required for 0.3.0–0.3.3 and is verified harmless on 0.3.4+, so
every bundle ships it and CI records per release whether it is actually needed
(`shim_required` in [`linux/state.json`](linux/state.json)). Full write-up:
**[LINUX.md](linux/LINUX.md)**.

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

The proxy next to the game is what enables frame generation and is always needed. The
`system32` shim only exists for upstream 0.3.0–0.3.3; on 0.3.4+ pass `--no-shim` to leave the
prefix untouched. `./install.sh --help` lists every option.

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
`dinput8` / `dxgi` / `d3d12` proxy variants for games that do not load `version.dll`. With
0.3.0–0.3.3 all five aborted under Wine with the same `c0000142` error, and the shim only
covers `version.dll`. Upstream 0.3.4 relaxed that check for them as well — measured, they now
load in a headless Wine test — but nothing here verifies them end-to-end, so they stay out of
these bundles. If your game does not import `version.dll`, take them from upstream's release.

## Verifying a download

Every release ships `SHA256SUMS` next to the zip. The shim itself is verified in CI before
the release is published: its export set is checked against the upstream proxy, and a
headless Wine run proves that the proxy either fails with `err=1114` without it
(0.3.0–0.3.3) or initializes on its own (0.3.4+), and that it loads plus forwards correctly
with the shim installed in either case.

## Releases and updating from upstream

[`.github/workflows/wine-release.yml`](.github/workflows/wine-release.yml) polls upstream
**automatically every 6 hours** and publishes a release whenever the runtime changes. It
also runs on demand: **Actions → Wine/Proton release → Run workflow**, or

```bash
gh workflow run wine-release.yml -R <you>/dlssg_for_sm86
```

Each run resolves the newest upstream GitHub release (the `ref` input can pin a branch, tag
or commit instead), then:

1. fetches that upstream ref and extracts `version.dll` + `dlssg_sm86.ini` from it;
2. merges upstream `main` into this fork's `main` (`sync_fork`), so the fork's tree tracks
   upstream while this README stays put;
3. builds the shim, checks its exports against the new proxy, runs the Wine smoke test and
   records whether the shim is required for that upstream version;
4. publishes a release tagged `wine-<upstream version>` (e.g. `wine-0.3.4`, or
   `wine-0.3.4-r2` if the payload or the shim changed without a version bump);
5. records what it built — upstream ref, commit, payload hash, shim hashes, shim
   requirement — in [`linux/state.json`](linux/state.json).

It skips the build when upstream's payload (`version.dll`, `dlssg_sm86.ini`) and the shim
sources are byte-identical to the last recorded release, so documentation-only upstream
commits do not produce release churn. Manual inputs: `ref` (default `latest-release`),
`sync_fork` (default on), `draft` (default off — turn it on to inspect a bundle before
publishing), `force` (release even if nothing changed).

A scheduled poll keeps working because of [`linux/heartbeat.json`](linux/heartbeat.json):
GitHub disables scheduled workflows after 60 days without repository activity, so a
scheduled run with nothing to release refreshes that timestamp at most once a week.

One-time setup in a fresh fork: enable Actions (Actions tab → "I understand my workflows…")
and allow write permissions (Settings → Actions → General → Workflow permissions → *Read and
write*). GitHub disables workflows, including schedules, in forks until Actions are enabled.
If you ever sync manually, run `git config merge.ours.driver true` first — it lets this
README win over upstream's, as declared in [`.gitattributes`](.gitattributes).

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
