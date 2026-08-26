#!/usr/bin/env python3
"""The index containers: esdb.dir, esdb.alias, mapper.dir -> .db, and
charset.pivot -> .pvdb.

citrus_lookup_factory.c and citrus_pivot_factory.c in Python. The readers try
the container first and fall back to a linear scan of the text, so both forms
ship.

    mkdb.py --lookup <in> <out.db>
    mkdb.py --pivot  <in> <out.pvdb>
"""

import re
import sys
from pathlib import Path

from citrusdb import DbFactory

LOOKUP_MAGIC = b"LOOKUP\0\0"
PIVOT_MAGIC = b"CSPIVOT\0"
PIVOT_SUB_MAGIC = b"CSPIVSUB"


def fields(line):
    """A line cut at its comment, then split on whitespace."""
    line = line.split("#", 1)[0]
    return re.split(r"[ \t]+", line.strip()) if line.strip() else []


def lookup(text):
    df = DbFactory()
    for line in text.split("\n"):
        f = fields(line)
        if not f:
            continue
        # The key folds to lower; the data is the rest of the line, trimmed.
        rest = line.split("#", 1)[0].strip()
        data = rest[len(f[0]):].strip()
        df.add_string(f[0].lower(), data)
    return df.serialize(LOOKUP_MAGIC)


def pivot(text):
    # Grouped by first field, case-insensitively, in first-seen order.
    order, groups = [], {}
    for line in text.split("\n"):
        f = fields(line)
        if len(f) < 3:
            continue
        k = f[0].lower()
        if k not in groups:
            order.append((k, f[0]))
            groups[k] = DbFactory()
        groups[k].add32(f[1], int(f[2], 0))

    df = DbFactory()
    for k, name in order:
        df.add_by_string(name, groups[k].serialize(PIVOT_SUB_MAGIC))
    return df.serialize(PIVOT_MAGIC)


def main(argv):
    if len(argv) != 4 or argv[1] not in ("--lookup", "--pivot"):
        sys.exit(__doc__)
    text = Path(argv[2]).read_text()
    Path(argv[3]).write_bytes(
        lookup(text) if argv[1] == "--lookup" else pivot(text))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
