# NormalizationTest.txt, which pyunicode.mjs --full turns into normdata.py.

import unicodedata as u
from normdata import ROWS
bad = 0
for c1, c2, c3, c4, c5 in ROWS:
    want = (c2, c3, c4, c5)
    for src in (c1, c2, c3):
        if u.normalize("NFC", src) != c2 or u.normalize("NFD", src) != c3:
            bad += 1
            if bad < 5: print("canon", ascii(src))
    for src in (c1, c2, c3, c4, c5):
        if u.normalize("NFKC", src) != c4 or u.normalize("NFKD", src) != c5:
            bad += 1
            if bad < 5: print("compat", ascii(src))
    if not u.is_normalized("NFC", c2) or not u.is_normalized("NFKD", c5):
        bad += 1
print(len(ROWS), "rows", bad, "bad")
