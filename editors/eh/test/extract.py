#!/usr/bin/env python3
"""Derive this port's test data from upstream's, once.

Upstream drives 138 cases as `printf 'keys' | eh file >a.out` from a 1400-line
make file, asserting two goldens each: a terminfo escape trace and the file the
run wrote. Braam emits no escapes, so the traces cannot be diffed -- but they
replay. The fake `textterm` terminal's capabilities are readable tokens, so
replaying one into a 24x80 character image plus a reverse-video mask recovers
what upstream's own binary drew.

That replay is checked, not trusted: upstream ships every case twice, from two
curses libraries whose escape streams differ a lot, and a case is only written
out when both replay to the same image.

Writes cases.json, golden/<name>.img and data/<name>.txt.
Run: python3 test/extract.py [path/to/upstream/eh]
"""

import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
UP = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "tmp", "eh")
UPT = os.path.join(UP, "test")

ROWS, COLS = 24, 80

# Cases that cannot carry over, and why.
SKIP = {
    "test": "the harness's own entry point",
    "tests": "the harness's own entry point",
    "tests_have_term_data": "checks for a TERM directory",
    "tests_skipped": "upstream's own skip list",
    "term0": "TERM=BOGUS; there is no terminal type here",
    "bang6": "pipes through fmt(1), which Braam has not got",
    "bang7": "pipes through fmt(1), which Braam has not got",
    "all": "a make target",
    "clean": "a make target",
    "distclean": "a make target",
    "clobber": "a make target",
}


# --------------------------------------------------------------- printf(1)

OCTAL = re.compile(r"\\([0-7]{1,3})")
NAMED = {
    "a": "\a", "b": "\b", "f": "\f", "n": "\n",
    "r": "\r", "t": "\t", "v": "\v", "\\": "\\", "e": "\x1b",
}


def printf_escapes(s):
    """What printf(1) does to its format: named escapes and \\ooo octal."""
    out = []
    i = 0
    while i < len(s):
        c = s[i]
        if c != "\\" or i + 1 >= len(s):
            out.append(c)
            i += 1
            continue
        m = OCTAL.match(s, i)
        if m:
            out.append(chr(int(m.group(1), 8)))
            i = m.end()
            continue
        n = s[i + 1]
        if n in NAMED:
            out.append(NAMED[n])
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


# What make does to a recipe line before the shell sees it. `$$` is a literal
# dollar; a bare `$` names a variable, and an undefined one expands to nothing.
# replace4's `/$/plugh!/a` is therefore `//plugh!/a` -- an empty pattern, not
# `$` -- which is what upstream's own golden records.
MAKE_VARS = {"T": "test", "DATA": "test/data"}


def make_expand(s):
    out = []
    i = 0
    while i < len(s):
        if s[i] != "$":
            out.append(s[i])
            i += 1
            continue
        if i + 1 >= len(s):
            out.append("$")
            break
        c = s[i + 1]
        if c == "$":
            out.append("$")
            i += 2
        elif c in "({":
            close = ")" if c == "(" else "}"
            j = s.find(close, i + 2)
            if j < 0:
                out.append("$")
                i += 1
                continue
            out.append(MAKE_VARS.get(s[i + 2 : j], ""))
            i = j + 1
        else:
            out.append(MAKE_VARS.get(c, ""))
            i += 2
    return "".join(out)


def unquote(word):
    """One shell word, single- or double-quoted, as make left it."""
    if len(word) >= 2 and word[0] == word[-1] and word[0] in "'\"":
        return word[1:-1]
    return word


def split_quoted(line):
    """Shell words, keeping quotes so unquote() can see which kind."""
    out, cur, q = [], "", None
    for c in line:
        if q:
            cur += c
            if c == q:
                q = None
        elif c in "'\"":
            q = c
            cur += c
        elif c.isspace():
            if cur:
                out.append(cur)
                cur = ""
        else:
            cur += c
    if cur:
        out.append(cur)
    return out


# ------------------------------------------------------------ the makefile


