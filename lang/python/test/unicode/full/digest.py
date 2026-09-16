# Every codepoint, as a digest per block of 256: pyunicode.mjs --full runs it
# under the host CPython and here, and compares.

import unicodedata as u
lo = 0
hi = 0x110000
M = (1 << 61) - 1
def mix(h, s):
    for ch in s:
        h = (h * 1000003 + ord(ch)) % M
    return (h * 31 + 7) % M
for blk in range(lo, hi, 256):
    h = 0
    for cp in range(blk, blk + 256):
        c = chr(cp)
        s = "|".join([u.category(c), u.bidirectional(c), u.east_asian_width(c), str(u.combining(c)),
            str(u.mirrored(c)), str(u.decimal(c, -1)), str(u.digit(c, -1)), repr(u.numeric(c, -1)),
            u.name(c, "-"), u.decomposition(c),
            c.lower(), c.upper(), c.title(), c.casefold(), c.swapcase(), c.capitalize(),
            str(c.isalpha()), str(c.isdecimal()), str(c.isdigit()), str(c.isnumeric()), str(c.isalnum()),
            str(c.islower()), str(c.isupper()), str(c.istitle()), str(c.isspace()), str(c.isprintable()),
            str(c.isidentifier()), str(("a" + c).isidentifier()), repr(c),
            u.normalize("NFC", c), u.normalize("NFD", c), u.normalize("NFKC", c), u.normalize("NFKD", c),
            str(len(("x" + c + "y").splitlines())), str(len(("x" + c + "y").split())),
            ("a" + c + "Σ").lower(), ("aΣ" + c).lower()])
        h = mix(h, s)
        n = u.name(c, None)
        if n is not None:
            try:
                if u.lookup(n) != c: h = mix(h, "LOOKUP")
            except KeyError:
                h = mix(h, "NOLOOKUP")
    print("%06X %d" % (blk, h))
