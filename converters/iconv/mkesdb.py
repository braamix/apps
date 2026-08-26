#!/usr/bin/env python3
"""esdb .src -> .esdb.

mkesdb's lex.l and yacc.y in Python, and with them the part-expansion each
directory's BSD Makefile did: most encodings are one template .src sed into a
file per line of a .part list.

    mkesdb.py --out <file.esdb> <file.src>
    mkesdb.py --dir <esdb/CODE> --outdir <dir>      the whole directory

The output is byte-identical to upstream's; verify.py is what says so.
"""

import re
import sys
from pathlib import Path

from citrusdb import DbFactory

MAGIC = b"ESDB\0\0\0\0"
VERSION = 1


class Error(Exception):
    pass


# One row per esdb/<dir>/Makefile. `sub` is the template substitution, run over
# each line of <CODE>.src with the part spliced in; None means the directory
# ships a .src per part already.
#
#   code    the NAME stem              sep      "" where the Makefile says NO_SEP
#   eprefix False where NO_EPREFIX: the part is the whole codeset name
DIRS = {
    "APPLE":    dict(code="MAC", sep="", subdir="APPLE",
                     sub=[("changeme", "{part}")]),
    "AST":      dict(code="ARMSCII", sep="-", subdir="AST",
                     sub=[("ARMSCII-x", "ARMSCII-{part}")]),
    "BIG5":     dict(code="Big5", sep="-", subdir="BIG5", sub="big5"),
    "CP":       dict(code="CP", sep="", subdir="CP",
                     sub=[("CPx", "CP{part}")]),
    "DEC":      dict(code="DEC", sep="", subdir="DEC",
                     sub=[("DECx", "DEC{part}")]),
    "EBCDIC":   dict(code="EBCDIC", sep="-", subdir="EBCDIC",
                     sub=[("EBCDIC-x", "EBCDIC-{part}")]),
    "EUC":      dict(code="EUC", sep="-", subdir="EUC", sub=None),
    "GB":       dict(code="GB", sep="", subdir="GB", sub=None),
    "GEORGIAN": dict(code="GEORGIAN", sep="-", subdir="GEORGIAN",
                     sub=[("GEORGIANx", "GEORGIAN-{part}"),
                          ("GEORGIANy", "GEORGIAN-{part_colon}")]),
    "ISO-2022": dict(code="ISO-2022", sep="-", subdir="ISO-2022", sub=None),
    "ISO-8859": dict(code="ISO-8859", sep="-", subdir="ISO-8859",
                     sub=[("ISO-8859-x", "ISO-8859-{part}")]),
    "ISO646":   dict(code="ISO646", sep="-", subdir="ISO646",
                     sub=[("ISO646-x", "ISO646-{part}")]),
    "KAZAKH":   dict(code="KAZAKH", sep="-", subdir="KAZAKH",
                     sub=None, eprefix=False),
    "KOI":      dict(code="KOI", sep="", subdir="KOI",
                     sub=[("KOIx", "KOI{part}")]),
    "MISC":     dict(code="MISC", sep="-", subdir="MISC",
                     sub=None, eprefix=False),
    "TCVN":     dict(code="TCVN", sep="-", subdir="TCVN",
                     sub=None, eprefix=False),
    "UTF":      dict(code="UTF", sep="-", subdir="UTF", sub="utf"),
}

# esdb/UTF/Makefile's table: each part names the stdenc module and the VARIABLE
# string handed to it.
UTF_MOD = {
    "16": ("UTF1632", "utf16"),
    "16BE": ("UTF1632", "utf16,big,force"),
    "16LE": ("UTF1632", "utf16,little,force"),
    "16-INTERNAL": ("UTF1632", "utf16,internal,force"),
    "16-SWAPPED": ("UTF1632", "utf16,swapped,force"),
    "32": ("UTF1632", "utf32"),
    "32BE": ("UTF1632", "utf32,big,force"),
    "32LE": ("UTF1632", "utf32,little,force"),
    "32-INTERNAL": ("UTF1632", "utf32,internal,force"),
    "32-SWAPPED": ("UTF1632", "utf32,swapped,force"),
    "8-MAC": ("UTF8MAC", "utf8mac"),
    "8": ("UTF8", "utf8"),
    "7": ("UTF7", "utf7"),
}


def read_part(path):
    out = []
    for line in path.read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            out.append(line)
    return out


