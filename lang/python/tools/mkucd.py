#!/usr/bin/env python3
"""Write the Unicode tables: ucddb.h and ucddb.cpp.

    tools/mkucd.py            write them, and say how big each table is
    tools/mkucd.py --fetch    first download the UCD files it reads

Two sources, and neither is committed. Everything the host's `unicodedata`
and `str` can answer comes from them, codepoint by codepoint, so the tables
agree with the CPython every golden was written by. What they cannot list --
the name aliases, the named sequences, the simple case mappings and the
composition exclusions -- comes from the UCD files of the same version, under
tmp/ucd/<version>/.

The tables are checked before they are written: each is decoded again here and
compared with what it was made from.
"""

import argparse
import os
import re
import sys
import unicodedata
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
VERSION = unicodedata.unidata_version
UCD = os.path.join(ROOT, "tmp", "ucd", VERSION)
FILES = ["UnicodeData.txt", "NameAliases.txt", "NamedSequences.txt",
         "DerivedNormalizationProps.txt"]
NCP = 0x110000

CATEGORIES = ["Cn", "Lu", "Ll", "Lt", "Lm", "Lo", "Mn", "Mc", "Me", "Nd", "Nl", "No",
              "Pc", "Pd", "Ps", "Pe", "Pi", "Pf", "Po", "Sm", "Sc", "Sk", "So",
              "Zs", "Zl", "Zp", "Cc", "Cf", "Cs", "Co"]
BIDI = ["", "L", "LRE", "LRO", "R", "AL", "RLE", "RLO", "PDF", "EN", "ES", "ET", "AN",
        "CS", "NSM", "BN", "B", "S", "WS", "ON", "LRI", "RLI", "FSI", "PDI"]
WIDTHS = ["N", "Na", "A", "W", "H", "F"]
TAGS = ["", "<font>", "<noBreak>", "<initial>", "<medial>", "<final>", "<isolated>",
        "<circle>", "<super>", "<sub>", "<vertical>", "<wide>", "<narrow>", "<small>",
        "<square>", "<fraction>", "<compat>"]

# The record's flag bits; ucd.h names the same ones.
F_ALPHA, F_DECIMAL, F_DIGIT, F_NUMERIC = 1, 2, 4, 8
F_LOWER, F_UPPER, F_TITLE, F_SPACE = 16, 32, 64, 128
F_LINEBREAK, F_PRINTABLE, F_XID_START, F_XID_CONTINUE = 256, 512, 1024, 2048
F_CASED, F_CASE_IGNORABLE, F_MIRRORED = 4096, 8192, 16384

HANGUL = (0xAC00, 0xD7A3)
LOOKUP_ONLY = ["TANGUT IDEOGRAPH-"]


def die(msg):
    sys.exit(f"mkucd: {msg}")


def fetch():
    os.makedirs(UCD, exist_ok=True)
    for f in FILES:
        url = f"https://www.unicode.org/Public/{VERSION}/ucd/{f}"
        print(f"mkucd: {url}")
        with urllib.request.urlopen(url) as r, open(os.path.join(UCD, f), "wb") as out:
            out.write(r.read())


def ucd_lines(name):
    path = os.path.join(UCD, name)
    if not os.path.isfile(path):
        die(f"no {path} -- run with --fetch")
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if line:
                yield [x.strip() for x in line.split(";")]


# ------------------------------------------------------------------ sources

def simple_cases():
    """cp -> (upper, lower, title), from UnicodeData.txt."""
    out = {}
    first = None
    for f in ucd_lines("UnicodeData.txt"):
        cp = int(f[0], 16)
        up = int(f[12], 16) if f[12] else cp
        lo = int(f[13], 16) if f[13] else cp
        ti = int(f[14], 16) if f[14] else up
        if (up, lo, ti) != (cp, cp, cp):
            out[cp] = (up, lo, ti)
    return out