def read_targets(path):
    """{name: [recipe lines]} for every target with a recipe."""
    # Latin-1, so a byte is a character: the scripts mix printf escapes, which
    # are bytes, with literal UTF-8 (the mb* cases type an emoji). The runner
    # turns the whole script back into bytes and decodes it once.
    targets, cur = {}, None
    with open(path, "rb") as f:
        text = f.read().decode("latin-1")
    for raw in text.split("\n"):
        if raw.startswith("\t"):
            if cur is not None:
                targets[cur].append(raw[1:])
            continue
        m = re.match(r"^([A-Za-z_][\w.$/{}-]*)\s*:[^=]?", raw)
        cur = m.group(1) if m else None
        if cur is not None and cur not in targets:
            targets[cur] = []
    return targets


def fixture(path):
    """One of upstream's input files. Latin-1, so a byte is a character and the
    deliberately malformed UTF-8 the imb* cases feed in survives JSON."""
    p = os.path.join(UP, path)
    with open(p, "rb") as f:
        return f.read().decode("latin-1")


def map_path(p):
    """Upstream's own relative path, kept: it is what the status line shows.
    The runner chdirs to /tmp and plants these under it."""
    return p.replace("${T}", "test").replace("$T", "test")


def parse_case(name, recipe):
    """One case, or None when the shape is not the usual one."""
    setup, script, argv, image, wants_file, extra = [], None, [], False, False, []

    for line in recipe:
        s = line.lstrip("-@").strip()
        if not s or s.startswith("${TITLE}") or s.startswith("${PASS}"):
            continue
        if s == "${A_CORE}" or s == "${RESET}":
            continue
        if s == "${A_OUT}":
            image = True
            continue
        if s == "${A_TXT}":
            wants_file = True
            continue

        # printf '...' | ${PROG} [file] >a.out
        m = re.match(r"^printf\s+(.*?)\s*\|\s*\$\{PROG\}\s*(.*?)\s*>a\.out$", s)
        if m:
            if script is not None:
                # mark0/mark1 run the editor twice; not the usual shape.
                return None
            script = printf_escapes(unquote(make_expand(m.group(1))))
            rest = m.group(2).strip()
            argv = [map_path(rest)] if rest else []
            continue

        # printf '...' >file
        m = re.match(r"^printf\s+(.*?)\s*>\s*(\S+)$", s)
        if m:
            setup.append({
                "path": map_path(m.group(2)),
                "text": printf_escapes(unquote(make_expand(m.group(1)))),
            })
            continue

        # cp src dst
        m = re.match(r"^cp\s+(\S+)\s+(\S+)$", s)
        if m:
            src = m.group(1).replace("${DATA}", "test/data").replace("$@", name)
            try:
                setup.append({"path": map_path(m.group(2)), "text": fixture(src)})
            except OSError:
                return None
            continue

        extra.append(s)

    if script is None:
        return None

    # Braam's rootfs has no fmt(1).
    if "fmt " in script:
        return None

    # Upstream's checked-in fixtures have no setup line; plant whichever the
    # case names, so the runner needs nothing but cases.json. Only what lives
    # under test/: a.txt is the scratch file, and a case that does not create
    # it means to start with no file at all. A fixture can be named in the
    # script rather than in argv -- read1 types `Rtest/short.txt`.
    named = [a for a in argv if a.startswith("test/")]
    named += re.findall(r"test/[\w.-]+", script)
    for path in named:
        if any(x["path"] == path for x in setup):
            continue
        try:
            setup.insert(0, {"path": path, "text": fixture(path)})
        except OSError:
            pass

    case = {"name": name, "setup": setup, "argv": argv, "script": script}
    if image:
        case["image"] = True
    if wants_file:
        case["file"] = "a.txt"
    if extra:
        case["extra"] = extra
    return case


# ------------------------------------------------------------- the replayer

# Every capability but <CR> and <SMSO> is followed by a literal \r\n that
# belongs to the capability, not to the screen; it has to be swallowed.
TOK = re.compile(r"<(BEL|CR|NL|IND|CLEAR|ED|EL|CUB1|CUD1|HOME|RMSO|SMSO|(\d+);(\d+))>(\r\n)?")


