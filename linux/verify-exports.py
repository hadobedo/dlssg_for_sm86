#!/usr/bin/env python3
"""Fail if the shim no longer covers every export the proxy requires.

The mod's version.dll refuses to initialize unless every export it looks for
resolves on the system version.dll -- which, under Wine, is our shim. So the
shim's export set must always be a superset of the proxy's.

Usage: verify-exports.py <proxy.dll> <shim.dll>

Run by CI on every release; needs python3-pefile (or `pip install pefile`).
"""
import sys

import pefile


def exports(path):
    pe = pefile.PE(path, fast_load=True)
    try:
        pe.parse_data_directories(
            directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]]
        )
        table = getattr(pe, "DIRECTORY_ENTRY_EXPORT", None)
        if table is None:
            return set()
        return {e.name.decode() if e.name else f"ordinal#{e.ordinal}" for e in table.symbols}
    finally:
        pe.close()


def main(argv):
    if len(argv) != 3:
        print(__doc__.strip(), file=sys.stderr)
        return 2

    proxy, shim = argv[1], argv[2]
    required, provided = exports(proxy), exports(shim)
    print(f"{proxy}: {len(required)} exports")
    print(f"{shim}: {len(provided)} exports")

    missing = sorted(required - provided)
    if missing:
        print(
            f"\nerror: the shim is missing {len(missing)} export(s) the proxy requires:",
            file=sys.stderr,
        )
        for name in missing:
            print(f"  - {name}", file=sys.stderr)
        print(
            "\nAdd a forwarder (or a stub returning 0, if Wine does not implement\n"
            "the function) for each name to linux/version_shim.c and "
            "linux/version_shim.def,\nthen re-run. See linux/LINUX.md.",
            file=sys.stderr,
        )
        return 1

    # Extra exports are fine: the proxy ignores what it does not look for.
    extra = sorted(provided - required)
    if extra:
        print(f"note: shim exports not used by the proxy: {', '.join(extra)}")

    print("ok: the shim covers every export the proxy requires")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
