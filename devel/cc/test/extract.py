#!/usr/bin/env python3
"""Lift the C programs out of vcc's GoogleTest sources.

    extract.py <vcc checkout> <output directory>

Each raw string literal holding a `main` becomes <suite>/<Test>.c, where the
suite is the test file's name and the test is the TEST or TEST_F around it;
a second program in one test is <Test>_2.c, and so on. When the test holds
it to a plain string, EXPECT_EQ("...", CompileAndRun...(R"(...)")), that
string is <Test>.expect.

Left out: chapter 20, which is about register allocation, which wasm has
none of, and whose programs are glued to helpers by the test; and every
program the wasm32 fixture skips (backend/wasm/test/book_test.h), most of
them for assuming a 64-bit long.
"""

import ast
import pathlib
import re
import sys

SOURCES = [
    "backend/common/test/book/*.cpp",
    "backend/common/test/coro/*.cpp",
    "backend/common/test/alloca/*.cpp",
    "backend/wasm/test/*.cpp",
]
LEFT_OUT = {"chapter20_tests"}

TEST = re.compile(r"^TEST(?:_F)?\(\s*\w+\s*,\s*(\w+)\s*\)", re.M)
RAW = re.compile(r'R"(\w*)\((.*?)\)\1"', re.S)
EXPECT = re.compile(r'EXPECT_EQ\(\s*((?:"(?:[^"\\\n]|\\.)*"\s*)+),\s*CompileAndRun\w*\(\s*$')


def skipped(vcc):
    text = (vcc / "backend/wasm/test/book_test.h").read_text()
    return set(re.findall(r'\{\s*"(\w+)",', text))


def expected(text, at):
    """The plain string the program's output is held to, or None."""
    m = EXPECT.search(text[max(0, at - 4000) : at])
    if not m:
        return None
    try:
        return ast.literal_eval(m.group(1))
    except (SyntaxError, ValueError):
        return None


def main():
    vcc, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    skip = skipped(vcc)
    total = expects = 0
    for pattern in SOURCES:
        for path in sorted(vcc.glob(pattern)):
            if path.stem in LEFT_OUT:
                continue
            text = path.read_text()
            tests = [(m.start(), m.group(1)) for m in TEST.finditer(text)]
            suite = out / path.stem
            seen = {}
            for m in RAW.finditer(text):
                body = m.group(2)
                if not re.search(r"\bmain\s*\(", body):
                    continue
                owner = [name for at, name in tests if at < m.start()]
                if not owner or owner[-1] in skip:
                    continue
                name = owner[-1]
                seen[name] = seen.get(name, 0) + 1
                if seen[name] > 1:
                    name += f"_{seen[name]}"
                suite.mkdir(parents=True, exist_ok=True)
                (suite / f"{name}.c").write_text(body.lstrip("\n") + "\n")
                total += 1
                want = expected(text, m.start())
                if want is not None:
                    (suite / f"{name}.expect").write_text(want)
                    expects += 1
    print(f"{total} programs, {expects} with their output")


if __name__ == "__main__":
    main()
