#!/usr/bin/env python3
"""stamp.py's stand-in under `make LINKER=ld`, taking its arguments.

ld writes the braam section itself, so this writes nothing. It checks that the
section is last and holds what stamp.py would have written.
"""
import argparse
import re
import struct
import sys
from pathlib import Path

MAGIC = 0x6D617262


def leb(data: bytes, at: int):
    value, shift = 0, 0
    while True:
        byte = data[at]
        at += 1
        value |= (byte & 0x7F) << shift
        shift += 7
        if not byte & 0x80:
            return value, at


def last_section(data: bytes):
    at, last = 8, None
    while at < len(data):
        sid = data[at]
        size, body = leb(data, at + 1)
        last, at = (sid, data[body : body + size]), body + size
    return last


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("binary", type=Path)
    ap.add_argument("--flags", type=int, default=0)
    ap.add_argument("--initial-pages", type=int, required=True)
    ap.add_argument("--max-pages", type=int, required=True)
    ap.add_argument("--sysabi", type=Path, required=True)
    args = ap.parse_args(argv[1:])
    abi = int(re.search(r"PROC_ABI\s*=\s*(\d+)", args.sysabi.read_text()).group(1))
    want = struct.pack("<IIIII", MAGIC, abi, args.flags, args.initial_pages, args.max_pages)
    sid, body = last_section(args.binary.read_bytes())
    n, at = leb(body, 0) if sid == 0 else (0, 0)
    if sid != 0 or body[at : at + n] != b"braam" or body[at + n :] != want:
        sys.exit(f"stamped.py: {args.binary}: the braam section is not stamp.py's "
                 f"({want.hex()})")


if __name__ == "__main__":
    main(sys.argv)
