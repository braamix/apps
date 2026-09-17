# Every codepoint through the regular expression engine, as a digest per block
# of 256: the classes in both modes, and matching regardless of case.

import re
M = (1 << 61) - 1
def mix(h, s):
    for ch in s:
        h = (h * 1000003 + ord(ch)) % M
    return (h * 31 + 7) % M
CLASSES = [re.compile(p) for p in (r"\w", r"\d", r"\s", r"\W", r"\b.", r"(?a)\w", r"(?a)\s")]
for blk in range(0, 0x110000, 256):
    text = "".join(map(chr, range(blk, blk + 256)))
    h = 0
    for p in CLASSES:
        h = mix(h, ",".join(str(m.start()) for m in p.finditer(text)))
    for c in text:
        for other in (c.lower(), c.upper()):
            if len(other) == 1:
                h = mix(h, "1" if re.fullmatch(re.escape(other), c, re.I) else "0")
                h = mix(h, "1" if re.fullmatch("[" + re.escape(other) + "]", c, re.I) else "0")
    print("%06X %d" % (blk, h))
