#!/usr/bin/env python3
"""csmapper .src -> .mps.

mkcsmapper's lex.l and yacc.y, in Python. Upstream's is a lex+yacc pair that
links against libiconv for the database factory, so it cannot be built here.
The grammar is small enough to read by hand.

    mkcsmapper.py --out <file.mps> <file.src>
    mkcsmapper.py --dir <srcdir> --outdir <dir>     every .src, and .646 copied

The output is byte-identical to upstream's; verify.py is what says so.
"""

import re
import sys
from pathlib import Path

from citrusdb import DbFactory

MAGIC = b"MAPPER\0\0"

OOB_NONIDENTICAL = 0
OOB_ILSEQ = 1

MF_TRANSLIT = 1
ROWCOL_MAX = 4


class Error(Exception):
    pass


def strip_comments(text):
    # /* */ spans lines; # and // run to the end of one. A newline is a token
    # in upstream's lexer, so the count of them has to survive.
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    return re.sub(r"(#|//).*", "", text)


TOKEN = re.compile(r'0[xX][0-9A-Fa-f]+|[1-9][0-9]*|0[0-9]*|[=/|-]|"[^"]*"|[^\s=/|-][^\s]*')


def lex(line):
    return TOKEN.findall(line)


def imm(tok):
    if not re.fullmatch(r"0[xX][0-9A-Fa-f]+|[0-9]+", tok):
        raise Error(f"not a number: {tok}")
    return int(tok, 0) & 0xFFFFFFFF


