#!/usr/bin/env python3
"""Bring one MicroPython test into this port's suite.

    tools/mkexp.py basics/andor.py [more...]

Copies tests/<name> into test/cases/<name> byte for byte, writes <name>.exp
beside it, and adds a `fail` row to test/manifest.txt. The expected output is
upstream's own .exp when there is one and the host CPython's otherwise, which
is what upstream's run-tests.py compares against. The clone in tmp/ is not
committed, so the commit a copy came from is recorded.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
UPSTREAM = os.path.join(ROOT, "tmp", "micropython")
TESTS = os.path.join(UPSTREAM, "tests")
CASES = os.path.join(ROOT, "test", "cases")
MANIFEST = os.path.join(ROOT, "test", "manifest.txt")

# Written for a specific CPython. An older host prints something else, and an
# .exp generated from it would be a lie.
VERSIONED = ("_cp310", "_cp311", "_py312", "python34.py", "python36.py")


def die(msg):
    sys.exit(f"mkexp: {msg}")


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
    rows.sort(key=lambda r: r[1])
    width = [max((len(r[i]) for r in rows), default=0) for i in range(5)]
    with open(MANIFEST, "w", encoding="utf-8") as f:
        for line in head:
            f.write(line + "\n")
        for r in rows:
            f.write("  ".join(v.ljust(width[i]) for i, v in enumerate(r)).rstrip() + "\n")


def cpython_output(path):
    """What the host's CPython prints. Run from a scratch directory: a test
    may write files."""
    with tempfile.TemporaryDirectory() as cwd:
        r = subprocess.run([sys.executable, os.path.abspath(path)],
                           capture_output=True, cwd=cwd)
    if r.returncode != 0:
        die(f"{path}: CPython exited {r.returncode}\n{r.stderr.decode(errors='replace')}")
    if r.stderr:
        die(f"{path}: CPython wrote to stderr, which run-tests.py does not capture")
    out = r.stdout.decode("utf-8")
    if out.startswith("SKIP"):
        die(f"{path}: the test skips itself on this host")
    return out


def add(name, force):
    src = os.path.join(TESTS, name)
    if not os.path.isfile(src):
        die(f"{name}: no such test under {TESTS}")
    if not force and any(v in name for v in VERSIONED):
        die(f"{name}: written for a specific CPython version; "
            f"host is {sys.version_info.major}.{sys.version_info.minor} — pass --force to copy anyway")

    dst = os.path.join(CASES, name)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copyfile(src, dst)

    if os.path.isfile(src + ".exp"):
        shutil.copyfile(src + ".exp", dst + ".exp")
        origin = "upstream"
        with open(dst + ".exp", encoding="utf-8") as f:
            if "########" in f.read():
                print(f"mkexp: {name}.exp matches lines by pattern, which "
                      f"runcases.mjs does not do yet", file=sys.stderr)
    else:
        with open(dst + ".exp", "w", encoding="utf-8") as f:
            f.write(cpython_output(src))
        origin = f"cpython{sys.version_info.major}.{sys.version_info.minor}"

    head, rows = read_manifest()
    rows = [r for r in rows if r[1] != name]
    rows.append(["fail", name, origin, commit(), "tests/" + name])
    write_manifest(head, rows)
    print(f"mkexp: {name} ({origin})")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("tests", nargs="+", help="paths under tests/, e.g. basics/andor.py")
    ap.add_argument("--force", action="store_true",
                    help="copy a version-specific test anyway")
    args = ap.parse_args()

    if not os.path.isdir(TESTS):
        die(f"no upstream clone at {UPSTREAM}")
    for name in args.tests:
        name = name.removeprefix("tests/")
        add(name, args.force)


if __name__ == "__main__":
    main()
