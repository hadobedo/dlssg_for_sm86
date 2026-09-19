# DLSSG SM86 fused build

[中文](INSTALL.md) · **English**

**Copy every file at the package root (`version.dll`, `winmm.dll`, `dbghelp.dll`, `dinput8.dll` and `dlssg_sm86.ini`) next to the game's actual rendering EXE and launch as usual.** There is nothing to pick among the four proxies: whichever one the game loads first runs the mod, the rest go on standby and only forward (see "Several proxies at once" below). No Python and no PowerShell launcher at runtime.

The proxy DLL embeds a matching stock DLSSG runtime (models and pipeline included) and its SM86 backend; the default is `Mode=Bundled`. Whatever DLSSG version the game asks for, this bundled implementation is loaded. On first run the bundled files are extracted to `%LOCALAPPDATA%\DlssgSm86\bundles\<bundle-id>`, verified, then loaded; later runs reuse the cache and repair it automatically when it is damaged.

**The factory INI has only two modes** (see "Normal use" below). Every diagnostic / compatibility / performance-experiment switch is still parsed and supported but is left out of the factory INI; when absent each takes a safe, validated default. The complete list is under "Advanced / diagnostic keys" further down.

## Two build variants (which runtime is embedded)

A build embeds exactly **one** runtime, and `manifest.json`'s `runtime_model` says which one; the INI keys are identical in both variants:

| Variant | Artifact | Embedded runtime | Max generated frames | Notes |
|---|---|---|---|---|
| **310.1** | `dlssg-release-x64.zip` | `nvngx_dlssg.dll` 310.1.0.0 | 3 (4X) | Optimized kernels, cross-kernel fusions and image-kernel patches are all available |
| **310.9** | `dlssg-release-x64-310.9.zip` | `nvngx_dlssg_310.9.1.dll` 310.9.1.0 | 5 (6X) | Native 6X; optimized kernels, cross-kernel fusions and image-kernel patches are all available (63 variant rows: 60 network variants + 3 image patches re-derived against 310.9.1 RVAs). `Router=SM75` now works here too (verified by forward JIT on a 3070 and on a real Turing, RTX 2080 Ti). `HardwareBilinear` is still unavailable |

Build with `powershell -File build.ps1 -RuntimeModel 310.1|310.9` (the 310.9 runtime does not ship with the source; it is read from
`assets/runtime/nvngx_dlssg_310.9.1.dll` by default, or pass `-Runtime3109 <path>`). The factory INI's
`MaxGeneratedFrames=3` (4X) applies directly on both builds; 6X needs the 310.9 build and **you have to set it to 5 yourself** (see below).

## Several proxies at once (four proxies at the package root)

The package root ships four utility proxies: `version.dll`, `winmm.dll`, `dbghelp.dll`, `dinput8.dll`. They embed the same runtime and backend and behave identically; only the file name and the forwarding target differ. **Just copy all of them over; there is nothing to work out about which one the game will load**:

- The first proxy loaded in the process (the one whose DllMain runs first) takes a process-wide named marker and becomes **active**: it installs the `LoadLibrary` hook, reads the INI and loads the backend.
- Proxies of the same family loaded afterwards become **standby**: they still resolve and forward every export of their own system DLL (the forwarding is complete), but **install no hooks, read no INI and write no log**, so there is never a second frame generation and no duplicate cache extraction.
- At `[Logging] Level=2` the loader's `configuration` record carries a `proxies` field such as `{"active":"version.dll","standby":["winmm.dll"]}`, which shows directly who is doing the work this run.
- When uninstalling, delete/restore all four (and `dlssg_sm86.ini`) together.

`dxgi.dll` and `d3d12.dll` sit on the rendering hot path and are load-order sensitive, so they are **not at the root** but in `alternatives\`; copy **one** of them over by hand only when none of the four utility proxies was loaded by the game. See `alternatives\README.md`.

## Where the INI goes and when it takes effect

The configuration file is always named `dlssg_sm86.ini` and sits in the proxy DLL's directory (= the rendering EXE's directory). **Exit the game completely and restart it after editing**; there is no hot reload.

- Boolean switches take `0` or `1`. A line starting with `;` is a comment.
- Enumerated values such as `Mode` are case-insensitive.
- The log and cache directories accept absolute paths; a relative path is based on the proxy DLL / INI directory. A custom path does not expand environment variables such as `%LOCALAPPDATA%` or `%TEMP%`, so write the real path; leave `CacheDirectory` empty to use the system default cache directory.
- **A key written wrongly (an invalid number, a misspelled enum, out of range) costs only that key**: it falls back to its own default, the log gets one `configuration_warning{section,key,value,default,reason}` (visible from Level 1), the mod stays enabled and the other keys are unaffected. For example `MaxGeneratedFrames=99` falls back to `3`, `Router=typo` falls back to `Auto`, and a file at `[Runtime] Path` that is missing or does not match falls back to `Mode=Bundled`.
- `configuration_error` (the whole mod switches off and the game's original DLSSG loading is kept) is now reserved for the case where **the whole configuration cannot be read at all** (out of memory, or `[Logging] Directory` / `[Debug] CaptureDirectory` / `[Runtime] Path` written as a string that cannot form a path). An ordinary typo no longer gets here.
- When the backend does not support a switch the INI asks for (for example `Preset=B` on the 310.1 build, or `[Backends]` pointing at an external backend with the old ABI), the loader **strips those flag bits and installs anyway**, logging one `kernel_selection_unsupported{requested_flags,supported_flags,stripped_flags,effect:"installed without them"}`; that switch does nothing, everything else works as usual.

## Normal use: two modes

The complete factory `dlssg_sm86.ini`:

```ini
[General]
Enabled=1

[FrameGeneration]
Optimized=1
MaxGeneratedFrames=3

[Compatibility]
Preset=Auto

[Logging]
Level=1
Directory=dlssg_sm86\logs