def exclusions():
    out = set()
    for f in ucd_lines("DerivedNormalizationProps.txt"):
        if f[1] != "Full_Composition_Exclusion":
            continue
        lo, _, hi = f[0].partition("..")
        for cp in range(int(lo, 16), int(hi or lo, 16) + 1):
            out.add(cp)
    return out


def aliases():
    return [(f[1], int(f[0], 16)) for f in ucd_lines("NameAliases.txt")]


def sequences():
    return [(f[0], [int(x, 16) for x in f[1].split()]) for f in ucd_lines("NamedSequences.txt")]


def case_ignorable(c, cased):
    # Final_Sigma is the one place str shows this property: a sigma after a
    # cased letter is final unless something that is not ignorable follows.
    if cased:
        return ("aΣ" + c).lower()[1] == "ς"
    return ("a" + c + "Σ").lower()[-1] == "ς"


def record_of(cp, simple):
    c = chr(cp)
    u = unicodedata
    cat = u.category(c)
    flags = 0
    lower, upper = c.islower(), c.isupper()
    cased = lower or upper or cat == "Lt"
    for bit, on in ((F_ALPHA, c.isalpha()), (F_DECIMAL, c.isdecimal()),
                    (F_DIGIT, c.isdigit()), (F_NUMERIC, c.isnumeric()),
                    (F_LOWER, lower), (F_UPPER, upper), (F_TITLE, cat == "Lt"),
                    (F_SPACE, c.isspace()),
                    (F_LINEBREAK, len(("a" + c + "b").splitlines()) == 2),
                    (F_PRINTABLE, c.isprintable()),
                    (F_XID_START, c.isidentifier() and c != "_"),
                    (F_XID_CONTINUE, ("a" + c).isidentifier()),
                    (F_CASED, cased), (F_CASE_IGNORABLE, case_ignorable(c, cased)),
                    (F_MIRRORED, u.mirrored(c) == 1)):
        if on:
            flags |= bit
    dec = u.decimal(c, 0)
    dig = u.digit(c, 0)
    num = u.numeric(c, None)
    up, lo, ti = simple.get(cp, (cp, cp, cp))
    full = (c.lower(), c.upper(), c.title(), c.casefold())
    return (CATEGORIES.index(cat), BIDI.index(u.bidirectional(c)),
            WIDTHS.index(u.east_asian_width(c)), u.combining(c), flags, dec, dig,
            num, (lo - cp, up - cp, ti - cp), full)


# ------------------------------------------------------------------ packing

def split_once(t, shift):
    size = 1 << shift
    blocks, index1, index2 = {}, [], []
    for i in range(0, len(t), size):
        b = tuple(t[i:i + size])
        at = blocks.get(b)
        if at is None:
            at = len(index2) >> shift
            blocks[b] = at
            index2.extend(b)
        index1.append(at)
    return index1, index2


def width_of(values):
    return 1 if max(values) < 256 else 2 if max(values) < 65536 else 4


def splitbins(t, width):
    """t -> (s1, s2, j1, j2, i2): three levels, with
        blk = j2[(j1[i >> (s1 + s2)] << s2) + ((i >> s1) & m2)]
        t[i] = i2[(blk << s1) + (i & m1)]
    at the pair of shifts that makes the whole smallest."""
    best = None
    for s1 in range(3, 10):
        i1, i2 = split_once(t, s1)
        for s2 in range(2, 10):
            j1, j2 = split_once(i1, s2)
            cost = len(j1) * width_of(j1) + len(j2) * width_of(j2) + len(i2) * width
            if best is None or cost < best[0]:
                best = (cost, s1, s2, j1, j2, i2)
    _, s1, s2, j1, j2, i2 = best
    m1, m2 = (1 << s1) - 1, (1 << s2) - 1
    for i in range(len(t)):
        blk = j2[(j1[i >> (s1 + s2)] << s2) + ((i >> s1) & m2)]
        assert i2[(blk << s1) + (i & m1)] == t[i]
    return s1, s2, j1, j2, i2


