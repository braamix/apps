# str by Unicode's rules: the full case mappings, the predicates by category,
# whitespace and line breaks, the number grammars and repr.
#
# The same program on CPython and here, byte for byte.

WORDS = ["stra\xdfe", "\ufb01ne", "\u0130stanbul", "\u0149", "\u03b0",
         "\u1f80\u1fb3", "O\u03a3O\u03a3", "\u03a3", "a\u03a3", "a\u03a3b",
         "a\u03a3.", "a.\u03a3", "a\u03a3\u0345", "\u03a3a", "A\u0345\u03a3",
         "\u01c4\u01c5\u01c6x", "\u10d0\u10e1", "\u13a0\uab70", "\u0587",
         "hello WORLD", "\U00010400\U00010428", "\u1e9e", "\u2126\u212a",
         "d\u017ez", "o'neil van-der \u00e9t\u00e9", "\u0149x", "\xb5\u0131"]
for w in WORDS:
    print(ascii(w), [ascii(f(w)) for f in (str.lower, str.upper, str.title, str.casefold,
                                            str.capitalize, str.swapcase)])

PREDICATES = (str.isalpha, str.isalnum, str.isdecimal, str.isdigit, str.isnumeric,
              str.isspace, str.isprintable, str.islower, str.isupper, str.istitle,
              str.isidentifier, str.isascii)
for w in ["", "a", "\xdf", "\u01c5", "\u01c5a", "A\u01c5", "\u0660", "\xb2", "\xbd",
          "\u2160", "\u3007", "\u2028", "\x85", "\xa0", "\u200b", "\xad",
          "\u1680", "ab c", "_x1", "1x", "\u2118", "\u212e", "x\u00b7", "\u0345",
          "\u02b0", "\ua7f8", "Hello World", "Hello world", "\u00c9t\u00e9",
          "\U0001d7ce", "\ud800", "\U000e0100", "x\U000e0100", " "]:
    print(ascii(w), "".join("T" if p(w) else "." for p in PREDICATES))

# Whitespace and line breaks are Unicode's.
S = "\u2003a\x1cb\x85c\u2028d\t e\u3000 "
print(S.split(), S.split(None, 2), S.rsplit(None, 2), ascii(S.strip()))
print([ascii(x) for x in S.splitlines()], [ascii(x) for x in S.splitlines(True)])
print([ascii(x) for x in "a\r\nb\rc\x0bd\x0ce\x1df\x1eg\u2029h".splitlines()])
print(ascii("\xa0x\xa0".strip()), ascii("\xa0x\xa0".lstrip()), ascii("\u200bx".strip()))
print(ascii("a\u2003b".partition(" ")), "a\u2003b".split(" "))

# The number grammars read any script's digits, and any whitespace.
print(int("\u0661\u0662"), int("\u2003-\u0967\u0968\u3000"), int("\u0e51_\u0e52"),
      int("\uff11\uff10", 16), float("\u0664.\u0665e\u0661"), float("\xa0-1.5\u2028"),
      complex("\u0661+\u0662j"), int("\U0001d7ce\U0001d7cf"))
for bad in ["\xb2", "1\xbd", "\u2155", "\u0661x", "", " ", "\u2003"]:
    try:
        int(bad)
    except ValueError as e:
        print(e)
    try:
        float(bad)
    except ValueError as e:
        print(e)
print(int(b" 12 "), float(b"1.5"))
try:
    int(b"\xb2")
except ValueError as e:
    print(e)

# repr escapes what Unicode calls unprintable, and nothing else.
print(repr("a\xa0b\xadc\u200bd\u2028e\ud800f\U000e0001g\U0001f600h\u0378"))
print(ascii("\xe9\u20ac\U0001f600"), repr(["\x85", "\u3000", "\u1680"]))

# Surrogates are characters until something encodes them.
s = "a\ud800\udc00b"
print(len(s), ascii(s[1]), ascii(s[2]), s.find("\udc00"), ascii(s.upper()), ascii(chr(0xdfff)))
print(ascii("\ud800" + "\udc00"), "\ud800" < "\ue000", "\ud7ff" < "\ud800",
      sorted(["\U00010000", "\ud800", "\uffff"]) == ["\ud800", "\uffff", "\U00010000"])
print(ascii(str.maketrans({"\ud800": "x"})), ascii("a\ud800".translate({0xd800: 0x41})))