def sed_first(text, pat, rep):
    # sed 's/a/b/' without /g: the first match on each line.
    return "\n".join(line.replace(pat, rep, 1) for line in text.split("\n"))


def expand(dirname, spec, srcdir):
    """Every (codeset, source text) the directory produces."""
    code = spec["code"]
    sep = spec["sep"]
    eprefix = code + sep if spec.get("eprefix", True) else ""
    parts = read_part(srcdir / f"{code}.part")

    out = []
    for part in parts:
        name = eprefix + part
        # A per-part .src that is checked in wins over the template.
        own = srcdir / (name.replace(":", "@") + ".src")
        if own.exists():
            out.append((name, own.read_text()))
            continue

        sub = spec["sub"]
        if sub is None:
            raise Error(f"{dirname}: no source for {name}")

        if sub == "utf":
            mod, var = UTF_MOD[part]
            t = (srcdir / f"{code}.src").read_text()
            t = sed_first(t, "UTF-x", f"UTF-{part}")
            t = sed_first(t, "UTF-mod", mod)
            t = sed_first(t, "UTF-var", var)
        elif sub == "big5":
            variable = None
            for line in (srcdir / "Big5.variable").read_text().splitlines():
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                f = re.split(r"[ \t]+", line, 1)
                if f[0] == part:
                    variable = f[1].strip() if len(f) > 1 else ""
            t = (srcdir / "Big5.src").read_text()
            t = sed_first(t, "encoding", f"Big5-{part}")
            t = sed_first(t, "variable", variable or "")
        else:
            t = (srcdir / f"{code}.src").read_text()
            for pat, rep in sub:
                t = sed_first(t, pat, rep.format(
                    part=part, part_colon=part.replace("-", ":", 1)))
        out.append((name, t))
    return out


# -- the grammar ---------------------------------------------------------

TOKEN = re.compile(r'"[^"]*"|0[xX][0-9A-Fa-f]+|[0-9]+|[^\s]+')


def compile_src(text):
    name = encoding = variable = None
    invalid = None
    csids = []

    for line in text.splitlines():
        line = re.sub(r"#.*", "", line).strip()
        if not line:
            continue
        toks = TOKEN.findall(line)
        k = toks[0].upper()
        if k == "NAME":
            name = toks[1].strip('"')
        elif k == "ENCODING":
            encoding = toks[1].strip('"')
        elif k == "VARIABLE":
            variable = toks[1].strip('"')
        elif k == "DEFCSID":
            csids.append((toks[1].strip('"'), int(toks[2], 0)))
        elif k == "INVALID":
            invalid = int(toks[1], 0)
        else:
            raise Error(f"unknown property {k}")

    if not name:
        raise Error("NAME is mandatory.")
    if not encoding:
        raise Error("ENCODING is mandatory.")

    df = DbFactory()
    df.add32("version", VERSION)
    df.add_string("encoding", encoding)
    if variable is not None:
        df.add_string("variable", variable)
    if invalid is not None:
        df.add32("invalid", invalid)
    df.add32("num_charsets", len(csids))
    for i, (sym, csid) in enumerate(csids):
        df.add_string(f"csname_{i}", sym)
        df.add32(f"csid_{i}", csid)
    return df.serialize(MAGIC)


def main(argv):
    out = outdir = srcdir = None
    args = []
    i = 1
    while i < len(argv):
        a = argv[i]
        if a == "--out":
            out = argv[i + 1]; i += 2
        elif a == "--outdir":
            outdir = argv[i + 1]; i += 2
        elif a == "--dir":
            srcdir = argv[i + 1]; i += 2
        else:
            args.append(a); i += 1

    if srcdir:
        src = Path(srcdir)
        dst = Path(outdir)
        dst.mkdir(parents=True, exist_ok=True)
        spec = DIRS[src.name]
        for name, text in expand(src.name, spec, src):
            try:
                blob = compile_src(text)
            except Error as e:
                sys.exit(f"mkesdb.py: {src.name}/{name}: {e}")
            (dst / (name.replace(":", "@") + ".esdb")).write_bytes(blob)
        return 0

    if len(args) != 1 or not out:
        sys.exit(__doc__)
    try:
        Path(out).write_bytes(compile_src(Path(args[0]).read_text()))
    except Error as e:
        sys.exit(f"mkesdb.py: {args[0]}: {e}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