[Runtime]
Mode=Bundled
CacheDirectory=
```

There is only one switch that decides "how frame generation runs": `[FrameGeneration] Optimized`. It is a **consistency tier**, with a single criterion:
**how far the generated image may move from the official NVIDIA runtime**. A higher tier is faster and further from the official image; every other knob follows from the tier,
and a normal user does not need to understand them one by one.

| Tier | Setting | Consistency guarantee | What this tier turns on |
|---|---|---|---|
| **0 stock** | `Optimized=0` | **Bit-identical to the official runtime** | No acceleration at all. **Note this is not "no kernel replacement"**: the SM86 route is still active, and what the backend substitutes are the **stock kernel images** extracted from the runtime (cubin on a real SM86 card, PTX on other architectures), numerically identical to the runtime. It is not the same as `KernelImage=Original` — which matters most on the **310.9 build + Ampere**: 310.9.1's stock image kernels are sm_89 blobs that cannot be created on Ampere at all, so this path is the only way to get "stock numerics" on Ampere. |
| **1 bit-identical (default, recommended)** | `Optimized=1` | **Bit-identical to the official runtime** (measured on two real game captures, 320 images) | Every acceleration that is **provably output-preserving**: 63 rewritten kernel variants + all cross-kernel fusions and the exact image-kernel patches (310.9: P1/P3/P4/P7). Equivalent to `OptimizedKernels=1, ImagePatches=1, SkipRepeatedRealCopy=0, HardwareBilinear=0, ChainBlock0=0, LaunchChains=0, PlainVariant=0, DisableFusions=0`. (`SkipRepeatedRealCopy` is no longer armed by any tier in versions after 0.3.2; write `1` explicitly to use it, see below.) |
| **2 fast (lossy)** | `Optimized=2` | **No longer bit-identical**; the worst measured PSNR is still above roughly 50 dB (two captures) | Tier 1 plus the lossy image-kernel rows that meet the PSNR threshold. **310.9 build only**; the 310.1 build does not have these kernels and falls back to tier 1 automatically, logging one `kernel_selection_unsupported`. |
| **3 fastest (lossy)** | `Optimized=3` | **No longer bit-identical**, the largest image-quality cost | Every lossy acceleration that is still faster: every lossy image-kernel row, plus `HardwareBilinear=1` on the **310.1** build (texture-unit bilinear; the bottom row can differ by 12 LSB when the camera rotates). Variants that are slower or show structural artifacts are permanently retired and are enabled at no tier. |

The tiers are **cumulative and monotone**: a higher tier only adds, it never withdraws an acceleration a lower tier turned on. To override one item individually, use
`ImagePatches` / `SkipRepeatedRealCopy` / `HardwareBilinear` / `ImageApprox` / `ImageApproxMask` from "Advanced / diagnostic keys" below — they are applied after
the tier, so they can switch off something the tier turned on, or turn one item on at a lower tier.

The remaining factory keys:

| Key | Section | Default | Description |
|---|---|---|---|
| `Enabled` | `[General]` | `1` | `1` enables DLSSG redirection, adaptation and frame generation; `0` disables it: the game loads its own DLSSG unchanged (which on Ampere means no frame generation). The proxy still forwards the system DLL's original exports. |
| `Optimized` | `[FrameGeneration]` | `1` | Consistency tier `0`–`3`, see the table above. `[Compatibility] OptimizedKernels` is its **backward-compatible alias** (it takes the same `0`–`3`, so `OptimizedKernels=1` in an old INI is still tier 1): when both are present `Optimized` wins, when only the alias is present the alias is used, and when neither is present the default is tier 0. **A wrong or out-of-range value falls back to tier 1** and logs one `configuration_warning` — whoever wrote this key meant to turn acceleration on and should not be dropped back to stock silently. |
| `MaxGeneratedFrames` | `[FrameGeneration]` | `3` | Ceiling on the reported maximum number of "extra generated frames": `5` = up to 6X, `3` = up to 4X, `2` = 3X, `1` = 2X, `0` = keep whatever the runtime reports. **The actual count is requested by the game.** The factory value is `3` (4X): a game with dynamic MFG runs at whatever ceiling is written here by default, and `5` is too high for most people (public issues #497/#499), so 6X became **a manual edit when you want it** — set this line to `5` on the 310.9 build (the 310.1 build clamps a 5 back to 3 and logs one `limit_clamped`). A game that ships an older 4X Streamline plugin stays at 4X whatever is written here. |
| `Preset` | `[Compatibility]` | `Auto` | DLSS-G render preset (UI recomposition), **310.9 only**. `Auto` lets the game / driver profile decide (default); `A` forces UI recomposition off; `B` forces it on (cleaner HUD/UI inside generated frames) — **but B only takes effect when the game hands DLSS-G both a HUD-less image and a UI plane**, which most games do not, and then it is a no-op. 310.1 ignores this key. |
| `Level` | `[Logging]` | `1` | `0` no logging; `1` errors only; `2` adds configuration, loading and capability information; `3` adds kernel creation, Evaluate and marker information on top. The log files are named `loader_<PID>.jsonl` and `backend_<PID>.jsonl`. |
| `Directory` | `[Logging]` | `dlssg_sm86\logs` | Log directory; a relative path is based on the INI's directory. |
| `Mode` | `[Runtime]` | `Bundled` | Where the runtime comes from. `Bundled` always uses the runtime and matching backend embedded in the proxy (use this for a normal install; no version matching needed). `Auto`/`Pinned` are advanced, see below. |
| `CacheDirectory` | `[Runtime]` | empty | Empty uses `%LOCALAPPDATA%\DlssgSm86\bundles`; a non-empty value is the cache root (relative to the INI's directory). |

A normal install can leave the factory INI alone. Set `Optimized` to `0` to compare against stock numerics, or to `2` for a bit more speed (accepting that the image is
no longer bit-identical); nothing else needs touching.

## Advanced / diagnostic keys (not in the factory INI)

The keys below are **still parsed and supported by the loader** (development, validation, monitoring, capture and the MFG probe all need them); they are simply not in the factory INI. **When absent each takes its safe / best default**, so a normal install does not need to write any of them. To use one, add the section and key to the INI (add a `[SectionName]` line yourself if the section is missing).

> **Note: the defaults below are already the fastest / safest set.** Turning these on **reduces performance** and is only for diagnostics or re-measuring on particular hardware:
> `HardwareBilinear=1`, `ChainBlock0=1`, `LaunchChains=1`, `PlainVariant>0`, `DisableFusions>0`, `ForceGeneratedFrames>0`.

### [FrameGeneration]

| Key | Values | Default | Description |
|---|---|---|---|
| `SkipRepeatedRealCopy` | `0/1` | **`0`** (no tier turns it on) | `1` skips the full-resolution `OutputReal` copy the runtime emits **repeatedly** inside one group (byte-for-byte identical after the first frame, a dead store). About −566 µs per real frame at 4K 6X, about −164 µs at 1080p 6X, 0 at 2X. In offline replays it is bit-identical (two real captures: 60/60 and 20/20 `OutputReal`, 180/180 and 140/140 outputs), but that rests on the assumption that the game's plugin issues the Evaluates of one group back to back without touching the colour input in between — a property of the plugin, not of our kernels, and one that replays and synthetic scenes cannot test because their inputs cannot change. 0.3.2 shipped it on in tier 1 and a flicker report followed, so it is now an **explicit opt-in only**: write `1` to use it. The backend guards it, forwarding as usual and logging `real_copy_mismatch` on anything suspicious. See the subsection below. Supported on both models. |
| `ImageApprox` | `0/1` | **set by the tier** (`1` from tier ≥2) | Explicit switch for the lossy image-kernel rows, overriding the tier's answer: `1` at tier 1 adds only the lossy image rows, `0` at tier 2/3 keeps only the bit-identical part. **310.9 only**: 310.1 does not have these kernels, so writing it gets the bit stripped by the handshake and logs `kernel_selection_unsupported`. |
| `ImageApproxMask` | bitmask | **set by the tier** (tier 2 = the default set, tier 3 = all) | Names exactly which lossy image-kernel rows are enabled, one bit per fixed slot. Slots are **assigned once and never recycled**: a retired row leaves its slot empty, so a mask value written down in an old document always means the same set of rows; an unassigned bit in the mask is ignored and logs one `image_approx_mask_unassigned`. When one kernel has several rows, only the row with the lowest bit number is enabled and the other logs `image_approx_row_shadowed`. The armed rows, slots and resource ids are all in the backend's `image_approx` record. |
| `ForceGeneratedFrames` | `0`–`16` | `0` | **Diagnostic experiment (degrades the experience).** Makes **every getter** for `DLSSG.MultiFrameCount` and for the NVIDIA App override channel return this number (clamped to `MaxGeneratedFrames` and to the backend's ceiling), leaving the setters alone. It only does anything when the game's Streamline plugin **reads the count back**; otherwise it just breaks the interpolation phase and makes the image judder. Use `[Debug] MfgProbe=1` to decide, method in `docs/MFG_PROBE.md`. Set it back to `0` when done. |
| `ForcePluginFrames` | `0`/`2`–`5` | `0` | **Experimental.** Patches, in memory, a game that ships an **older 4X** Streamline plugin (Wukong `sl.dlss_g.dll` 2.7.4, Cyberpunk 2077 2.7.1) so it runs 6X: the plugin's hard-coded 4X ceiling (the `3` in `min(MultiFrameCountMax,3)`) and its default initial value become N, and the plugin's per-frame constant upload is hooked to force `numFramesToGenerate` to N, so the runtime's view, the plugin's loop and its resource allocation all land on N (which is exactly why `ForceGeneratedFrames`, changing only the getters, goes out of step). It acts only when a known version is **identified exactly** by SHA-256/FileVersion plus a unique byte signature; otherwise it does nothing and logs `plugin_mfg_unrecognized`. `MaxGeneratedFrames=N` must be set as well. The patch is applied **after** the plugin's signature check and **before** CreateFeature computes the ceiling. On 310.1 it clamps to 3 (a no-op). **Measured result (Wukong 2.7.4, 2026-09-14): the patch mechanism works** (the log shows `plugin_mfg_install` raising the ceiling 3→5 and `plugin_mfg_force` forcing `numFramesToGenerate` to 5), **but after a few 6X frames the whole of DLSS-G fails hard and switches off** (4X included) — a 4X integration builds its swap-chain back buffers for 4X once at init, so the forced 5th frame has no buffer to present, and Streamline does not fall back gracefully but shuts frame generation down entirely. **So this key is unusable on 4X games, and harmful**; off by default. Real 6X needs the game to ship `sl.dlss_g.dll` ≥ 2.11.1 (such as A Plague Tale's 2.11.1, which runs native 6X cleanly in testing). See `docs/MFG_PROBE.md`. |

### [Logging]

| Key | Values | Default | Description |
|---|---|---|---|
| `File` | `0/1` | `1` | `1` writes log files; `0` disables file output (still governed by `Level`). |
| `DebugOutput` | `0/1` | `0` | `1` also sends the log through Windows debug output, where a debugger can receive it; nothing is shown on the game's screen. |
| `EvaluateEvery` | `1`–`1000000` | `120` | Sampling interval for the normal Evaluate / marker log lines (counted in Evaluate calls). `1` logs every one; `0` is treated as 120. At Level 3 the first 12 are always logged. |

### [Debug]

| Key | Values | Default | Description |
|---|---|---|---|
| `MarkGeneratedFrames` | `0/1` | `0` | `1` draws a marker with the actual index, such as `FG 1/3`, on the generated output, to tell whether an artifact is on a generated frame or a real one. Real frames and Reset outputs are skipped. The marker changes generated-frame pixels, so turn it off for whole-image numeric comparisons. |
| `MarkerX` / `MarkerY` | `0`–`65535` | `8` / `8` | Top-left corner of the marker (in output-texture pixels). |
| `MarkerScale` | `1`–`8` | `2` | Marker scale factor; the rectangle is `24×scale` wide and `9×scale` high. |
| `Capture` | `0`–`100000` | `0` | How many Evaluates to record — parameters, inputs and outputs — for offline replay (GCR). `0` installs no hooks at all and costs nothing. The count is in Evaluate calls (2 per real frame at 3X, 5 per real frame at 6X). About 48 MB each at 1080p RGBA8. Full mechanism in `docs/CAPTURE.md`. |
| `CaptureDirectory` | path | `dlssg_sm86\capture` | Capture directory, relative to the game EXE's directory. |
| `CaptureSkip` | `0`–`1e8` | `0` | How many Evaluates to skip first (to get past loading screens / cutscenes). |
| `CaptureStartOnReset` | `0/1/2` | `1` | Which frame recording starts on. `2`: wait only for "the first Evaluate right after a feature was created", the only starting point at which a replay can be bit-identical; `1`: a `DLSSG.Reset` frame or the first frame of a newly created feature, whichever comes first; `0`: start immediately. With `1`/`2`, when 3 minutes pass without a hit nothing is recorded. |
| `CaptureStartKey` | key name / VK code | `0` | Key-triggered starting point: `F9` (F1–F24), a single letter or digit, or a virtual-key code (`120` / `0x78`). Once set, after `CaptureSkip` is satisfied it keeps waiting until this key is pressed in the game, then applies the `CaptureStartOnReset` rule from that moment (`0` = start recording on the Evaluate during which it was pressed, `start_reason=key`; `1`/`2` = wait for a Reset / a newly created feature from the key press on, with the 3-minute timeout counted from the key press). This is the way to land a capture on the moment "the character is actually moving"; `capture_waiting_for_key` is logged while waiting, `capture_start_key` on the press. |
| `CaptureNoStall` | `0/1` | `0` | `1`: the game thread never waits. The whole capture is buffered in system memory: the ring is as deep as the requested count, the readback buffers are built ahead of time by the writer thread while it waits for the starting point, and after recording ends the files finish in the background (`capture_finishing` → `capture_done`), so what gets recorded is consecutive frames. A group's memory is estimated as a whole from the sizes learned during prewarm (the first group, or any group with nothing in flight, is always let through; when the budget is smaller than one group it degenerates into recording one group at a time), and when the budget is short the whole group is skipped (a group = all generated frames of one real frame, `MultiFrameIndex` 1..count) — a plane or a frame is never dropped in the middle of a group. `0`: a 4-slot ring, and when the writer cannot keep up the game thread waits at most 2 seconds (at 4K the game drops to a few frames). Events `capture_prewarm` / `capture_group_skipped`, manifest `capture.taken_groups` / `skipped_groups` / `skipped_groups_memory`. On replay the first group after each gap only warms the runtime's previous-frame history, and `capture_compare.py` excludes it from the summary by default. |
| `CaptureRingSlots` | `0`, `4`–`1024` | `0` | Number of readback slots in flight; `0` = the default (4, or the `Capture` count when `CaptureNoStall=1`). |
| `CaptureMemoryMB` | `0`–`1048576` | `0` | Readback memory budget (pooled + in flight), in MiB; `0` = half the physical memory available at install time. Readback heaps live in system memory and use no VRAM. |
| `CaptureAsyncCopy` | `0/1` | `1` | `1`: moves the PCIe transfer off the rendering timeline. The game's own command list only does a VRAM→VRAM `CopyResource` (into a staging texture), and a separate `COPY` queue then moves the staging texture into the readback buffer in parallel with the game's rendering; the `COPY` queue first `Wait`s on the fence of the game's submission, and `collect()` waits on the `COPY` queue's own fence. `0`: the readback copy stays in the game's command list (the old path, measured at +122% per generated frame at 4K on a 3070). Events `capture_plane_inline` (one plane fell back to the old path) and `capture_async_copy_unavailable` (this machine cannot create the `COPY` queue/list, so everything falls back to the old path). |
| `CaptureVramMB` | `0`–`1048576` | `0` | VRAM budget for the staging textures, in MiB; `0` = 1536. Staging textures are pooled and reused by (device, format, size) and returned to the pool as soon as the `COPY` queue is done reading them, so the steady state only needs a few groups' worth; a plane over budget falls back to a per-plane inline readback and logs `capture_plane_inline`, which does not make the capture incomplete. This key does nothing when `CaptureAsyncCopy=0`. |
| `CaptureLean` | `0/1` | `0` | `1`: read back only what cannot be reconstructed. Inside one group, the input resources handed over by an Evaluate with index>1 are exactly the same as at index 1 (54/54 texture planes byte-for-byte identical across three Wukong captures), so they are recorded only as a reference to the index-1 file; `DLSSG.OutputReal` is the runtime's copy of `DLSSG.Backbuffer` (18/18) and is recorded as a reference too. A referenced plane's json carries `stored=false` and `source=<file relative to the capture root>`, and `sm86_replay` and `capture_compare.py` follow it. At 4X this roughly halves the bytes per group and cuts the readback PCIe time by the same amount. The 16-byte buffer parameters are always recorded in full. |
| `CaptureInputs` | `0/1` | `1` | `0` does not record input pixels (much smaller, but cannot be replayed). |
| `CaptureOutputs` | `0/1` | `1` | `0` does not record `OutputInterpolated` / `OutputReal` pixels. |
| `MfgProbe` | `0/1` | `0` | `1` logs every set and get of the frame-count parameters, with the calling module name, slot, value and context, to work out who decides "how many frames are generated". It reuses the capture's parameter-store hooks but also works on its own with `Capture=0`. Events and how to read them in `docs/MFG_PROBE.md`. |

### [Compatibility]

| Key | Values | Default | Description |
|---|---|---|---|
| `OptimizedKernels` | `0`–`3` | tier 0 (see the alias note) | **Backward-compatible alias** for `[FrameGeneration] Optimized`, taking the same tier range; when both are present `Optimized` wins. `OptimizedKernels=1` in an old INI is still tier 1. |
| `KernelImage` | `Auto/PTX/Cubin/Original` | `Auto` | Kernel image format, see the table below. **When the driver rejects a cubin it falls back automatically to the same kernel's PTX** (see "Driver version requirements and the automatic cubin → PTX fallback"). `Original` installs the hooks but replaces no kernel at all (a local stock-numerics reference; **the 310.9 build cannot use this on Ampere** — its stock image kernels are sm_89 and fail to create on Ampere, so use `Optimized=0` + `KernelImage=PTX` for a local reference). |
| `Router` | `Auto/SM86/SM75` | `Auto` | Kernel family. `Auto` picks it from the physical GPU (SM86 and above go SM86, Turing goes SM75). **SM75 is experimental** and is **present in both builds**. At `Optimized=1` both create all 63 variant rows with cross-kernel fusions and image patches fully on, and log one `sm75_route_limits` (`variant_rows=63`, `image_patches_partial` empty); the only differences are where the patches come from and `HardwareBilinear`: 310.1 has two image patches plus the texture probe (`HardwareBilinear` available), 310.9 has 3 patches re-derived against 310.9.1 RVAs (63 = 60 + 3), and since 310.9.1 removed the kernel the texture probe attached to, `HardwareBilinear` is forced to 0 on that build. On 3070 forward JIT both builds are bit-identical to their own SM86 stock route (including 6X on 310.9); **on a real Turing (RTX 2080 Ti)** the sm_75 cubins match their PTX and tier 0 matches tier 1 bit for bit, and for the same inputs the output is bit-identical to an RTX 3080 Ti's (synthetic rotate scene, 32/32; both differ from the official output on an RTX 5070 by the same bytes — the hardware accumulation difference between Blackwell and Turing/Ampere, see `docs/evidence/sm75/turing_2026-09-17.md`). Performance on Turing is not measured yet. The SM75 stock kernel family comes from Coldwood1026 (see `THIRD_PARTY_NOTICES.txt`). |
| `SM75Family` | `Repaired/Original` | `Repaired` | Only meaningful with `Router=SM75` (or on physical Turing); selects which **imported sm_75 stock kernel family** is used. `Repaired` is the one this project repaired: Coldwood1026's f16x2 min/max emulation swapped halfwords through a `.local` buffer accessed generically (undefined behavior, 342 sites in 9 DL2 kernels), which has been rewritten value-equivalently into registers, and all 72 cubins were recompiled from the repaired PTX — **the default, and every result in this repository was measured on it**. `Original` is the family imported as-is (the cubins are Coldwood's binaries byte for byte, the PTX only had its `.version` normalized); it is kept because an offline `ptxas` gives the unrepaired PTX a real stack frame, **the stock cubin path very likely never hits this defect on real Turing**, and only a physical RTX 20 can answer that; it is there for A/B testing by real Turing users. On the JIT path that takes PTX as input (forward JIT on a non-Turing card, measured on a 3070), `Original` reproduces the pre-repair 43–73 dB table. It swaps the stock kernels only; our own rewritten variants and the image patches are unaffected. On the 310.9 build it covers only the 44 kernels shared with 310.1 (the 26 new ones have no "as-is" version). On the SM86 route the key does nothing at all and one `sm75_family_ignored` is logged. |
| `Preset` | `Auto/A/B` | `Auto` | DLSS-G render preset (UI recomposition), **effective on the 310.9 build only**. `Auto` does not interfere; `A` forces UI recomposition off; `B` forces it on (needs the game to provide both a HUD-less image and a UI plane, and costs two extra full-resolution FP16 surfaces). Written on 310.1 it is refused (`kernel_selection_unsupported`). See the subsection below. |
| `SpoofArchToGame` | absent / `0/1` | **absent = automatic** | Governs **only the gate the game — or Streamline on its behalf — puts on the architecture**. **This is a tri-state key**: **not written** (the factory INI has no such line) = automatic and **`1`** = explicit install the redirect at exactly the same moment, the earliest of the five triggers below, on every GPU; **`0`** = never installed (that answer has to be known before the full INI parse, so at startup the loader makes one separate kernel32-only `GetPrivateProfileStringW` read of `[General] Enabled` and this key; a malformed or empty value counts as automatic), logging one `arch_spoof_disabled{redirect_installed}` on Turing. The only difference between automatic and an explicit `1` is the **log**: automatic releases its `arch_spoof_*` records only once the host has proved to be a Turing (the first real rewrite, or the backend's Turing verdict plus `turing_host_defaults`), so **a non-Turing card running the factory INI still produces not a single `arch_spoof_*` record**. Why it has to be this early: **Streamline 2.x decides the frame-generation plugin is "not supported on this platform" inside `slInit`** — `sl.dlss_g` defaults its minimum architecture to AD100, `sl.common` compares every adapter's `NvAPI_GPU_GetArchInfo` architecture against it and evicts the plugin (`Ignoring plugin 'sl.dlss_g' since it is not supported on this platform`), which happens before a D3D device exists and long before the NGX core asks us for the DLSS-G snippet, so anything armed "once the runtime is loaded" is too late (measured on an RTX 2080 Ti with FF7 Rebirth / Streamline 2.8.0.0: writing `1` and omitting the line gave the identical outcome, the plugin absent from the process either way). Mechanism: `nvapi64.dll`'s export-table entry for `nvapi_QueryInterface` is redirected to a 14-byte trampoline in that module's own section padding, so `NvAPI_GPU_GetArchInfo` reports a newer architecture to the outside (which one is the next row, `SpoofArchValue`: by default both Turing and Ampere are told Blackwell `0x1b0`; no byte of NVIDIA code is changed; **NVIDIA's own components always see the real architecture**: `_nvngx.dll`, `nvngx_*`, `nvapi*`, our own `sm86_backend.dll`, the driver components under `\DriverStore\FileRepository\`, and **the snippets in the NGX model store** - with the NVIDIA App's "DLSS override" active, or after an NGX OTA update, the core loads the super-resolution / ray-reconstruction / frame-generation snippets from paths like `C:\ProgramData\NVIDIA\NGX\models\dlss\versions\<n>\files\160_E658700.bin`, which carry no `nvngx_` in the name; 0.3.3 rewrote the answer for those as well, the SR snippet took its `arch >= 0x180` kernel paths and hung the GPU (`DXGI_ERROR_DEVICE_HUNG` right after the SuperSampling feature is created, before frame generation is ever asked for; issue #535, and the startup crashes #538 / #540 / #542), so since 0.3.4 every component under `\NVIDIA\NGX\models\` other than `sl_*`, and any `.bin` module, sees the truth, while `models\sl_*` - Streamline's own OTA plugins - is exactly what the rewrite is for and is still rewritten). The log carries one `arch_spoof_applied` per **distinct** calling module (with a new `path` field giving the full path) and one `arch_spoof_excluded` per distinct excluded module, so it shows who asked and which answer it got. **It only rewrites when the real answer is Turing (`0x160`) or Ampere (`0x170`)** (Ampere was added in 0.3.3: an RTX 3060 Laptop showed the same symptom as the 2080 Ti in FF7 Rebirth, issue #509): on other architectures the redirection is installed but does nothing, logging one `arch_spoof_inert{real_arch,rewritten_archs}`; and a `0` found by the full parse (i.e. the INI was unreadable at startup) stops it rewriting immediately. `arch_spoof_installed` carries `trigger` and `default`: the **three early triggers** `dllmain` (`nvapi64.dll` was already mapped when we were) / `nvapi_load` (our LoadLibrary hook caught `nvapi64.dll`'s own load) / `dependency_load` (`nvapi64.dll` arrived as another module's static import and was found mapped after some load) — only these three are ahead of `slInit`; the **two fallbacks** `settings` (the INI read, i.e. the game's first `nvngx_dlssg.dll` request) / `backend` (after the backend install). `default=true` means the automatic setting made the decision and says nothing about how early it was armed. **It has nothing to do with the NGX core's `0xbad0000b`**: that one is the core comparing the GPU architecture against the minimum architecture the runtime exports (see the `fg_gate_create_feature` row below), which no NVAPI spoof can change. |
| `SpoofArchValue` | `Auto/Ada/Blackwell` | `Auto` | Which architecture the rewrite reports. `Auto` (the default) tells **both RTX 20 (Turing) and RTX 30 (Ampere) Blackwell (`0x1b0`)**: both bundled runtimes do multi-frame generation, the backend already hands the runtime a Blackwell host anyway, and games and Streamline unlock the 3X / 4X / 6X choices on exactly this architecture — an Ada answer passes the frame-generation gate but leaves only 2X on screen (measured on real RTX 20 hardware, issues #527 / #528). `Ada` (`0x190`) is kept as a **diagnostic override**: it is what 0.2.4 and 0.3.0–0.3.2 reported, so it can be written back for comparison. `Blackwell` pins the same value explicitly (both also accept `0x190` / `0x1b0`). **This value was never the cause of the 0.3.3 crash**: what crashed was NVIDIA's own DLSS Super Resolution model receiving the rewritten architecture — with the NVIDIA App's "DLSS override" or an NGX OTA update in place the core loads it out of the NGX model store under a name like `160_E658700.bin`, which a leaf-name exclusion rule cannot catch. On a 3070 both the Ada and the Blackwell answer hang the GPU, and neither does once that snippet is excluded, so the fix is excluding NVIDIA's own components by location (`src/spoof_policy.h`, see the row above), not a smaller architecture. Read at startup with kernel32 alone like `SpoofArchToGame`; a malformed value falls back to `Auto` with one `configuration_warning`. `configuration.spoof_arch_value`, `arch_spoof_installed{spoof_arch_value,reported_arch_turing,reported_arch_ampere}` and `arch_spoof_applied.reported_arch` (432 / 400) say what was actually used. |
| `ForceSM86Route` | `0/1` | `0` | `1` allows forcing the SM86 backend on a non-SM86 GPU for validation (anything below SM86 is still refused). |
| `SimulateAmpere` | `0/1` | `0` | `1` adjusts the reported architecture for validation; requires `ForceSM86Route=1` as well. It does not change the physical GPU. |
| `LaunchChains` | `0/1` | `0` | **Experimental, do not turn it on (it reduces performance and can be harmful)**. `1` batches consecutive kernel submissions into `NvAPI_D3D12_LaunchCuKernelChain`. No gain on a 5070 (2–3% slower per Evaluate) and **measured harmful on 310.9** (the median Evaluate exceeds the minimum by 3–120×). It is kept only for re-measuring on another driver / GPU. |
| `DisableFusions` | `0`–`63` | `0` | **Diagnostic bitmask (turning bits on makes it slower)** that disables one class of `OptimizedKernels` cross-kernel fusion each: 1 decoder upscale+add+1×1, 2 decoder 1×1 pair, 4 `k_initial_merge`+convPre, 8 conv0+pooling, 16 residual chain, 32 `k_central_block`+block1 conv0. The default 0 = every fusion on. |
| `PlainVariant` | `0`–`3` | `0` | Selects the n-th replacement implementation when several are registered for the same kernel (out of range takes the last). **The default 0 is the measured-fastest set**; a value >0 re-measures an alternative implementation and is usually slower. |
| `ImagePatches` | `0/1` | **set by the tier** (`1` from tier ≥1) | `0` disables the **exact** PTX-level patches on the image-processing kernels (needs tier ≥1; supported on both models). The patches do not change semantics and are bit-identical. Note it covers the exact patches only: the lossy rows are governed by `ImageApprox`. |
| `HardwareBilinear` | `0/1` | **set by the tier** (`1` at tier 3, `0` otherwise; always 0 on 310.9) | **310.1 only** (forced to 0 on 310.9). `1` makes the composite kernels use texture-unit bilinear sampling instead of a hand-written fp32 blend, saving about 5 µs per index, but **the bottom row can differ by 12 LSB in a rotating scene** (the stock kernel mixes in zero texels at the bottom edge) — which is why it belongs to the lossy accelerations of tier 3. |
| `ChainBlock0` | `0/1` | `0` | `1` also replaces DL2 block0's 8 residual convolutions with the persistent chain kernel. **On par** with 8 native submissions on a 5070 (no gain), so it is off by default; it can be re-measured on other GPUs. |

### [Runtime]

| Key | Values | Default | Description |
|---|---|---|---|
| `Path` | path | empty | Used only with `Mode=Pinned`; points at the target stock `nvngx_dlssg.dll`. Not used by `Bundled`/`Auto`. |

`Mode` values: `Bundled` (a normal install, with the embedded runtime and backend); `Auto` (load the stock runtime the game asked for, with a backend matched automatically for a known hash); `Pinned` (use the runtime named by `Path`, which needs a matching backend). In the default mode, a failure to extract the cache, to load, or to install logs `runtime_selection_failed` and falls back to the original request.

### [Backends]

An advanced external-backend mapping; there is no such section by default. The key is the full SHA256 of the target stock runtime and the value is the path to a matching backend DLL; only `Auto`/`Pinned` need it:

```ini
[Backends]
; <runtime-sha256>=backends\matching_backend.dll
```

An external backend had better support the kernel-selection extension (the optional export `DlssgMod_SupportedRouteFlags`). When it does not (or supports only part of it), the loader **strips the flag bits that backend cannot answer and installs anyway**, logging one `kernel_selection_unsupported{requested_flags,supported_flags,stripped_flags,effect:"installed without them"}`; those switches do nothing, but frame generation itself is not lost because of it. Note that the factory default best set itself carries the two diagnostic bits `NO_HW_BILINEAR` / `NO_CHAIN_BLOCK0`, so an old-ABI backend without that export always produces this record (its `supported_flags` is 0 and everything is stripped).

### KernelImage behavior

| KernelImage value | Behavior once the route is active |
|---|---|
| `Auto` | Uses the precompiled cubin when the physical architecture matches the selected kernel family, otherwise PTX. Falls back to PTX automatically when the driver rejects a cubin. |
| `PTX` | Always uses the selected kernel family's PTX, JIT-compiled by the driver; works on a 3080 Ti too. |
| `Cubin` | **Prefers** the precompiled cubin and requires the physical architecture to match the kernel family **exactly**, otherwise it refuses to install that path (under the default Bundled mode it falls back to the original loading). When the architecture matches but the driver rejects the cubin it still falls back to PTX (better a JIT than no frame generation). |
| `Original` | **Diagnostic / reference**: installs the hooks but replaces **not a single kernel**, and enables no optimization / fusion / image patches. Mutually exclusive with `PTX`/`Cubin` and independent of `Router`. **The 310.9 build cannot use it on Ampere.** |

What `KernelImage` × `Router` actually selects:

| Router / physical GPU | `Auto` | `PTX` | `Cubin` |
|---|---|---|---|
| SM86 family, physical SM86 (3070/3080 Ti) | `cubin_sm86` | `ptx_sm86` | `cubin_sm86` |
| SM86 family, physical SM89/SM120 (forced-route validation) | `ptx_sm86` | `ptx_sm86` | refused |
| SM75 family, physical SM75 (RTX 20-series) | `cubin_sm75` | `ptx_sm75` | `cubin_sm75` |
| SM75 family, physical SM86 and above (forward-JIT validation) | `ptx_sm75` | `ptx_sm75` | refused |

The SM75 family and the SM86 family are **bit-identical** under forward JIT on an RTX 3070 (both the stock family and the optimized set: 240/240 images across the box and rotate scenes at 7 resolutions; splitting `m16n8k16` into two `m16n8k8` adds no rounding step that differs from stock). The optimized set on SM75 is **not one row short**: all 63 variant rows have an sm_75 image, and both image patches and `HardwareBilinear`'s texture probe are available (with `HardwareBilinear=1` the two routes' outputs are bit-identical as well).

**The same holds on the 310.9 build**: of 310.9.1's 70 kernels, 44 share 310.1's sm_75 family, and the sm_75 images of the 26 new kernels are rewritten from this repository's own sm_86 PTX by `scripts/perf/ptx_sm75.py` (`assets/kernels/310_9_1/sm75/`). Measured under 3070 forward JIT: box + rotate × 7 resolutions, 240/240 images each for the stock family and the optimized set, plus **another 80/80 with `--multi 5` (6X)**, all bit-identical to the SM86 stock route on the same machine; the MFG phase (the `i/(N+1)` centroid) is exactly the same on both routes. Details in `docs/evidence/310_9_1_sm75.md`.

The numerics were verified bit-identical to the SM86 route by `.target sm_75` forward JIT on an RTX 3070; on **real Turing (a physical RTX 20, 2026-09-15/17)** the whole `sm_75` cubin family creates and runs with 0 fallbacks, cubin and PTX are bit-identical, tier 0 and tier 1 are bit-identical, and it works in game. Timings on Turing are not yet quantified. Details in `docs/VALIDATION.md` and `docs/evidence/sm75/turing_2026-09-17.md`.

#### Driver version requirements and the automatic cubin → PTX fallback

| Path | Minimum driver | Reason |
|---|---|---|
| cubin (`Auto` on physical SM86, `Cubin`) | roughly **R580+** (591.86 measured working on this machine) | every cubin is produced by CUDA 13.0.2's `ptxas` (ELF ABI 65) |
| PTX (other architectures, `PTX`) | roughly **R555+** (PTX ISA 8.5) | every PTX declares `.version 8.5` and is JIT-compiled by the driver |

With a driver in between, cubin creation fails: **the backend retries once in place with the same kernel's PTX fatbin**, after which that image family (base / variant / image patch, each independent) uses PTX. So the only consequence of an older driver is one extra JIT on first load; frame generation runs as usual and the numerics are bit-identical. Look for `kernel_image_fallback` in the log (error level, once per process). The diagnostic variable `DLSSG_FORCE_CUBIN_FAIL=1` forces this path (for validation only).

#### `SkipRepeatedRealCopy`: skipping the repeated OutputReal copy

Both runtimes' host graphs emit a full-resolution `CopyResource(Backbuffer → DLSSG.OutputReal)` at the end of **every** Evaluate. `ext_real` has a single writer and no reader and `ext_color` is read-only, so within one group every copy after the first writes byte-for-byte identical content — all dead stores. `1` makes the backend swallow it when `MultiFrameIndex > 1`. The gain grows with the multiplier (−566 µs per real frame at 4K 6X, 0 at 2X) and is orthogonal to kernel replacement (it works at `Optimized=0` too). No tier turns it on (0.3.2 had it on in tier 1; withdrawn afterwards); turning it on assumes "the game does not redraw the real frame between two Evaluates of the same group", which only a live run in that game can confirm — replays cannot; the backend guards against this, forwarding as usual and logging `real_copy_mismatch` on anything suspicious. Mechanism in `docs/ARCHITECTURE.md`.

#### Preset: the A / B presets (UI recomposition), 310.9 build only

The DLSS frame-generation "render preset A / B" in the driver / NVIDIA App corresponds to exactly one boolean inside 310.9.1: UI recomposition (`DLSSG.UserInterfaceRecompositionEnabled`). `A` = the UI is baked into the color history and interpolated with it (the HUD is smeared by the motion vectors); `B` = the HUD-less color and the "UI color + alpha" are interpolated separately (a static HUD is not dragged along). `B` needs the game to provide both `DLSSG.HUDLess` and `DLSSG.UI`; otherwise the image is the same as A while two full-resolution FP16 surfaces are wasted (about 33 MB at 1080p, about 133 MB at 4K). `A`/`B` in the INI overrides the driver's / NVIDIA App's preset override; keep `Auto` to let the driver speak. One `preset_pinned` is logged per feature creation.

## How to confirm the configuration took effect

Check in a Level 2 or 3 log (the factory `Level=1` records errors only; set it to `2` or `3` temporarily when verifying the configuration):

| Event / field | Meaning |
|---|---|
| loader: `configuration` | The INI as read this run, the runtime mode, the kernel-format request, plus `optimized_tier`/`optimized_tier_name` (the parsed consistency tier), every knob that tier resolves to (`optimized_kernels`/`image_patches`/`image_approx`/`image_approx_mask`/`skip_repeated_real_copy`/`hardware_bilinear`/`chain_block0`/`launch_chains`), `warnings` (how many keys fell back this run) and `proxies` (`{active, standby[]}`, which proxy is doing the work). |
| loader: `configuration_warning` (Level 1) | A key's value was invalid and has fallen back to that key's default: `section`/`key`/`value`/`default`/`reason`. The mod stays enabled and only that key does nothing. One record per bad key. |
| loader: `configuration_error` (Level 1) | The whole configuration could not be read at all (a path that cannot be formed, out of memory); the mod switches off and the game's original loading is kept. **An ordinary typo does not get here** — that is the row above. |
| loader: `kernel_selection_unsupported` (Level 1) | The backend does not support some flag bits the INI asked for: `stripped_flags` are the bits removed and `effect` is `installed without them` — **the install proceeds as usual**, those switches simply do nothing. |
| backend: `install` | `actual_sm` the physical architecture; `active` whether the route is enabled this run; `image` the selected format; `target_sm`/`router` the resolved kernel family; `optimized_tier`/`optimized_tier_name` and `optimized_knobs` (the whole set of knobs that tier actually landed on). |
| backend: `image_approx` | 310.9 only: how the lossy image rows resolved — `enabled`, `optimized_tier`, `mask`/`mask_from` (given by the INI or the tier default), `armed[]` (each row's entry name, slot and resource id), `rows_shadowed`, `exact_patches_replaced`. |
| loader: `backend_install` | `status=0` means the backend installed successfully; combine with `install.active` to tell whether kernel replacement is enabled. |
| `image=ptx_sm86` / `cubin_sm86` / `original` | The SM86 kernel format that was selected, or no replacement. |
| backend: `kernel_image_fallback` | The driver rejected a cubin and that image family has switched to PTX automatically. |
| backend: `preset_pinned` | `[Compatibility] Preset` took effect (310.9 only). |
| backend: `real_copy_skip` / `real_copy_skipped` / `real_copy_mismatch` | `SkipRepeatedRealCopy`'s hook, skip count and guard trigger. |
| backend: `mfg_probe` / `mfg_summary` | Records from `[Debug] MfgProbe` and `[FrameGeneration] ForceGeneratedFrames`; how to read them in `docs/MFG_PROBE.md`. |
| Level 3: `kernel_create` / `evaluate` | The format and status actually used for each kernel creation; the actual generated count, the result, `real_copies_skipped`, `image_fallbacks`. |

To confirm the route installed, check both `install.active=true` and the matching `backend_install.status=0`. `image=ptx_sm86` on its own does not mean a kernel was created or executed; look at the later `kernel_create` / `evaluate` as well.

### Why frame generation will not turn on: `fg_gate_*` (always on, no INI key)

When `install` looks entirely normal after installation but there is not a single `kernel_create` / `evaluate`, either the game never created the DLSS-G feature or the creation failed before the runtime built its first kernel — two things our old log could not tell apart, let alone explain. The decision is made entirely above the runtime: Streamline's own gates (**hardware-accelerated GPU scheduling (HAGS) off** → `eFeatureNotSupportedHWSchedulingDisabled`, or a driver below what the plugin requires), the NGX core's final answer to `GetFeatureRequirements` (the core adds its own GPU / driver / HAGS judgement on top of the runtime's answer), the NGX capability parameters the plugin reads, and the game's own UI switch. `src/fg_gate.cpp` observes only these four places, and all five records are visible at Level 2 (only a failed CreateFeature is error level and is recorded at Level 1 as well):

| Event (backend) | Content / how to read it |
|---|---|
| `fg_gate_environment` | One record at install time. `hags` (`KMTQAITYPE_WDDM_2_7_CAPS` from `D3DKMTQueryAdapterInfo`: `supported`/`enabled`/`enabled_by_default`, `state=on\|off\|unsupported`), `driver` (`NvAPI_SYS_GetDriverAndBranchVersion`, e.g. `610.74` / `r610_00`, plus the file version of a loaded `nvapi64.dll`), `windows` (`RtlGetVersion`), `ngx_core` (`_nvngx.dll` path + version), `streamline` (the loaded `sl.interposer.dll` / `sl.common.dll` / `sl.dlss_g.dll` / `sl.dlss.dll`, looked up with `GetModuleHandle` only, never loaded). **`hags.state=off` is basically the answer.** Every item is optional; when it cannot be obtained it is written as `available=false` plus a reason, and it never makes the install fail. |
| `fg_gate_hooks` / `fg_gate_hooks_deferred` | What is hooked are the **public exports of the NGX core `_nvngx.dll`** (`GetFeatureRequirements` / `GetCapabilityParameters` / `GetParameters` / `CreateFeature`), each of the four reporting `attached`. When the core is not loaded yet, `LoadLibrary` is watched first (`fg_gate_hooks_deferred`) and the hooks go in when it appears; the record also carries the `ngx_core` version once. |
| `fg_gate_requirements` | Each `GetFeatureRequirements` in the core (up to 8). `feature_supported` is the value **the caller actually received**, `decoded` unpacks it bit by bit (0 supported / 1 GPU unsupported / 2 driver version unsupported / 4 OS version unsupported / 8 hardware scheduling off), and there are also `min_hw_architecture`, `min_os_version`, `caller` (calling module + offset within it) and `adapter_description`. Do not confuse it with the backend's own `feature_requirements`: that one is only the runtime's answer to our bridge, and the core still adds its own judgement on top. |
| `fg_gate_capability` | After each capability query (up to 4), reads back `FrameGeneration.Available` / `.NeedsUpdatedDriver` / `.MinDriverVersionMajor` / `.MinDriverVersionMinor` / `.FeatureInitResult`, `DLSSG.MultiFrameCountMax`, `DLSSG.ReflexWarp.Available`, and `SuperSampling.Available` as a control. `Available=0` or `NeedsUpdatedDriver=1` = the plugin refused before anything on our side ever ran. `.FeatureInitResult` is the NGX status the core recorded for this feature: `3134193675` = `0xBAD0000B` (`NVSDK_NGX_Result_FAIL_OutOfDate`), which means exactly the same as the `CreateFeature` failure in the next row, except that the core recorded it back in the capability-query stage, and once the plugin sees `Available=0` it never calls `CreateFeature` again (measured on an RTX 2080 Ti in The Witcher 3). |
| `fg_gate_create_feature` | Each `CreateFeature` in the core (up to 16): `feature_id` (DLSS-G = 11), `status`, `succeeded`, `handle`, `caller`, and for DLSS-G also `parameters` (`multi_frame_count` / `multi_frame_count_max` / `width` / `height`). **A failure is error level**; `0xbad0000b` (`NVSDK_NGX_Result_FAIL_OutOfDate`) = the core took the GPU architecture it read from NVAPI during its own initialization, compared it against `NVSDK_NGX_GetGPUArchitecture` exported by the DLSS-G runtime (snippet) (= the minimum architecture the runtime declares), and refused. The backend reports that export as the **physical architecture** (`turing_host.gpu_architecture_export`, `0x160` on Turing; it used to report Ada `0x190`, which was exactly why frame generation would not turn on on real Turing). `SpoofArchToGame` governs the other gate (the game's / Streamline's own) and does not help here: the redirect is now installed at game start (`trigger=dllmain`/`nvapi_load`/`dependency_load`), but what the core compares here is not NVAPI's answer, it is the minimum architecture the snippet exports. |
| `fg_gate_summary` | One record, on the first Evaluate (`reason=first_evaluate`) or at process exit (`process_exit`): the counts of requirements / capability / create and the status of the last creation. `create_calls=0` with an entirely normal `install` = the game never asked for the feature. |

The `== fg gate ==` section of `python scripts\analysis\sm75_report.py <log directory>` puts the records above on one page, and the VERDICT gets an extra `feature created` line: PASS when creation succeeded, FAIL (with the status) when it failed, and NOT RUN when it was never called, naming the most likely gate from the evidence.

## Advanced configuration examples

The examples below are for diagnostics / validation only; a normal install needs none of them. Just add the section and key to the factory INI (add the `[Compatibility]` / `[Debug]` section yourself if it is missing).

**PTX JIT on a 3080 Ti (the same as automatic detection, normally not needed):**

```ini
[Compatibility]
KernelImage=PTX
```

**Validating the SM86 PTX path on an RTX 5070:**

```ini
[Compatibility]
KernelImage=PTX
ForceSM86Route=1
SimulateAmpere=1

