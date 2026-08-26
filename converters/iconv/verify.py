#!/usr/bin/env python3
"""Compare a generated i18n tree against upstream's prebuilt one.

    verify.py <generated> <upstream-i18n>

The generators exist because upstream's mkcsmapper and mkesdb are lex+yacc that
link against libiconv and cannot be built here. This is what says they agree:
every file upstream ships and any index names must come out byte for byte.

Needs tmp/citrus-iconv/i18n, which is gitignored — a developer check, not part
of `make test`.

Two of upstream's files are deliberately not produced:
  - esdb/GB/GB2312-80.esdb is a stale build artifact. GB.part does not list it
    and esdb.dir does not name it, so nothing can ever resolve to it.
  - README describes upstream's tree, and is not payload.
"""

import sys
from pathlib import Path

from citrusdb import parse

NOT_PRODUCED = {"esdb/GB/GB2312-80.esdb", "README"}


def explain(got, want):
    """Why two containers differ, in their own terms rather than as hex."""
    if len(got) != len(want):
        return f"size {len(got)} vs {len(want)}"
    try:
        gm, ge = parse(got)
        wm, we = parse(want)
    except Exception:
        return "bytes differ"
    if gm != wm:
        return f"magic {gm!r} vs {wm!r}"
    if len(ge) != len(we):
        return f"{len(ge)} entries vs {len(we)}"
    for i, (a, b) in enumerate(zip(ge, we)):
        if a != b:
            if a["key"] != b["key"]:
                return f"entry {i}: key {a['key']!r} vs {b['key']!r}"
            return f"entry {i} ({a['key'].decode()}): data or chain differs"
    return "bytes differ outside the entries"


def main(argv):
    if len(argv) != 3:
        sys.exit(__doc__)
    gen, ref = Path(argv[1]), Path(argv[2])

    same = diff = missing = 0
    extra = []
    problems = []

    for f in sorted(ref.rglob("*")):
        if not f.is_file():
            continue
        rel = str(f.relative_to(ref))
        if rel in NOT_PRODUCED:
            continue
        g = gen / rel
        if not g.exists():
            missing += 1
            problems.append(f"missing  {rel}")
            continue
        a, b = g.read_bytes(), f.read_bytes()
        if a == b:
            same += 1
        else:
            diff += 1
            problems.append(f"differs  {rel}: {explain(a, b)}")

    for f in sorted(gen.rglob("*")):
        if f.is_file() and not (ref / f.relative_to(gen)).exists():
            extra.append(str(f.relative_to(gen)))

    print(f"identical {same}, differing {diff}, missing {missing}, extra {len(extra)}")
    for p in problems[:20]:
        print("  " + p)
    for e in extra[:20]:
        print("  extra    " + e)

    return 1 if (diff or missing or extra) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
