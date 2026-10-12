#!/usr/bin/env python3
"""Lift the C programs out of vcc's GoogleTest sources.

    extract.py <vcc checkout> <output directory>

Each raw string literal holding a `main` becomes <suite>/<Test>.c, where the
suite is the test file's name and the test is the TEST or TEST_F around it;
a second program in one test is <Test>_2.c, and so on.
"""

import pathlib
import re
import sys

SOURCES = [
    "backend/common/test/book/*.cpp",
    "backend/common/test/coro/*.cpp",
    "backend/common/test/alloca/*.cpp",
    "backend/wasm/test/*.cpp",
]

TEST = re.compile(r"^TEST(?:_F)?\(\s*\w+\s*,\s*(\w+)\s*\)", re.M)
RAW = re.compile(r'R"(\w*)\((.*?)\)\1"', re.S)


def main():
    vcc, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    total = 0
    for pattern in SOURCES:
        for path in sorted(vcc.glob(pattern)):
            text = path.read_text()
            tests = [(m.start(), m.group(1)) for m in TEST.finditer(text)]
            suite = out / path.stem
            seen = {}
            for m in RAW.finditer(text):
                body = m.group(2)
                if not re.search(r"\bmain\s*\(", body):
                    continue
                owner = [name for at, name in tests if at < m.start()]
                if not owner:
                    continue
                name = owner[-1]
                seen[name] = seen.get(name, 0) + 1
                if seen[name] > 1:
                    name += f"_{seen[name]}"
                suite.mkdir(parents=True, exist_ok=True)
                (suite / f"{name}.c").write_text(body.lstrip("\n") + "\n")
                total += 1
    print(f"{total} programs")


if __name__ == "__main__":
    main()
