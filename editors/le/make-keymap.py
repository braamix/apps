#!/usr/bin/env python3
# keymap-default -> keymap-default.h, the DefaultActionCodeTable[] body.
# Was make-keymap.pl.

import re
import sys

NAME = re.compile(r"^([^(]*)(?:\((.*)\))?$")


def unquote(arg):
    # An unescaped _ is a space; \_ is a literal one.
    arg = re.sub(r"(^|[^\\])_", r"\1 ", arg)
    return arg.replace("\\_", "_")


def main(src, dst):
    out = []
    with open(src) as f:
        for line in f:
            w = line.split()
            if not w:
                continue
            name, code = w[0], w[1]
            m = NAME.match(name)
            action = "A_" + m.group(1).upper().replace("-", "_")
            arg = m.group(2) or ""
            if arg:
                arg = ',(char*)"%s"' % unquote(arg)
            code = re.sub(r"\\([$|])", r"\\\\\1", code)
            out.append('\t{%s,(char*)"%s"%s},' % (action, code, arg))
    with open(dst, "w") as f:
        f.write("\n".join(out) + "\n")


main(sys.argv[1], sys.argv[2])
