# tomllib: every value type, tables, arrays of tables, and the errors.
import tomllib

doc = '''
# comment
title = "TOML \\u00e9 \\"quoted\\""
literal = 'C:\\path'
multi = """
line one \\
  continued"""
raw = \'\'\'
first\'\'\'
int = +99_000
hex = 0xDEAD_beef
oct = 0o755
bin = 0b1101
flt = -3.1415e2
inf = inf
nan_is = nan
yes = true
odt = 1979-05-27T07:32:00-08:00
ldt = 1979-05-27T07:32:00.999
ld = 1979-05-27
lt = 07:32:00
arr = [1, 2, [3, "four"], ]
inline = { x = 1, y.z = "deep" }

[server."alpha beta"]
ip = "10.0.0.1"

[[fruit]]
name = "apple"
[fruit.physical]
color = "red"

[[fruit]]
name = "banana"
'''
d = tomllib.loads(doc)
for k, v in d.items():
    if k == "nan_is":
        print(k, v != v)
    else:
        print(k, repr(v))
print(tomllib.loads("a = 1.5", parse_float=lambda s: "F" + s))
for bad in ["a = ", "a = 1\na = 2", "[t]\n[t]", "x = [1,,2]", "k = 07", 'k = "\\q"']:
    try:
        tomllib.loads(bad)
    except tomllib.TOMLDecodeError as e:
        print("TOMLDecodeError", e, e.lineno, e.colno)
import io
print(tomllib.load(io.BytesIO(b"k = 'v'")))
try:
    tomllib.load(io.StringIO("k = 1"))
except TypeError as e:
    print("TypeError", e)
