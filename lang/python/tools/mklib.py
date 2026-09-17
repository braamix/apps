#!/usr/bin/env python3
"""Take a module from CPython's standard library into lib/.

    tools/mklib.py [--floor NAME] types.py collections/__init__.py [more...]

Copies Lib/<path> into lib/<path> byte for byte and adds its row to
lib/manifest.txt: the name, the native module it stands on (`-` for none, or
--floor), the commit the copy came from, and where it came from. A module
already listed is copied again and keeps its floor unless --floor is given.

The copy is never edited. A module that needs something this interpreter has
not got waits in the clone rather than being trimmed here.
"""

import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
UPSTREAM = os.path.join(ROOT, "tmp", "cpython")
LIB = os.path.join(UPSTREAM, "Lib")
OURS = os.path.join(ROOT, "lib")
MANIFEST = os.path.join(OURS, "manifest.txt")


def die(msg):
    sys.exit(f"mklib: {msg}")


def commit():
    try:
        out = subprocess.run(["git", "-C", UPSTREAM, "rev-parse", "--short", "HEAD"],
                             capture_output=True, text=True, check=True)
    except (OSError, subprocess.CalledProcessError):
        die(f"cannot read the upstream commit — is {UPSTREAM} a clone?")
    return out.stdout.strip()


def read_manifest():
    """The rows as field lists, plus the header lines above them."""
    head, rows = [], []
    with open(MANIFEST, encoding="utf-8") as f:
        for line in f:
            t = line.strip()
            if not t or t.startswith("#"):
                if not rows:
                    head.append(line.rstrip("\n"))
                continue
            rows.append(t.split())
    return head, rows


def write_manifest(head, rows):
    rows.sort(key=lambda r: r[0])
    width = [max((len(r[i]) for r in rows), default=0) for i in range(4)]
    with open(MANIFEST, "w", encoding="utf-8") as f:
        for line in head:
            f.write(line + "\n")
        for r in rows:
            f.write("  ".join(v.ljust(width[i]) for i, v in enumerate(r)).rstrip() + "\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--floor", help="the native module these stand on")
    ap.add_argument("paths", nargs="+", help="paths under Lib/, e.g. types.py")
    args = ap.parse_args()

    if not os.path.isdir(LIB):
        die(f"no upstream clone at {UPSTREAM}")
    head, rows = read_manifest()
    have = {r[0]: r for r in rows}
    rev = commit()
    for path in args.paths:
        src = os.path.join(LIB, path)
        if not os.path.isfile(src):
            die(f"{path}: no such file under {LIB}")
        dst = os.path.join(OURS, path)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(src, dst)
        floor = args.floor or (have[path][1] if path in have else "-")
        have[path] = [path, floor, rev, "Lib/" + path]
        print(f"mklib: {path}")
    write_manifest(head, list(have.values()))


if __name__ == "__main__":
    main()
