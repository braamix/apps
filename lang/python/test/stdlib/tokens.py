# tokenize over the native _tokenize: every token kind, the errors, and bytes with a cookie.
import tokenize, io, token
srcs = [
    "x = 1\n",
    "def f(a, b=2):\n    '''doc\n    string'''\n    return a  # c\n\n\n",
    "if x:\n\tpass\n  # comment\nelse:\n    y = [1,\n  2]\n",
    "s = f'a{b!r:>{w}} {c=} {{x}} {d:%H}' + rf'\\d{e}' + t'{x}'\n",
    "x = 0x_1f + 0o17 + 0b1_0 + 1_000.5e-3j + .5 + 5. + 1e10\n",
    "a = b'x' Rb'y' u'z' \"\"\"q\nw\"\"\"\n",
    "x = 1 \\\n  + 2\n",
    "lambda: (yield)\n@dec\nclass A(B, metaclass=M): ...\n",
    "x = 'unterminated\n",
    "def f():\n  return 1\n return 2\n",
    "x = (1,\n",
    "x = 1\r\ny = 2\r\n",
    "no newline at end",
    "é = 'ü' + \"日本\"  # ñ\n",
    "f'{x:{y:{z}}}'\n",
    "x @= y ** 2 // 3 >>= 4 != 5 -> 6 := 7\n",
    "  \n\n# only comment",
    "if True:\n    x = f'''\n{\n1\n}\n'''\n",
    "$x\n",
    "print(1)\n\x0c\nprint(2)\n",
    "1if x else 2\n",
    "x = 1_\n",
]
for s in srcs:
    print("----", repr(s))
    try:
        for t in tokenize.generate_tokens(io.StringIO(s).readline):
            print(token.tok_name[t.type], repr(t.string), t.start, t.end, repr(t.line))
    except Exception as e:
        print("ERR", type(e).__name__, e)
for s in [b"# -*- coding: latin-1 -*-\nx = '\xe9'\n", b"\xef\xbb\xbfx = 1\n", b"x = '\xc3\xa9'\ny = 2", b"x = 1\n\xff\n"]:
    print("----", repr(s))
    try:
        for t in tokenize.tokenize(io.BytesIO(s).readline):
            print(token.tok_name[t.type], repr(t.string), t.start, t.end, repr(t.line))
    except Exception as e:
        print("ERR", type(e).__name__, e)
print(tokenize.untokenize(tokenize.generate_tokens(io.StringIO("x=( 1 ,2)\n").readline)))
import _tokenize
print(list(_tokenize.TokenizerIter(iter(["a = [1 ,\n", "2]\n"]).__next__, extra_tokens=False)))
def bad():
    yield "x = 1\n"
    raise ValueError("boom")
try:
    for t in tokenize.generate_tokens(bad().__next__):
        print(t)
except ValueError as e:
    print("raised", e)
