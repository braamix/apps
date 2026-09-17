# _string: str.format's grammar as string.Formatter reads it, errors included.
import _string as s
print(sorted(n for n in dir(s) if not n.startswith('__')))
for t in ["", "abc", "a{0}b", "{}", "{{", "}}", "a{{b}}c", "{0!r:>{1}}", "{a.b[c]}", "{!s}",
          "{:}", "{0:}", "x{0!}", "{0!rr}", "}", "{", "a{0", "{0:{1:{2}}}", "{0[}", "{0]}",
          "{ 0 }", "{0!r", "{:{}", "{a!=}", "{a[!]}x", "é{0!é}ü", "{a{b}", "{0!", "x}y"]:
    it = s.formatter_parser(t)
    got = []
    try:
        for x in it:
            got.append(x)
        print(repr(t), type(it).__name__, got)
    except Exception as e:
        print(repr(t), got, type(e).__name__, e)
for t in ["", "a", "0", "a.b[c]", "0[1].x", "a[", "a.", "a..b", "a[0]x", "[0]", ".a", "a[x]]",
          "00", "a[-1]", "a[01]", "1a", "a[]", "٣", "a[٤٥]", "9" * 30, "a[" + "9" * 30 + "]"]:
    try:
        h, r = s.formatter_field_name_split(t)
        got = []
        try:
            for x in r:
                got.append(x)
            print(repr(t), repr(h), type(r).__name__, got)
        except Exception as e:
            print(repr(t), repr(h), got, type(e).__name__, e)
    except Exception as e:
        print(repr(t), type(e).__name__, e)
for f in (s.formatter_parser, s.formatter_field_name_split):
    try:
        f(1)
    except TypeError as e:
        print(e)
it = iter(s.formatter_parser("a"))
print(iter(it) is it)