def replay(text):
    """One textterm trace as (24 rows of text, 24 rows of reverse-video mask)."""
    img = [[" "] * COLS for _ in range(ROWS)]
    att = [[0] * COLS for _ in range(ROWS)]
    y = x = so = i = 0

    def clamp():
        nonlocal y, x
        y = 0 if y < 0 else ROWS - 1 if y >= ROWS else y
        x = 0 if x < 0 else x

    while i < len(text):
        m = TOK.match(text, i)
        if m:
            i = m.end()
            t = m.group(1)
            if m.group(2) is not None:
                y, x = int(m.group(2)) - 1, int(m.group(3)) - 1
            elif t == "CR":
                x = 0
            elif t in ("NL", "IND"):
                y += 1
                if t == "NL":
                    x = 0
            elif t == "CLEAR":
                img = [[" "] * COLS for _ in range(ROWS)]
                att = [[0] * COLS for _ in range(ROWS)]
                y = x = 0
            elif t == "ED":
                clamp()
                for r in range(y, ROWS):
                    for c in range(x if r == y else 0, COLS):
                        img[r][c], att[r][c] = " ", 0
            elif t == "EL":
                clamp()
                for c in range(x, COLS):
                    img[y][c], att[y][c] = " ", 0
            elif t == "CUB1":
                x -= 1
            elif t == "CUD1":
                y += 1
            elif t == "HOME":
                y = x = 0
            elif t == "SMSO":
                so = 1
            elif t == "RMSO":
                so = 0
            clamp()
            continue

        ch = text[i]
        i += 1
        if ch == "\r":
            x = 0
            continue
        if ch == "\n":
            y += 1
            clamp()
            continue
        clamp()
        if x < COLS:
            img[y][x], att[y][x] = ch, so
        x += 1
        if x >= COLS:
            x, y = 0, y + 1
        clamp()
    return img, att


def as_golden(img, att):
    rows = ["".join(r).rstrip() for r in img]
    mask = ["".join("#" if a else "." for a in r).rstrip(".") for r in att]
    return "\n".join(rows) + "\n---\n" + "\n".join(mask) + "\n"


def golden_for(name):
    """The image both curses implementations agree on, or None."""
    seen = []
    for imp in ("NCurses", "Curses"):
        p = os.path.join(UPT, imp, "textterm", name + ".out")
        if not os.path.exists(p):
            continue
        with open(p, "rb") as f:
            seen.append(as_golden(*replay(f.read().decode("utf-8", errors="surrogateescape"))))
    if not seen:
        return None, "no trace"
    if len(set(seen)) != 1:
        return None, "the two curses traces replay differently"
    return seen[0], None


# ------------------------------------------------------------------- main


def main():
    targets = read_targets(os.path.join(UPT, "Makefile"))
    os.makedirs(os.path.join(HERE, "golden"), exist_ok=True)
    os.makedirs(os.path.join(HERE, "data"), exist_ok=True)

    cases, dropped = [], []
    for name, recipe in targets.items():
        if name in SKIP:
            dropped.append((name, SKIP[name]))
            continue
        if not any("${PROG}" in l for l in recipe):
            continue
        case = parse_case(name, recipe)
        if case is None:
            dropped.append((name, "not the usual recipe shape"))
            continue

        if case.get("image"):
            img, why = golden_for(name)
            if img is None:
                dropped.append((name, why))
                case.pop("image")
            else:
                with open(os.path.join(HERE, "golden", name + ".img"), "w",
                          encoding="utf-8", errors="surrogateescape") as f:
                    f.write(img)

        if case.get("file"):
            src = os.path.join(UPT, "data", name + ".txt")
            if os.path.exists(src):
                with open(src, "rb") as f:
                    raw = f.read()
                with open(os.path.join(HERE, "data", name + ".txt"), "wb") as f:
                    f.write(raw)
            else:
                case.pop("file")

        if case.get("image") or case.get("file"):
            cases.append(case)
        else:
            dropped.append((name, "nothing left to assert"))

    cases.sort(key=lambda c: c["name"])
    with open(os.path.join(HERE, "cases.json"), "w") as f:
        json.dump(cases, f, indent=1)
        f.write("\n")

    print(f"{len(cases)} cases")
    for name, why in sorted(dropped):
        print(f"  dropped {name}: {why}")


if __name__ == "__main__":
    main()
