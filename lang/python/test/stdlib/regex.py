# re over the native _sre: the pattern, the match, the scanner, the template,
# and the errors, as CPython's own re/ reports them.
import copy
import re

p = re.compile(r"(?P<word>\w+)\s+(?P<num>\d+)?")
m = p.search("  hello 42 world")
print(p, p.pattern, p.groups, dict(p.groupindex), p.flags == re.UNICODE)
print(m, m.group(), m.group("word", 2), m.groups(), m.groupdict(), m["num"], m[1])
print(m.span(), m.start("num"), m.end(2), m.lastindex, m.lastgroup, m.regs, m.pos, m.endpos)
print(m.re is p, m.string, m.expand(r"<\g<num>:\1>"), m.expand(b"x" and r"\g<0>!"))
m2 = p.match("abc ")
print(m2.groups(), m2.groups("-"), m2.groupdict("?"), m2.span("num"), m2.group(2))
print(p.match("  x"), p.prefixmatch("x 1"), p.fullmatch("x 1"), p.fullmatch("x 1 "))
print(p.search("q 7 r 8", 2), p.search("q 7 r 8", 1, 3), p.match("  hi 5", pos=2, endpos=5))

print(re.findall(r"\d+", "a1b22c333"), re.findall(r"(\w)(\d)", "a1 b2"), re.findall(r"x*", "axb"))
print([mm.span() for mm in re.finditer(r"\b\w", "one two  three")])
print(re.split(r"\W+", "Words, words, words."), re.split(r"(\W+)", "a, b", maxsplit=1))
print(re.split(r"x*", "axbc"), re.split(r"^$", "foo\n\nbar\n", flags=re.M))
print(re.sub(r"(\w+) (\w+)", r"\2 \1", "hello world, x y"), re.subn(r"a", "-", "banana", count=2))
print(re.sub(r"\d", lambda mm: str(int(mm.group()) * 2), "a1b2c9"), re.sub("x*", "-", "abxd"))
print(re.sub(r"(?P<y>\d{4})-(?P<m>\d\d)", r"\g<m>/\g<y>", "2024-05 and 1999-12"))
print(re.sub(r"a", r"\n\t\\", "cat"), repr(re.sub("b", "\\\\x", "abc")), re.sub(r"(b)|c", r"[\1]", "abc"))
print(re.escape("1.5*[a-z]+ ~#"), re.escape(b"a.b"), re.purge())

print(re.match(r"(?i)straße", "STRASSE"), re.match(r"(?i)k", "K"), re.match("(?i)[a-c]+", "AbC"))
print(re.findall(r"^\w+$", "one\ntwo", re.M), re.match(r"a.b", "a\nb", re.S), re.match(r"a.b", "a\nb"))
print(re.match(r"""(?x) \d+   # digits
                   \s* [a-z]+  # letters""", "12 abc"))
print(re.findall(r"\w+", "naïve café", re.A), re.findall(rb"\w+", b"caf\xe9 ok", re.L))
print(re.search(r"(?<=-)\w+", "spam-egg"), re.search(r"(?<!-)\b\w+", "-spam egg"),
      re.search(r"\w+(?=!)", "hi! yo"), re.search(r"\w+(?!\w|!)", "hi! yo"))
print(re.match(r"(a)?(?(1)b|c)", "ab"), re.match(r"(a)?(?(1)b|c)", "c"), re.match(r"(?>a+)ab", "aaab"))
print(re.match(r"a*+a", "aaa"), re.match(r"(?:ab)++c", "ababc"), re.match(r"<.*?>", "<a><b>"))
print(re.match(r"(\w)\1", "aa"), re.match(r"(?P<q>['\"]).*?(?P=q)", "'x'y"), re.match(r"(?i)(a)\1", "aA"))
print(re.fullmatch(r"\p{Lu}+\p{digit}\P{L}", "ABC5!"), re.findall(r"[\p{Nd}\s]+", "a 12 b"))
print(re.match(rb"[\x80-\xff]+", b"\x81\xfe"), re.match(b"a", bytearray(b"ab")), re.search(b"b", memoryview(b"ab")))
print(re.match(r"é+", "ééx").end(), re.search("[^\x00-\x7f]+", "abc déf"), re.sub("é", "e", "éte"))

s = re.Scanner([(r"\d+", lambda sc, t: int(t)), (r"[a-z]+", lambda sc, t: t.upper()), (r"\s+", None)])
print(s.scan("12 ab 7 !"))
sc = re.compile(r"\d").scanner("a1b2")
print(sc.search(), sc.search(), sc.search(), sc.match(), sc.pattern)

print(re.Pattern, re.Match, re.Pattern[str], re.Match[bytes], type(re.finditer("", "")).__name__)
print(p == re.compile(p.pattern), hash(p) == hash(re.compile(p.pattern)), p != re.compile("x"))
print(copy.copy(p) is p, copy.deepcopy(m) is m, re.compile(p) is p, re.compile("a", re.I | re.M | re.X))
print(re.compile(b"x", re.L), re.compile("x" * 300)[:0] if False else repr(re.compile("y" * 300))[-30:])
print(re.NOFLAG, re.I, re.IGNORECASE | re.M, ~re.I & re.S, re.RegexFlag(10))

for bad in ["(", "[a", "a**", "(?P<1>x)", r"\1", "(?<=a+)b", "x{2,1}", "(?Z)", "a\n(b"]:
    try:
        re.compile(bad)
    except re.error as e:
        print(repr(bad), "|", e, "|", e.msg, e.pos, e.lineno, e.colno)
for call in [lambda: re.match("a", b"a"), lambda: re.match(b"a", "a"), lambda: re.search("x", 5),
             lambda: re.compile(re.compile("a"), re.I), lambda: m.group(5), lambda: m.group("nope"),
             lambda: re.sub("(a)", r"\2", "a"), lambda: re.sub("a", r"\g<x", "a"), lambda: p.match(),
             lambda: re.sub("a", 5, "a"), lambda: re.sub("a", lambda mm: 5, "a")]:
    try:
        print(call())
    except (TypeError, ValueError, IndexError, re.error) as e:
        print(type(e).__name__, e)
