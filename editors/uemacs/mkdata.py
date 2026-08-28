#!/usr/bin/env python3
"""The files uemacs looks for at run time, as a C++ table.

A package's non-bin payload lands under a path carrying the version, which the
program would then have to know; adventure compiles its data in for the same
reason.  fileio.cpp serves these when nothing on disk answers, so a user's own
.emacsrc still wins.

Bytes go out as three-digit octal escapes, which nothing following can extend
the way a hex escape can.

Usage: mkdata.py <out.cpp> <name>=<path> ...
"""

import sys

out, pairs = sys.argv[1], sys.argv[2:]

PLAIN = set(range(0x20, 0x7f)) - set(b'"\\?')


def literal(data):
    lines, cur = [], ""
    for b in data:
        cur += chr(b) if b in PLAIN else "\\%03o" % b
        if b == 0x0a or len(cur) > 68:
            lines.append(cur)
            cur = ""
    if cur:
        lines.append(cur)
    return "\n".join('\t"%s"' % l for l in lines) or '\t""'


with open(out, "w") as f:
    f.write('#include "builtin.h"\n\n')
    for i, p in enumerate(pairs):
        name, path = p.split("=", 1)
        data = open(path, "rb").read()
        f.write("/* %s */\nstatic const char text_%d[] =\n%s;\n\n"
                % (name, i, literal(data)))
    f.write("const struct builtin_file builtin_files[] = {\n")
    for i, p in enumerate(pairs):
        name = p.split("=", 1)[0]
        f.write('\t{ "%s", text_%d, sizeof(text_%d) - 1 },\n' % (name, i, i))
    f.write("\t{ 0, 0, 0 }\n};\n")