[Logging]
Level=3
```

**Taking a local stock-numerics reference:** just use the factory INI and change `Optimized` to `0` (do not use `KernelImage=Original` on 310.9).

**Turning on generated-frame markers (to tell whether an artifact is on a generated frame or a real one):**

```ini
[Debug]
MarkGeneratedFrames=1
```

**310.9 build + 6X, skipping the repeated real-frame copy:**

```ini
[FrameGeneration]
MaxGeneratedFrames=5
SkipRepeatedRealCopy=1
```

## Installation notes and uninstalling

The four utility proxies at the package root (`version.dll`, `winmm.dll`, `dbghelp.dll`, `dinput8.dll`) **all go over; there is nothing to pick and no need to keep only one**: the embedded runtime and backend are identical and only the file name and forwarding target differ. The first one loaded in the process becomes active, and the rest go on standby and only forward (mechanism in "Several proxies at once" above).

The render-path proxies `alternatives/dxgi.dll` and `alternatives/d3d12.dll` are not at the root: they sit on the D3D12 hot path and are load-order sensitive, so copy **one** of them over by hand only when none of the four above was loaded by the game. The instructions ship with the package in `alternatives/README.md`.

Every proxy forwards all of its own exports to the real DLL of the same name in `System32`, and intercepts only the loading of `nvngx_dlssg.dll`.

Code signing the release DLLs is optional: `build.ps1 -SignCert <pfx> -SignPass <pw>` (or `DLSSG_SIGN_CERT`/`DLSSG_SIGN_PASS`) applies a SHA-256 Authenticode signature to all four root proxies and to `alternatives\*.dll`; without a certificate the artifacts are unsigned. The steps, how to make a throwaway test certificate, and why a self-signed certificate gives no default trust are in `docs/SIGNING.md`.

Windows x64 / D3D12 for now. This package's build checks and optional GPU validation status are in `manifest.json` / `validation.json`, the architecture in `docs/ARCHITECTURE.md`, capture and replay in `docs/CAPTURE.md`, and the full record in `docs/VALIDATION.md`.

To uninstall, exit the game and remove **every** proxy this package added (the four at the root, and any `dxgi.dll`/`d3d12.dll` copied over by hand) and the INI; the cache can stay. Ownership of the original NVIDIA DLL and the kernel resources is in `THIRD_PARTY_NOTICES.txt`; the SM75 GPU resources come from Coldwood1026's RTX 20-series porting work, gratefully acknowledged.
