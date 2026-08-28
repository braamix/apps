#!/usr/bin/env python3
# mainmenu -> mainmenu-default.h, the MainMenu[] body. Was make-mainmenu.pl.

import re
import sys

LINE = re.compile(r'^\s*(\w+)(?:\s+("[^"]+")(?:\s+(\S+)(.*))?)?')
NAME = re.compile(r"^([^(]*)(?:\((.*)\))?$")


def unquote(arg):
    arg = re.sub(r"(^|[^\\])_", r"\1 ", arg)
    return arg.replace("\\_", "_")


def main(src, dst):
    out = []
    with open(src) as f:
        for line in f:
            if line.startswith("#"):
                continue
            m = LINE.match(line)
            if not m:
                continue
            kind, text, action, options = m.group(1), m.group(2), m.group(3), m.group(4) or ""

            arg = ""
            if action:
                a = NAME.match(action)
                action = "A_" + a.group(1).upper().replace("-", "_")
                if a.group(2):
                    arg = ',{(char*)"%s"}' % unquote(a.group(2))

            hide = ""
            conds = []
            for o in options.split():
                if o == "hide":
                    hide = "|HIDE"
                else:
                    conds.append("MENU_COND_" + o.upper().replace("-", "_"))
            conds = ("|" + "|".join(conds)) if conds else ""

            if kind == "submenu":
                out.append("{(char*)%s, SUBM%s}," % (text, conds))
            elif kind == "function":
                assert action, line
                out.append("{(char*)%s, FUNC%s%s, { %s%s } }," % (text, hide, conds, action, arg))
            elif kind == "end":
                out.append("{NULL},")
            elif kind == "hline":
                out.append('{(char*)"---"},')
    with open(dst, "w") as f:
        f.write("\n".join(out) + "\n")


main(sys.argv[1], sys.argv[2])
