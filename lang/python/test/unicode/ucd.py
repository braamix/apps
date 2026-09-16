# unicodedata: every function, the names both ways, and normalization.
#
# The same program on CPython and here, byte for byte.

import unicodedata as u

print(u.unidata_version)

SAMPLE = ("aZ5 _\x00\x7f\xa0\xad\xb2\xbd\xdf\u0130\u01c5\u0345\u0360\u03a3\u0660"
          "\u0f33\u1100\u2028\u2160\u2460\u3000\u3007\u4e00\uac01\ud7a3\ue000"
          "\uf900\ufb01\ufdfa\ufeff\uffff\U00010400\U0001d7ce\U0001f600\U00013460"
          "\U00018b00\U0001b170\U0002f800\U000e0100\U0010ffff")
for c in SAMPLE:
    print(ascii(c), u.category(c), repr(u.bidirectional(c)), u.east_asian_width(c),
          u.combining(c), u.mirrored(c), u.decimal(c, None), u.digit(c, None),
          u.numeric(c, None), u.name(c, None), repr(u.decomposition(c)))

# Names back to characters: plain, algorithmic, aliases, sequences, and case.
for name in ["LATIN SMALL LETTER A", "latin small letter a", "SPACE",
             "HANGUL SYLLABLE GAG", "hangul syllable ga", "HANGUL SYLLABLE A",
             "CJK UNIFIED IDEOGRAPH-4E00", "cjk unified ideograph-2a700",
             "CJK COMPATIBILITY IDEOGRAPH-F900", "EGYPTIAN HIEROGLYPH-13460",
             "NULL", "BYTE ORDER MARK", "LINE FEED", "LATIN CAPITAL LETTER GHA",
             "LATIN CAPITAL LETTER A WITH MACRON AND GRAVE",
             "KHMER CONSONANT SIGN COENG KA", "TANGUT IDEOGRAPH-17000"]:
    try:
        print(repr(name), ascii(u.lookup(name)))
    except KeyError as e:
        print(repr(name), "KeyError", e)
for bad in ["NOPE", "", "LATIN SMALL LETTER A ", "CJK UNIFIED IDEOGRAPH-04E00",
            "CJK UNIFIED IDEOGRAPH-", "HANGUL SYLLABLE ", "HANGUL SYLLABLE GAX", "x" * 300]:
    try:
        u.lookup(bad)
    except KeyError as e:
        print("KeyError", str(e)[:60])
print(ascii(u.lookup(b"EM DASH")))

# Every function refuses what is not one character.
for fn in [u.category, u.bidirectional, u.combining, u.mirrored, u.decomposition,
           u.east_asian_width, u.name, u.decimal, u.digit, u.numeric]:
    for arg in ["", "ab", 5]:
        try:
            fn(arg)
        except TypeError as e:
            print(e)
for fn, msg in [(u.decimal, "a"), (u.digit, "a"), (u.numeric, "a"), (u.name, "\x00")]:
    try:
        fn(msg)
    except ValueError as e:
        print(e)

# Normalization, over text in which the forms differ.
TEXTS = ["caf\xe9", "cafe\u0301", "\u1e9b\u0323", "\ufb01ance", "\u2460\u2461",
         "\uac00\u11a8", "\u1100\u1161\u11a8", "a\u0328\u0301\u0306\u0323b",
         "\u212b", "\u0344", "\u00c5\u030a", "\u0958", "\U0001d15e", "\u3300",
         "\u0f73\u0f75", "x\u05ae\u0300\u0301y", "\u1100\uac00\u11a8",
         "\u0915\u093c", "\u2126", "\u00bd"]
for t in TEXTS:
    print(ascii(t), [ascii(u.normalize(f, t)) for f in ("NFC", "NFD", "NFKC", "NFKD")],
          [u.is_normalized(f, t) for f in ("NFC", "NFD", "NFKC", "NFKD")])
s = "plain"
print(u.normalize("NFC", s) is s, u.normalize("NFKD", ""))
for form, arg in [("NFX", "a"), ("nfc", "a"), (5, "a"), ("NFC", 5)]:
    try:
        u.normalize(form, arg)
    except (ValueError, TypeError) as e:
        print(type(e).__name__, e)

# A digest over two whole planes' worth of blocks: whatever differs, shows.
M = (1 << 61) - 1
for lo, hi in [(0x0, 0x800), (0x1e00, 0x2200), (0xa600, 0xa800), (0x1f100, 0x1f300)]:
    h = 0
    for cp in range(lo, hi):
        c = chr(cp)
        for part in (u.category(c), u.bidirectional(c), u.east_asian_width(c),
                     u.decomposition(c), u.name(c, "-"), str(u.combining(c)),
                     str(u.numeric(c, -1))):
            for ch in part:
                h = (h * 1000003 + ord(ch)) % M
    print(hex(lo), hex(hi), h)
