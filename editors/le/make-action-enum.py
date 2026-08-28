#!/usr/bin/env python3
# action-name-func -> action-enum.h. Was make-action-enum.pl.

import sys

BASE = 1024


def main(src, dst):
    out = ["enum Action {", "\tA__FIRST=%d," % BASE]
    n = BASE
    with open(src) as f:
        for line in f:
            if line.startswith("#"):
                continue
            w = line.split()
            if not w:
                continue
            out.append("\tA_%s=%d," % (w[0].upper().replace("-", "_"), n))
            n += 1
    out.append("\tA__LAST=%d," % (n - 1))
    out.append("\tMOUSE_ACTION=2048,")
    out.append("\tWINDOW_RESIZE=2049,")
    out.append("\tNO_ACTION=2050,")
    out.append("};")
    with open(dst, "w") as f:
        f.write("\n".join(out) + "\n")


main(sys.argv[1], sys.argv[2])
