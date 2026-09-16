# Source text: PEP 263's cookies, PEP 3120's UTF-8, PEP 3131's identifiers,
# and the escapes a string literal may hold.
#
# The same program on CPython and here, byte for byte.

def attempt(src, mode="exec"):
    try:
        code = compile(src, "<case>", mode)
    except SyntaxError as e:
        print("SyntaxError:", e.msg, e.lineno, e.offset)
        return None
    ns = {}
    exec(code, ns)
    return ns

for src in [b"x = '\xe9'\n", b"# -*- coding: latin-1 -*-\nx = '\xe9'\n",
            b"#!/bin/py\n# vim: set fileencoding=iso-8859-15 :\nx = '\xa4'\n",
            b"\n# coding: latin-1\nx = '\xe9'\n", b"y = 1\n# coding: latin-1\nx = '\xe9'\n",
            b"# coding=cp1252\nx = '\x80\x99'\n", b"# coding: ascii\nx = '\xe9'\n",
            b"# coding: nope\nx = 1\n", b"\xef\xbb\xbfx = '\xc3\xa9'\n",
            b"\xef\xbb\xbf# coding: latin-1\nx = 1\n", b"\xef\xbb\xbf# coding: utf-8\nx = '\xc3\xa9'\n",
            b"# coding: utf-8\nx = '\xed\xa0\x80'\n", b"x = 1\ny = '\xc3'\n",
            b"# coding: UTF_8-unix\nx = '\xc3\xa9'\n", b"# coding: Latin_1-x\nx = '\xe9'\n",
            b"x = '\\u00e9\\N{EM DASH}'\n"]:
    ns = attempt(src)
    if ns is not None:
        print(ascii(ns["x"]))
print(ascii(eval(b"'\xe9'".decode("latin-1"))), ascii(eval("# coding: latin-1\n'\xe9'")))

# Escapes: \N by name, alias and case; a lone surrogate; and what is refused.
for src in [r"'\N{EM DASH}'", r"'\N{em dash}'", r"'\N{NULL}\N{LINE FEED}'", r"'\N{BYTE ORDER MARK}'",
            r"'\N{CJK UNIFIED IDEOGRAPH-4E00}\N{HANGUL SYLLABLE GAG}'",
            r"r'\N{EM DASH}'", r"'\ud800'", r"'\udc80\U0000d800'", r"f'\N{EM DASH}{1}'",
            r"'\N{LATIN SMALL LETTER A}\N{LATIN CAPITAL LETTER A WITH MACRON AND GRAVE}'",
            r"'ab\N{NOPE}'", r"'\N'", r"'\N{'", r"'\N{}'", r"'\N{EM DASH'", r"'\x4'", r"'\u12'",
            r"'\U0011ffff'", r"'\xe9\x4'", r"'\xe9\N{BAD}'", r"b'\x4'", r"b'ab\x'",
            r"'''a\nb\x4'''", r"u'\N{EM DASH}'"]:
    try:
        print(src, ascii(eval(src)))
    except SyntaxError as e:
        print(src, "SyntaxError:", e.msg, e.lineno, e.offset)

# Identifiers: XID_Start and XID_Continue, and the NFKC form is the name.
ns = attempt("\u00e9t\u00e9 = 1\n\ufb01 = 2\n\U0001d518 = 3\n_\u0660 = 4\n\u2118 = 5\n"
             "x\u00b7y = 6\n\u1e9b\u0323 = 7\n")
print(sorted(k for k in ns if not k.startswith("__")))
for src in ["\u20ac = 1", "a\u20ac = 1", "x = a\xa0b", "ab\u200bc = 1", "1\u0660 = 1",
            "x = 1 \u2003+ 2", "\u0660 = 1", "x = \U0001f600", "if x:\n  \u2028 = 1"]:
    attempt(src)
print("\u00e9t\u00e9".isidentifier(), "\ufb01".isidentifier(), "\u2118".isidentifier(),
      "\u0660a".isidentifier(), "a\u0660".isidentifier(), "\U0001d518".isidentifier())
class T:
    ä = 1
    µ = 2
    ﬁne = 3
print(getattr(T, "\xe4"), getattr(T, "\u03bc"), getattr(T, "fine"))
