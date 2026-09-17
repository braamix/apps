# unicodedata.ucd_3_2_0: the database the idna codec reads, as it answered
# before everything added and corrected since.
#
# The same program on CPython and here, byte for byte.

import unicodedata

u = unicodedata.ucd_3_2_0
print(u.unidata_version, type(u).__name__, type(u).__module__, unicodedata.UCD is type(u))

# Assigned since, changed since, and the same.
SAMPLE = ("a5 Ƞȡȴϴ҇ׄ؀۝ஃჼ឴ᴀ⁰"
          "€₰☖♭⭐〡゠ㇰ㐀龥龦侮﹅�"
          "\U00010330\U0001d400\U0001f600\U0002a6d6\U0002a6d7\U0002f868\U0002f874陋\U000e0001")
for c in SAMPLE:
    print(ascii(c), u.category(c), repr(u.bidirectional(c)), u.east_asian_width(c),
          u.combining(c), u.mirrored(c), u.decimal(c, None), u.digit(c, None),
          u.numeric(c, None), u.name(c, None), repr(u.decomposition(c)))

# The corrections, in each form.
for c in "陋\U0002f868\U0002f874\U0002f91f\U0002f95f\U0002f9bf":
    print(ascii(c), [ascii(u.normalize(f, c)) for f in ("NFC", "NFD", "NFKC", "NFKD")],
          ascii(unicodedata.normalize("NFD", c)))
print(ascii(u.normalize("NFKC", "ⅠÅẛ̣\U0001d400Ƞ")),
      u.is_normalized("NFC", "Å"), u.is_normalized("NFD", "Å"))

# No aliases and no named sequences, but the plain names all the same.
for name in ["LATIN SMALL LETTER A", "grinning face", "LATIN CAPITAL LETTER GHA",
             "KEYCAP NUMBER SIGN", "HANGUL SYLLABLE GAG", "CJK UNIFIED IDEOGRAPH-4E00"]:
    try:
        print(name, ascii(u.lookup(name)))
    except KeyError as e:
        print(name, "KeyError", e)

for bad in (lambda: u.category("ab"), lambda: u.numeric("x"), lambda: unicodedata.UCD()):
    try:
        bad()
    except (TypeError, ValueError) as e:
        print(type(e).__name__)
