# Every codepoint as unicodedata.ucd_3_2_0 answers it, as a digest per block
# of 256: pyunicode.mjs --full runs it under the host CPython and here.

import unicodedata
u = unicodedata.ucd_3_2_0
M = (1 << 61) - 1
def mix(h, s):
    for ch in s:
        h = (h * 1000003 + ord(ch)) % M
    return (h * 31 + 7) % M
print(u.unidata_version)
for blk in range(0, 0x110000, 256):
    h = 0
    for cp in range(blk, blk + 256):
        c = chr(cp)
        s = "|".join([u.category(c), u.bidirectional(c), u.east_asian_width(c), str(u.combining(c)),
            str(u.mirrored(c)), str(u.decimal(c, -1)), str(u.digit(c, -1)), repr(u.numeric(c, -1)),
            u.name(c, "-"), u.decomposition(c),
            u.normalize("NFC", c), u.normalize("NFD", c), u.normalize("NFKC", c), u.normalize("NFKD", c),
            str(u.is_normalized("NFC", c)), str(u.is_normalized("NFKD", c))])
        h = mix(h, s)
    print("%06X %d" % (blk, h))
