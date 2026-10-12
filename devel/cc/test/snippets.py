#!/usr/bin/env python3
"""Every C snippet of vcc's tests, for the corpus.

    snippets.py <vcc checkout> <output directory>

Each raw string literal of every test source, and the body of
BITFIELD_RUN_SOURCE, becomes <dir>_<file>_<n>.c, whether it is a whole
program or not.
"""

import pathlib
import re
import subprocess
import sys

vcc, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
files = subprocess.run(["git", "-C", str(vcc), "ls-files", "*/test/*.cpp", "*/test/*.h",
                        "*/test/*/*.cpp", "*/test/*/*.h"], capture_output=True, text=True,
                       check=True).stdout.split()
RAW = re.compile(r'R"(\w*)\((.*?)\)\1"', re.S)
total = 0
for f in files:
    text = (vcc / f).read_text(errors="replace")
    bodies = [m.group(2) for m in RAW.finditer(text)]
    m = re.search(r"^BITFIELD_RUN_SOURCE\((.*)^\)", text, re.S | re.M)
    if m:
        bodies.append(m.group(1))
    stem = f.replace("/test/", "_").replace("/", "_").rsplit(".", 1)[0]
    for i, body in enumerate(bodies):
        if len(body) < 20 or ";" not in body:
            continue
        (out / f"{stem}_{i}.c").write_text(body + "\n")
        total += 1
print(f"{total} snippets")
