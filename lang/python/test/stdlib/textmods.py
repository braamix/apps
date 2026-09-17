# What stands on re: textwrap, string.Template, locale.format_string, json,
# fractions and difflib.
import difflib
import fractions
import json
import locale
import math
import string
import textwrap

text = "The quick brown fox jumps over the lazy dog. " * 3
print(textwrap.wrap(text, 30))
print(textwrap.fill("  indented\ttext with   spaces  ", width=12, expand_tabs=True))
print(repr(textwrap.dedent("    a\n      b\n\n    c\n")), repr(textwrap.indent("x\n\ny\n", "> ")))
print(textwrap.shorten("Hello  world, this is a long line", 20), textwrap.wrap("", 5))
w = textwrap.TextWrapper(width=20, initial_indent="* ", subsequent_indent="  ", break_long_words=False)
print(w.wrap("supercalifragilisticexpialidocious is long, isn't it -- yes"))
print(textwrap.wrap("a-b-c-d-e-f", 4), textwrap.wrap("x " * 5, 3, max_lines=2, placeholder="~"))

t = string.Template("$who likes ${what}; $$5")
print(t.substitute(who="tim", what="kung pao"), t.safe_substitute(who="x"), t.get_identifiers(), t.is_valid())
for bad in (lambda: t.substitute(who="x"), lambda: string.Template("$ x").substitute(),
            lambda: string.Template("${").substitute()):
    try:
        bad()
    except (KeyError, ValueError) as e:
        print(type(e).__name__, e)


class Dotted(string.Template):
    delimiter = "%"
    idpattern = r"[a-z]+\.[a-z]+"


print(Dotted("%user.name is %user.home").safe_substitute({"user.name": "ann"}))
print(locale.format_string("%d items at %.2f, %s", (1234, 5.5, "ok")), locale.format_string("%5.1f%%", 12.34))
print(locale.format_string("%(a)s-%(b)d", {"a": "x", "b": 3}), locale.currency(1.5) if False else "")

print(json.dumps({"b": [1, 2.5, None, True, False], "a": "é\n\"", "c": {}}, sort_keys=True))
print(json.dumps([1, {"k": [2]}], indent=2), json.dumps({"x": 1}, separators=(",", ":")))
print(json.dumps("€\U0001f600"), json.dumps("€", ensure_ascii=False), json.dumps(1e400))
print(json.loads('{"k": [1, 2, {"z": "\\u00e9\\ud83d\\ude00"}], "n": -1.5e3, "t": true, "u": null}'))
print(json.loads("[NaN, Infinity, -Infinity, 1E2, 0.5, 10]"), json.loads('"\\t"'), json.loads(" 7 "))
print(json.loads('{"a": 1, "a": 2}'), json.loads('[1, 2]', parse_int=str), json.loads('{"p": 1.5}', parse_float=repr))
print(json.loads('{"x": 1}', object_hook=lambda d: sorted(d)), json.loads('[[1,2]]', object_pairs_hook=None))
print(json.dumps({1: "a", 2.5: "b", True: "c", None: "d"}), json.dumps((1, 2)), json.dumps([], indent=4))
for doc in ['{"a": }', "[1, 2", '{"a" 1}', "tru", "[1,]", "", "01", '{"a":1}x']:
    try:
        json.loads(doc)
    except json.JSONDecodeError as e:
        print(repr(doc), e.msg, e.pos, e.lineno, e.colno)
try:
    json.dumps({"s": {1, 2}})
except TypeError as e:
    print(e)


class Enc(json.JSONEncoder):
    def default(self, o):
        return sorted(o) if isinstance(o, set) else super().default(o)


print(json.dumps({"s": {3, 1}}, cls=Enc), list(json.JSONDecoder().raw_decode('[1] rest')))
loop = []
loop.append(loop)
try:
    json.dumps(loop)
except ValueError as e:
    print(e)

F = fractions.Fraction
print(F(3, 7) + F(1, 3), F("1.25"), F(" -3/4 "), F(0.1).limit_denominator(100), F(7, -21), F(2.5))
print(F(1, 3) ** 2, F(4, 9) ** F(1, 2), F(-8, 27) ** -1, abs(F(-2, 3)), F(5, 3) // 1, F(5, 3) % 1)
print(math.floor(F(7, 2)), math.ceil(F(7, 2)), math.trunc(F(-7, 2)), round(F(7, 3), 2), round(F(5, 2)), int(F(-7, 2)))
print(f"{F(1, 3):.5f}", format(F(22, 7), "10.3f"), f"{F(1, 8):%}", f"{F(355, 113):e}", str(F(6, 4)), repr(F(6, 4)))
print(F(1, 2) == 0.5, F(1, 3) < 0.34, hash(F(1, 2)) == hash(0.5), F(1, 2).as_integer_ratio(), F.from_float(0.25))
print(F(1, 3).is_integer(), F(4, 2).is_integer(), F.from_decimal if False else "", F(10, 4).numerator, F(10, 4).denominator)
for bad in ("1/0", "x", "1.5/2", "3 /4"):
    try:
        print(F(bad))
    except (ValueError, ZeroDivisionError) as e:
        print(type(e).__name__, e)

a = ["one\n", "two\n", "three\n", "four\n"]
b = ["zero\n", "one\n", "tree\n", "four\n"]
print(list(difflib.ndiff(a, b)))
print(list(difflib.context_diff(a, b, "a", "b")))
print("".join(difflib.restore(difflib.ndiff(a, b), 2)))
sm = difflib.SequenceMatcher(None, "abxcd", "abcd")
print(sm.ratio(), sm.quick_ratio(), sm.get_matching_blocks(), sm.get_opcodes())
print(list(sm.get_grouped_opcodes(1)), sm.find_longest_match(0, 5, 0, 4))
print(difflib.get_close_matches("appel", ["ape", "apple", "peach", "puppy"]), difflib.IS_CHARACTER_JUNK(" "))
print(difflib.HtmlDiff(wrapcolumn=10).make_table(["abc"], ["abd"]).count("<tr>"))