class Mapper:
    def __init__(self):
        self.name = None
        self.rowcol = []            # (begin, end, width)
        self.rowcol_bits = None
        self.rowcol_mask = 0
        self.dst_invalid = None
        self.dst_ilseq = None
        self.dst_unit_bits = None
        self.oob_mode = None
        self.table = None
        self.translit = None
        self.table_size = 0
        self.src_next = 0

    # -- properties ------------------------------------------------------

    def set_src_zone(self, toks):
        # `b - e`, or `b - e / b - e / ... / bits`.
        parts, cur = [], []
        for t in toks:
            if t == "/":
                parts.append(cur)
                cur = []
            else:
                cur.append(t)
        parts.append(cur)

        bits = 32
        if len(parts) > 1 and len(parts[-1]) == 1:
            bits = imm(parts.pop()[0])
        for p in parts:
            if len(p) != 3 or p[1] != "-":
                raise Error(f"bad SRC_ZONE range {p}")
            b, e = imm(p[0]), imm(p[2])
            if b > e or len(self.rowcol) >= ROWCOL_MAX:
                raise Error("illegal SRC_ZONE")
            self.rowcol.append((b, e, e - b + 1))

        if bits not in (8, 16, 32) or len(self.rowcol) > 32 // bits:
            raise Error(f"illegal SRC_ZONE bits {bits}")
        self.rowcol_bits = bits
        self.rowcol_mask = (1 << bits) - 1
        for _, e, _ in self.rowcol:
            if e > self.rowcol_mask:
                raise Error("SRC_ZONE past the mask")

    def parse_property(self, toks):
        k = toks[0].upper()
        v = toks[1:]
        if k == "NAME":
            self.name = v[0].strip('"')
        elif k == "TYPE":
            if v[0].upper() != "ROWCOL":
                raise Error(f"unsupported TYPE {v[0]}")
        elif k == "SRC_ZONE":
            self.set_src_zone(v)
        elif k == "DST_INVALID":
            self.dst_invalid = imm(v[0])
        elif k == "DST_ILSEQ":
            self.dst_ilseq = imm(v[0])
        elif k == "DST_UNIT_BITS":
            if imm(v[0]) not in (8, 16, 32):
                raise Error("illegal DST_UNIT_BITS")
            self.dst_unit_bits = imm(v[0])
        elif k == "OOB_MODE":
            self.oob_mode = OOB_ILSEQ if v[0].upper() == "ILSEQ" else OOB_NONIDENTICAL
        else:
            raise Error(f"unknown property {k}")

    # -- the table -------------------------------------------------------

    def setup_map(self):
        if self.rowcol_bits is None:
            raise Error("SRC_ZONE is mandatory.")
        if self.dst_unit_bits is None:
            raise Error("DST_UNIT_BITS is mandatory.")
        if self.dst_invalid is None:
            self.dst_invalid = 0xFFFFFFFF
        if self.dst_ilseq is None:
            self.dst_ilseq = 0xFFFFFFFE
        if self.oob_mode is None:
            self.oob_mode = OOB_NONIDENTICAL

        self.table_size = 1
        for _, _, w in self.rowcol:
            self.table_size *= w
        self.table = self.alloc_table()

    def unit(self, val):
        n = self.dst_unit_bits // 8
        return (val & ((1 << self.dst_unit_bits) - 1)).to_bytes(n, "big")

    def alloc_table(self):
        val = self.dst_ilseq if self.oob_mode == OOB_ILSEQ else self.dst_invalid
        return bytearray(self.unit(val) * self.table_size)

    def check_src(self, begin, end):
        if begin > end:
            return False
        if begin < end and (begin & ~self.rowcol_mask) != (end & ~self.rowcol_mask):
            return False
        i = len(self.rowcol) * self.rowcol_bits
        p = 0
        for b, e, _ in self.rowcol:
            i -= self.rowcol_bits
            m = (begin >> i) & self.rowcol_mask
            if m < b or m > e:
                return False
            p += 1
        if begin < end:
            b, e, _ = self.rowcol[-1]
            n = end & self.rowcol_mask
            if n < b or n > e:
                return False
        return True

    def store(self, begin, width, dst, inc, flags):
        ofs = 0
        i = len(self.rowcol) * self.rowcol_bits
        for b, _, w in self.rowcol:
            i -= self.rowcol_bits
            ofs = ofs * w + (((begin >> i) & self.rowcol_mask) - b)

        if flags & MF_TRANSLIT:
            if self.translit is None:
                self.translit = self.alloc_table()
            table = self.translit
        else:
            table = self.table

        n = self.dst_unit_bits // 8
        at = ofs * n
        if inc:
            for k in range(width):
                table[at:at + n] = self.unit(dst + k)
                at += n
        else:
            table[at:at + width * n] = self.unit(dst) * width

    def parse_map_line(self, toks):
        if "=" not in toks:
            raise Error(f"no '=' in map line: {toks}")
        at = toks.index("=")
        src, rest = toks[:at], toks[at + 1:]

        flags = 0
        if "/" in rest:
            at = rest.index("/")
            for f in rest[at + 1:]:
                if f == "|":
                    continue
                if f.upper() != "TRANSLIT":
                    raise Error(f"unknown flag {f}")
                flags |= MF_TRANSLIT
            rest = rest[:at]

        inc = 0
        if rest and rest[-1] == "-":
            inc, rest = 1, rest[:-1]
        if len(rest) != 1:
            raise Error(f"bad destination {rest}")
        d = rest[0].upper()
        if d == "INVALID":
            dst = self.dst_invalid
        elif d == "ILSEQ":
            dst = self.dst_ilseq
        else:
            dst = imm(rest[0])

        if not src:
            begin = end = self.src_next
        elif len(src) == 1:
            begin = end = imm(src[0])
        elif len(src) == 2 and src[0] == "-":
            begin, end = self.src_next, imm(src[1])
        elif len(src) == 3 and src[1] == "-":
            begin, end = imm(src[0]), imm(src[2])
        else:
            raise Error(f"bad source {src}")

        if not self.check_src(begin, end):
            raise Error(f"illegal zone {begin:#x}-{end:#x}")
        self.src_next = end + 1
        self.store(begin, end - begin + 1, dst, inc, flags)

    # -- output ----------------------------------------------------------

    def rowcol_info(self):
        w = [self.rowcol_bits, self.dst_invalid]
        n = len(self.rowcol)
        # Backward compatibility, as upstream spells it: one dimension pads to
        # two, and neither one nor two records a length.
        if n == 1:
            w += [0, 0]
        length = 0 if n <= 2 else n
        for b, e, _ in self.rowcol:
            w += [b, e]
        w += [self.dst_unit_bits, length]
        return b"".join(x.to_bytes(4, "big") for x in w)

    def serialize(self):
        df = DbFactory()
        df.add_string("type", "rowcol")
        df.add_by_string("info", self.rowcol_info())
        df.add_by_string("rowcol_ext_ilseq",
                         self.oob_mode.to_bytes(4, "big") + self.dst_ilseq.to_bytes(4, "big"))
        df.add_by_string("table", bytes(self.table))
        if self.translit is not None:
            df.add_by_string("translit_table", bytes(self.translit))
        return df.serialize(MAGIC)


def compile_src(text):
    m = Mapper()
    lines = strip_comments(text).splitlines()
    i = 0
    while i < len(lines):
        toks = lex(lines[i])
        i += 1
        if not toks:
            continue
        if toks[0].upper() == "BEGIN_MAP":
            break
        m.parse_property(toks)
    else:
        raise Error("no BEGIN_MAP")

    m.setup_map()
    for line in lines[i:]:
        toks = lex(line)
        if not toks:
            continue
        if toks[0].upper() == "END_MAP":
            break
        m.parse_map_line(toks)
    return m.serialize()


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
        for f in sorted(src.glob("*.src")):
            try:
                (dst / (f.stem + ".mps")).write_bytes(compile_src(f.read_text()))
            except Error as e:
                sys.exit(f"mkcsmapper.py: {f}: {e}")
        for f in sorted(src.glob("*.646")):
            (dst / f.name).write_bytes(f.read_bytes())
        return 0

    if len(args) != 1 or not out:
        sys.exit(__doc__)
    try:
        Path(out).write_bytes(compile_src(Path(args[0]).read_text()))
    except Error as e:
        sys.exit(f"mkcsmapper.py: {args[0]}: {e}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