def lookup3(tabs, i):
    s1, s2, j1, j2, i2 = tabs
    blk = j2[(j1[i >> (s1 + s2)] << s2) + ((i >> s1) & ((1 << s2) - 1))]
    return i2[(blk << s1) + (i & ((1 << s1) - 1))]


def put_trie(out, hdr, name, tabs, what):
    s1, s2, j1, j2, i2 = tabs
    for suffix, arr, comment in (("_1", j1, f"codepoint >> {s1 + s2} -> first block"),
                                 ("_2", j2, f"first block and (codepoint >> {s1}) & {(1 << s2) - 1} -> second block"),
                                 ("_3", i2, f"second block and codepoint & {(1 << s1) - 1} -> {what}")):
        t, w = ctype_for(arr)
        out.array(t, name + suffix, arr, w, comment)
        hdr.append(f"extern const {t} {name}{suffix}[{len(arr)}];\n")
    hdr.append(f"constexpr u32 {name}_SHIFT1 = {s1};\n")
    hdr.append(f"constexpr u32 {name}_SHIFT2 = {s2};\n")


def utf16(cps):
    out = []
    for cp in cps:
        if cp < 0x10000:
            out.append(cp)
        else:
            cp -= 0x10000
            out += [0xD800 | (cp >> 10), 0xDC00 | (cp & 0x3FF)]
    return out


def from_utf16(units):
    out = []
    i = 0
    while i < len(units):
        u = units[i]
        if 0xD800 <= u < 0xDC00:
            out.append(0x10000 + ((u - 0xD800) << 10) + (units[i + 1] - 0xDC00))
            i += 2
        else:
            out.append(u)
            i += 1
    return out


class Out:
    def __init__(self):
        self.parts = []
        self.sizes = []

    def array(self, ctype, name, values, width, comment):
        self.sizes.append((name, len(values) * width))
        self.parts.append(f"// {comment}\nextern const {ctype} {name}[{len(values)}] = {{\n")
        line = "   "
        for v in values:
            item = f" {v},"
            if len(line) + len(item) > 99:
                self.parts.append(line + "\n")
                line = "   "
            line += item
        self.parts.append(line + "\n};\n\n")

    def text(self):
        return "".join(self.parts)


def ctype_for(values):
    lo, hi = min(values), max(values)
    if lo >= 0 and hi < 256:
        return "u8", 1
    if lo >= 0 and hi < 65536:
        return "u16", 2
    if lo >= -(1 << 31) and hi < (1 << 31):
        return ("i32", 4) if lo < 0 else ("u32", 4)
    die("a value past 32 bits")


# ------------------------------------------------------------------ the names

def algorithmic(name, cp):
    m = re.match(r"^(.*-)([0-9A-F]{4,6})$", name)
    return m.group(1) if m and int(m.group(2), 16) == cp else None


