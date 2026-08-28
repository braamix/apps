#!/usr/bin/env python3
# action-name-func -> action-name-func.h, the ActionNameProcTable[] body.
# Was make-action-name-func.pl.

import sys


def main(src, dst):
    out = []
    with open(src) as f:
        for line in f:
            if line.startswith("#"):
                continue
            w = line.split()
            if not w:
                continue
            out.append('\t{"%s", %s},' % (w[0], w[1]))
    with open(dst, "w") as f:
        f.write("\n".join(out) + "\n")


main(sys.argv[1], sys.argv[2])
