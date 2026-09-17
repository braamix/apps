#!/usr/bin/env python3
"""Bring one of CPython's own tests into this port's suite.

    tools/mkcpy.py test_unary.py test_io/test_fileio.py [more...]

Copies Lib/test/<path> into test/cpython/<name> byte for byte and adds a row
to test/cpython.txt. A test inside a package of tests is taken on its own,
under its file name, where it runs the same when it imports nothing of the
package; the row records the path it came from. The golden beside it is what this interpreter printed, so
it is written by the harness rather than here:

    node test/pycases.mjs --bless

The clone in tmp/ is not committed, so the commit a copy came from is recorded.
Unlike MicroPython's, these tests are not self-contained -- every one imports
unittest and most import test.support -- and the shims under test/shim/ are
what answers that.
"""

import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
UPSTREAM = os.path.join(ROOT, "tmp", "cpython")
TESTS = os.path.join(UPSTREAM, "Lib", "test")
CASES = os.path.join(ROOT, "test", "cpython")
MANIFEST = os.path.join(ROOT, "test", "cpython.txt")


def die(msg):
    sys.exit(f"mkcpy: {msg}")


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
    if not os.path.isfile(MANIFEST):
        return head, rows
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
    rows.sort(key=lambda r: r[1])
    width = [max((len(r[i]) for r in rows), default=0) for i in range(5)]
    with open(MANIFEST, "w", encoding="utf-8") as f:
        for line in head:
            f.write(line + "\n")
        for r in rows:
            f.write("  ".join(v.ljust(width[i]) for i, v in enumerate(r)).rstrip() + "\n")


def add(path):
    src = os.path.join(TESTS, path)
    if not os.path.isfile(src):
        die(f"{path}: no such test under {TESTS}")
    name = os.path.basename(path)

    os.makedirs(CASES, exist_ok=True)
    shutil.copyfile(src, os.path.join(CASES, name))

    head, rows = read_manifest()
    rows = [r for r in rows if r[1] != name]
    rows.append(["fail", name, "new", commit(), "Lib/test/" + path])
    write_manifest(head, rows)
    print(f"mkcpy: {name} — now run `node test/pycases.mjs --bless`")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("tests", nargs="+", help="names under Lib/test/, e.g. test_unary.py")
    args = ap.parse_args()

    if not os.path.isdir(TESTS):
        die(f"no upstream clone at {UPSTREAM}")
    for name in args.tests:
        add(name)


if __name__ == "__main__":
    main()