def names_tables(out, hdr):
    named = {}
    ranges = []           # [prefix, lo, hi]
    prefixes = []
    for cp in range(NCP):
        n = unicodedata.name(chr(cp), None)
        if n is None or HANGUL[0] <= cp <= HANGUL[1]:
            continue
        p = algorithmic(n, cp)
        if p is None:
            named[cp] = n
            continue
        if p not in prefixes:
            prefixes.append(p)
        pi = prefixes.index(p)
        if ranges and ranges[-1][0] == pi and ranges[-1][2] == cp - 1:
            ranges[-1][2] = cp
        else:
            ranges.append([pi, cp, cp])

    # The words, the most frequent first so they take one byte.
    freq = {}
    for n in named.values():
        for w in n.split(" "):
            freq[w] = freq.get(w, 0) + 1
    words = sorted(freq, key=lambda w: (-freq[w], w))
    ids = {w: i for i, w in enumerate(words)}
    if len(words) > 0xC0 + 64 * 256:
        die("too many words for a two-byte id")

    lex, lex_off = [], []
    for i, w in enumerate(words):
        if i % 16 == 0:
            lex_off.append(len(lex))
        b = w.encode("ascii")
        assert 0 < len(b) < 256
        lex.append(len(b))
        lex.extend(b)

    order = sorted(named)
    blob, name_off, runs = [], [], []
    for k, cp in enumerate(order):
        if k % 32 == 0:
            name_off.append(len(blob))
        ws = named[cp].split(" ")
        assert len(ws) < 256
        blob.append(len(ws))
        for w in ws:
            i = ids[w]
            if i < 0xC0:
                blob.append(i)
            else:
                i -= 0xC0
                blob += [0xC0 + (i >> 8), i & 0xFF]
        if runs and runs[-1][0] + runs[-1][1] == cp:
            runs[-1][1] += 1
        else:
            runs.append([cp, 1, k])

    # Decoded again, every one.
    def word(i):
        at = lex_off[i // 16]
        for _ in range(i % 16):
            at += 1 + lex[at]
        return bytes(lex[at + 1:at + 1 + lex[at]]).decode()

    for k, cp in enumerate(order):
        at = name_off[k // 32]
        for _ in range(k % 32):
            n = blob[at]
            at += 1
            for _ in range(n):
                at += 2 if blob[at] >= 0xC0 else 1
        n = blob[at]
        at += 1
        got = []
        for _ in range(n):
            b = blob[at]
            if b >= 0xC0:
                i = 0xC0 + ((b - 0xC0) << 8 | blob[at + 1])
                at += 2
            else:
                i = b
                at += 1
            got.append(word(i))
        assert " ".join(got) == named[cp], hex(cp)

    out.array("u8", "UCD_LEX", lex, 1, "the words of the names, each after its length")
    out.array("u32", "UCD_LEX_AT", lex_off, 4, "where every sixteenth word starts")
    out.array("u8", "UCD_NAMES", blob, 1, "each name: a word count, then one- or two-byte word ids")
    out.array("u32", "UCD_NAMES_AT", name_off, 4, "where every thirty-second name starts")
    flat = []
    for cp, n, k in runs:
        flat += [cp, n, k]
    out.array("u32", "UCD_NAME_RUNS", flat, 4, "runs of named codepoints: first, count, name number")

    # What lookup() takes and name() does not answer: 3.14 reads a Tangut
    # ideograph's name without writing one.
    for p in LOOKUP_ONLY:
        if p not in prefixes:
            prefixes.append(p)
        pi = prefixes.index(p)
        for cp in range(NCP):
            try:
                hit = unicodedata.lookup("%s%04X" % (p, cp)) == chr(cp)
            except KeyError:
                continue
            if not hit or unicodedata.name(chr(cp), None) is not None:
                continue
            if ranges and ranges[-1][0] == pi and ranges[-1][2] == cp - 1 and ranges[-1][3] == 0:
                ranges[-1][2] = cp
            else:
                ranges.append([pi, cp, cp, 0])
    alg = []
    for pi, lo, hi, *tail in ranges:
        alg += [lo, hi, pi | (0 if tail == [0] else 0x100)]
    out.array("u32", "UCD_ALGO", alg, 4,
              "codepoints named prefix-HEX: first, last, prefix | 0x100 where name() answers")
    hdr.append("extern const Str UCD_ALGO_PREFIX[%d];\n" % len(prefixes))
    out.parts.append("extern const Str UCD_ALGO_PREFIX[%d] = {\n" % len(prefixes))
    for p in prefixes:
        out.parts.append(f'    "{p}",\n')
    out.parts.append("};\n\n")

    # Aliases and named sequences: a name, and what it stands for.
    als = aliases()
    seqs = sequences()
    text, idx = [], []
    for name, cp in als:
        assert unicodedata.lookup(name) == chr(cp), name
        idx += [len(text), len(name), cp]
        text.extend(name.encode("ascii"))
    seq_units = []
    sidx = []
    for name, cps in seqs:
        assert unicodedata.lookup(name) == "".join(map(chr, cps)), name
        units = utf16(cps)
        sidx += [len(text), len(name), len(seq_units), len(units)]
        text.extend(name.encode("ascii"))
        seq_units += units
    out.array("u8", "UCD_EXTRA_TEXT", text, 1, "the alias and sequence names")
    out.array("u32", "UCD_ALIASES", idx, 4, "alias: text offset, length, codepoint")
    out.array("u32", "UCD_SEQS", sidx, 4, "named sequence: text offset, length, unit offset, units")
    out.array("u16", "UCD_SEQ_UNITS", seq_units, 2, "the sequences, in UTF-16")

    hdr.append(f"constexpr u32 UCD_NAMED = {len(order)};\n")
    hdr.append(f"constexpr u32 UCD_WORDS = {len(words)};\n")
    hdr.append(f"constexpr u32 UCD_LONGEST_NAME = {max(map(len, list(named.values()) + [a for a, _ in als] + [s for s, _ in seqs] + ['HANGUL SYLLABLE XXXXXXXXXX', 'CJK COMPATIBILITY IDEOGRAPH-10FFFF']))};\n")
    return {"UCD_LEX": lex, "UCD_LEX_AT": lex_off, "UCD_NAMES": blob,
            "UCD_NAMES_AT": name_off, "UCD_NAME_RUNS": flat, "UCD_ALGO": alg,
            "UCD_EXTRA_TEXT": text, "UCD_ALIASES": idx, "UCD_SEQS": sidx,
            "UCD_SEQ_UNITS": seq_units}


# ------------------------------------------------------------------ main

def old_version(out, hdr):
    """unicodedata.ucd_3_2_0, as what it answers otherwise than this version:
    the ranges 3.2.0 had not assigned, a record for each character whose
    answers differ, and the decompositions corrected since. Every answer is
    the host's own ucd_3_2_0, and is checked against it through the tables."""
    old = unicodedata.ucd_3_2_0
    new = unicodedata
    SAME, NONE = 0xFF, 0xFE
    unassigned, recs, norms, nums = [], [], [], []
    for cp in range(NCP):
        c = chr(cp)
        if old.category(c) == "Cn" and new.category(c) != "Cn":
            if unassigned and unassigned[-1][1] == cp - 1:
                unassigned[-1][1] = cp
            else:
                unassigned.append([cp, cp])
            continue
        # Unassigned in both: 3.2.0 answers as this version does.
        assert old.bidirectional(c) == new.bidirectional(c) or new.category(c) != "Cn", hex(cp)
        rec = [SAME] * 6
        if old.category(c) != new.category(c):
            rec[0] = CATEGORIES.index(old.category(c))
        if old.bidirectional(c) != new.bidirectional(c):
            rec[1] = BIDI.index(old.bidirectional(c))
        if old.east_asian_width(c) != new.east_asian_width(c):
            rec[2] = WIDTHS.index(old.east_asian_width(c))
        if old.mirrored(c) != new.mirrored(c):
            rec[3] = old.mirrored(c)
        if old.decimal(c, None) != new.decimal(c, None):
            v = old.decimal(c, None)
            rec[4] = NONE if v is None else v
        if old.numeric(c, None) != new.numeric(c, None):
            v = old.numeric(c, None)
            if v is not None and v not in nums:
                nums.append(v)
            rec[5] = NONE if v is None else nums.index(v)
        if rec != [SAME] * 6:
            recs.append([cp] + rec)
        # Everything else is the same, but where a decomposition was corrected.
        assert old.combining(c) == new.combining(c), hex(cp)
        assert old.decomposition(c) == new.decomposition(c), hex(cp)
        assert old.digit(c, None) == new.digit(c, None), hex(cp)
        if old.normalize("NFD", c) != new.normalize("NFD", c):
            d = old.normalize("NFD", c)
            assert len(d) == 1, hex(cp)
            norms.append((cp, ord(d)))
    if len(nums) >= NONE:
        die("the 3.2.0 numbers outgrew their index")

    flat = [x for r in unassigned for x in r]
    out.array("u32", "UCD_OLD_UNASSIGNED", flat, 4,
              "Unicode 3.2.0: first and last of each run it had not assigned; sorted")
    hdr.append(f"extern const u32 UCD_OLD_UNASSIGNED[{len(flat)}];\n")
    out.sizes.append(("UCD_OLD_RECS", len(recs) * 12))
    out.parts.append("// Unicode 3.2.0: codepoint, then category, bidi, width, mirrored,\n")
    out.parts.append("// decimal and numeric as it had them; 255 is as now, 254 is none\n")
    out.parts.append(f"extern const UcdOld UCD_OLD_RECS[{len(recs)}] = {{\n")
    for r in recs:
        out.parts.append("    { 0x%04X, %d, %d, %d, %d, %d, %d },\n" % tuple(r))
    out.parts.append("};\n\n")
    hdr.append(f"extern const UcdOld UCD_OLD_RECS[{len(recs)}];\n")
    out.sizes.append(("UCD_OLD_NUMBERS", len(nums) * 8))
    out.parts.append(f"extern const f64 UCD_OLD_NUMBERS[{len(nums)}] = {{\n")
    for v in nums:
        out.parts.append(f"    {float(v)!r},\n")
    out.parts.append("};\n\n")
    hdr.append(f"extern const f64 UCD_OLD_NUMBERS[{len(nums)}];\n")
    flat = [x for r in norms for x in r]
    out.array("u32", "UCD_OLD_NORM", flat, 4,
              "Unicode 3.2.0: codepoint, and the one it decomposed to before a correction")
    hdr.append(f"extern const u32 UCD_OLD_NORM[{len(flat)}];\n")

    # Decode them again, and ask every question of every codepoint.
    import bisect
    starts = [r[0] for r in unassigned]
    by_cp = {r[0]: r[1:] for r in recs}
    for cp in range(NCP):
        c = chr(cp)
        i = bisect.bisect_right(starts, cp) - 1
        if i >= 0 and unassigned[i][0] <= cp <= unassigned[i][1]:
            assert old.category(c) == "Cn" and old.bidirectional(c) == ""
            assert old.east_asian_width(c) == WIDTHS[0] and old.mirrored(c) == 0
            assert old.decimal(c, None) is None and old.numeric(c, None) is None
            assert old.combining(c) == 0 and old.decomposition(c) == ""
            assert old.name(c, None) is None, hex(cp)
            continue
        r = by_cp.get(cp, [SAME] * 6)
        pick = lambda k, now, tab: now if r[k] == SAME else tab(r[k])
        assert old.category(c) == pick(0, new.category(c), CATEGORIES.__getitem__)
        assert old.bidirectional(c) == pick(1, new.bidirectional(c), BIDI.__getitem__)
        assert old.east_asian_width(c) == pick(2, new.east_asian_width(c), WIDTHS.__getitem__)
        assert old.mirrored(c) == pick(3, new.mirrored(c), int)
        assert old.decimal(c, None) == pick(4, new.decimal(c, None),
                                            lambda v: None if v == NONE else v)
        assert old.numeric(c, None) == pick(5, new.numeric(c, None),
                                            lambda v: None if v == NONE else nums[v])
        assert old.name(c, None) == new.name(c, None), hex(cp)
    return len(unassigned), len(recs), len(norms)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fetch", action="store_true")
    args = ap.parse_args()
    if args.fetch:
        fetch()

    simple = simple_cases()
    excl = exclusions()

    out = Out()
    hdr = []

    # The per-codepoint record and the case table behind it.
    recs, rec_ids = [], {}
    cases, case_ids = [(0, 0, 0, 0, 0, 0, 0)], {(0, 0, 0, 0, 0, 0, 0): 0}
    ext = [0]
    ext_ids = {}
    nums, num_ids = [], {}
    rec_of = [0] * NCP

    def ext_of(cps):
        key = tuple(cps)
        if key not in ext_ids:
            ext_ids[key] = len(ext)
            ext.append(len(cps))
            ext.extend(cps)
        return ext_ids[key]

    for cp in range(NCP):
        cat, bidi, width, comb, flags, dec, dig, num, deltas, full = record_of(cp, simple)
        lo, up, ti = deltas
        fl, fu, ft, ff = (list(map(ord, s)) for s in full)
        xl = 0 if fl == [cp + lo] else ext_of(fl)
        xu = 0 if fu == [cp + up] else ext_of(fu)
        xt = 0 if ft == [cp + ti] else ext_of(ft)
        xf = 0 if ff == fl else ext_of(ff)
        case = (lo, up, ti, xl, xu, xt, xf)
        if case not in case_ids:
            case_ids[case] = len(cases)
            cases.append(case)
        ni = 0
        if num is not None:
            if num not in num_ids:
                num_ids[num] = len(nums)
                nums.append(num)
            ni = num_ids[num]
        rec = (cat, bidi, width, comb, flags, dec, dig, ni, case_ids[case])
        if rec not in rec_ids:
            rec_ids[rec] = len(recs)
            recs.append(rec)
        rec_of[cp] = rec_ids[rec]
    if len(cases) >= 65536 or len(ext) >= 65536 or len(nums) >= 256:
        die("a table outgrew its index")

    rec_trie = splitbins(rec_of, 2)
    put_trie(out, hdr, "UCD_REC", rec_trie, "record")

    out.sizes.append(("UCD_RECS", len(recs) * 12))
    out.parts.append(f"// cat, bidi, width, combining, flags, decimal, digit, numeric, casing\n")
    out.parts.append(f"extern const UcdRec UCD_RECS[{len(recs)}] = {{\n")
    for r in recs:
        out.parts.append("    { %d, %d, %d, %d, %d, %d, %d, %d, %d },\n" % r)
    out.parts.append("};\n\n")
    hdr.append(f"extern const UcdRec UCD_RECS[{len(recs)}];\n")

    out.sizes.append(("UCD_CASES", len(cases) * 20))
    out.parts.append("// simple lower, upper, title as deltas; full lower, upper, title, fold\n")
    out.parts.append("// as offsets into UCD_EXT, where 0 means the simple one (the lower one\n")
    out.parts.append("// for fold)\n")
    out.parts.append(f"extern const UcdCase UCD_CASES[{len(cases)}] = {{\n")
    for c in cases:
        out.parts.append("    { %d, %d, %d, %d, %d, %d, %d },\n" % c)
    out.parts.append("};\n\n")
    hdr.append(f"extern const UcdCase UCD_CASES[{len(cases)}];\n")
    out.array("u32", "UCD_EXT", ext, 4, "a full case mapping: its length, then the codepoints")
    hdr.append(f"extern const u32 UCD_EXT[{len(ext)}];\n")

    out.sizes.append(("UCD_NUMBERS", len(nums) * 8))
    out.parts.append(f"extern const f64 UCD_NUMBERS[{len(nums)}] = {{\n")
    for v in nums:
        out.parts.append(f"    {float(v)!r},\n")
    out.parts.append("};\n\n")
    hdr.append(f"extern const f64 UCD_NUMBERS[{len(nums)}];\n")

    # Decomposition: tag and length in one unit, then the codepoints in UTF-16.
    dec, dec_of, dec_ids = [0], [0] * NCP, {}
    pairs = []
    for cp in range(NCP):
        d = unicodedata.decomposition(chr(cp))
        if not d:
            continue
        parts = d.split()
        tag = ""
        if parts[0].startswith("<"):
            tag = parts.pop(0)
        cps = [int(x, 16) for x in parts]
        units = [(TAGS.index(tag) << 8) | len(cps)] + utf16(cps)
        key = tuple(units)
        if key not in dec_ids:
            dec_ids[key] = len(dec)
            dec.extend(units)
        dec_of[cp] = dec_ids[key]
        if not tag and len(cps) == 2 and cp not in excl:
            pairs.append((cps[0], cps[1], cp))
            assert unicodedata.normalize("NFC", unicodedata.normalize("NFD", chr(cp))) == chr(cp)
    if len(dec) >= 65536:
        die("the decomposition table outgrew its index")
    dec_trie = splitbins(dec_of, 2)
    put_trie(out, hdr, "UCD_DEC", dec_trie, "decomposition")
    out.array("u16", "UCD_DEC", dec, 2, "a decomposition: tag << 8 | count, then UTF-16")
    hdr.append(f"extern const u16 UCD_DEC[{len(dec)}];\n")

    pairs.sort()
    flat = []
    for a, b, c in pairs:
        flat += [a, b, c]
    out.array("u32", "UCD_COMPOSE", flat, 4, "first, second, composite; sorted")
    hdr.append(f"extern const u32 UCD_COMPOSE[{len(flat)}];\n")

    olds = old_version(out, hdr)

    tables = names_tables(out, hdr)
    for name, arr in tables.items():
        t = {"UCD_LEX": "u8", "UCD_NAMES": "u8", "UCD_EXTRA_TEXT": "u8",
             "UCD_SEQ_UNITS": "u16"}.get(name, "u32")
        hdr.append(f"extern const {t} {name}[{len(arr)}];\n")

    # Check the records against their source once more, through the tables.
    for cp in range(NCP):
        r = recs[lookup3(rec_trie, cp)]
        want = record_of(cp, simple)
        assert r[:7] == want[:7], hex(cp)
        assert (want[7] is not None) == bool(r[4] & F_NUMERIC), hex(cp)
        if want[7] is not None:
            assert nums[r[7]] == want[7], hex(cp)
        c = cases[r[8]]
        full = want[9]
        for delta, x, s in ((c[0], c[3], full[0]), (c[1], c[4], full[1]), (c[2], c[5], full[2])):
            got = [cp + delta] if not x else ext[x + 1:x + 1 + ext[x]]
            assert got == list(map(ord, s)), hex(cp)
        got = (ext[c[6] + 1:c[6] + 1 + ext[c[6]]] if c[6]
               else ([cp + c[0]] if not c[3] else ext[c[3] + 1:c[3] + 1 + ext[c[3]]]))
        assert got == list(map(ord, full[3])), hex(cp)
    for cp in range(NCP):
        at = lookup3(dec_trie, cp)
        d = unicodedata.decomposition(chr(cp))
        if not at:
            assert not d, hex(cp)
            continue
        head = dec[at]
        n = head & 0xFF
        units, k = [], at + 1
        for _ in range(n):
            wide = 0xD800 <= dec[k] < 0xDC00
            units += dec[k:k + 1 + wide]
            k += 1 + wide
        text = " ".join("%04X" % x for x in from_utf16(units))
        tag = TAGS[head >> 8]
        assert (tag + " " + text if tag else text) == d, hex(cp)

    hdr_text = (
        "// Generated by tools/mkucd.py from Unicode %s; do not edit.\n"
        "#pragma once\n\n#include \"ucd.h\"\n\n" % VERSION + "".join(hdr))
    body = ("// Generated by tools/mkucd.py from Unicode %s; do not edit.\n"
            "#include \"ucddb.h\"\n\n" % VERSION + out.text())
    with open(os.path.join(ROOT, "ucddb.h"), "w") as f:
        f.write(hdr_text)
    with open(os.path.join(ROOT, "ucddb.cpp"), "w") as f:
        f.write(body)

    total = 0
    for name, size in out.sizes:
        print(f"{size:9}  {name}")
        total += size
    print(f"{total:9}  in all, Unicode {VERSION}: {len(recs)} records, {len(cases)} casings, "
          f"{len(dec_ids)} decompositions, {len(pairs)} compositions; 3.2.0 is "
          f"{olds[0]} runs unassigned, {olds[1]} records and {olds[2]} corrections")


if __name__ == "__main__":
    main()
