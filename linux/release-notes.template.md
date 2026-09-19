Wine/Proton build of the DLSSG-SM86 `version.dll` proxy: the unmodified upstream runtime
plus the compatibility shim that lets it initialize under Wine.

- built from upstream `@REF@` at [`@SHA7@`](https://github.com/@UPSTREAM_REPO@/commit/@SHA@)
- upstream version: @VERSION@
- compatibility shim: @SHIM_NOTE@
- payload hash: `@PAYLOAD@`
- shim sources: `@SHIM_SRC@`

**Install** (game closed)

1. Unzip the bundle and enter it.
2. `./install.sh --prefix <wine prefix> --game-dir <rendering EXE folder>` — the prefix is
   the folder containing `drive_c`, e.g. `.../steamapps/compatdata/<AppID>/pfx`. It copies
   the proxy and INI next to the game and installs the shim into `system32`. If the shim is
   marked optional above, add `--no-shim` to leave `system32` alone; `./install.sh
   --uninstall` undoes the shim afterwards regardless.
3. Set the DLL override for that prefix once: `protontricks <AppID> winecfg` → Libraries:
   `version` = `native,builtin`.
4. Launch. A `dlssg_sm86/logs/loader_*.jsonl` next to the game executable means the
   `c0000142` startup abort is gone.

`game-dir/version.dll` and `game-dir/dlssg_sm86.ini` are the unmodified upstream files and
are what actually enable frame generation — they must sit next to the game's rendering EXE.
`system32/version.dll` is the shim, built from `shim-source/`, checked against the proxy's
export set and smoke-tested under Wine by CI; it is a compatibility layer for the proxy, not
a substitute for it. `LINUX.md` has the diagnosis, manual steps and troubleshooting.

This release was built and published by
[`.github/workflows/wine-release.yml`](https://github.com/@REPO@/blob/main/.github/workflows/wine-release.yml),
which polls upstream for new releases; `linux/state.json` records exactly what it built.

Unofficial community packaging. The failure was diagnosed by
[tB0nE](https://github.com/tB0nE/dlssg_for_sm86) in
[upstream issue #10](https://github.com/@UPSTREAM_REPO@/issues/10); the mod itself is
[sdli1995/dlssg_for_sm86](https://github.com/sdli1995/dlssg_for_sm86). See
`THIRD_PARTY_NOTICES.txt` for the upstream terms.
