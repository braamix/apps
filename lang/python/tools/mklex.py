#!/usr/bin/env python3
"""Write the expected token dump of a source file, using CPython's tokenizer.

    tools/mklex.py [--regen] test/lex/numbers.py [more...]

The golden is what `python --dump-tokens` must print, so the lexer is measured
against CPython's own tokenize module rather than against itself. Token values
come from ast.literal_eval, so the escapes are compared decoded.

It runs under $PYTHON when that is set; see tools/pyref.py.
"""

import ast
import io
import keyword
import os
import sys

import pyref
import token
import tokenize

SKIP = {token.COMMENT, token.NL, token.ENCODING}
LABEL = {
    token.NEWLINE: "newline",
    token.INDENT: "indent",
    token.DEDENT: "dedent",
    token.ENDMARKER: "endmarker",
}


def repr_of(v):
    """repr(), which is what --dump-tokens prints for a literal."""
    return repr(v)


def line_of(tok):
    kind, text, (row, col), _, _ = tok
    at = f"{row}:{col + 1}"

    if kind in LABEL:
        return f"{at} {LABEL[kind]}"
    if kind == token.NAME:
        return f"{at} kw {text}" if keyword.iskeyword(text) else f"{at} name {text}"
    if kind == token.OP:
        return f"{at} op {text}"
    if kind == token.NUMBER:
        v = ast.literal_eval(text)
        if isinstance(v, complex):
            # The lexer keeps the magnitude; the `j` is the token kind.
            return f"{at} imag {repr_of(v.imag)}j"
        return f"{at} float {repr_of(v)}" if isinstance(v, float) else f"{at} int {v}"
    if kind == token.STRING:
        prefix = text[: len(text) - len(text.lstrip("rbufRBUF"))]
        if "f" in prefix.lower():
            # The body as written: an f-string is parsed later, not now.
            quote = text[len(prefix)]
            n = 3 if text[len(prefix):].startswith(quote * 3) else 1
            body = text[len(prefix) + n: -n]
            return f"{at} fstring {repr_of(body)}"
        v = ast.literal_eval(text)
        return f"{at} {'bytes' if isinstance(v, bytes) else 'str'} {repr_of(v)}"
    raise SystemExit(f"mklex: token {token.tok_name[kind]} is not handled")


def fstring_of(lines, start, end):
    """One f-string as a single token, the way 3.11 and before tokenized it:
    3.12 splits it into pieces, and this lexer keeps the body whole."""
    (r0, c0), (r1, c1) = start, end
    if r0 == r1:
        text = lines[r0 - 1][c0:c1]
    else:
        text = lines[r0 - 1][c0:] + "".join(lines[r0:r1 - 1]) + lines[r1 - 1][:c1]
    prefix = text[: len(text) - len(text.lstrip("rbuftRBUFT"))]
    quote = text[len(prefix)]
    n = 3 if text[len(prefix):].startswith(quote * 3) else 1
    kind = "tstring" if "t" in prefix.lower() else "fstring"
    return f"{r0}:{c0 + 1} {kind} {repr_of(text[len(prefix) + n: -n])}"


def dump(source):
    out = []
    lines = source.decode("utf-8").splitlines(keepends=True)
    depth, start = 0, None
    for tok in tokenize.tokenize(io.BytesIO(source).readline):
        if tok.type in SKIP:
            continue
        name = token.tok_name[tok.type]
        if name in ("FSTRING_START", "TSTRING_START"):
            if not depth:
                start = tok.start
            depth += 1
            continue
        if name in ("FSTRING_END", "TSTRING_END"):
            depth -= 1
            if not depth:
                out.append(fstring_of(lines, start, tok.end))
            continue
        if depth:
            continue
        out.append(line_of(tok))
    return "".join(s + "\n" for s in out)


def main():
    pyref.reexec()
    paths, regen = pyref.args()
    if not paths:
        raise SystemExit(__doc__)
    for path in paths:
        pyref.check(path, pyref.tag(), regen)
        with open(path, "rb") as f:
            source = f.read()
        text = dump(source)
        with open(path + ".exp", "w", encoding="utf-8") as f:
            f.write(text)
        pyref.record(path, pyref.tag())
        print(f"mklex: {os.path.basename(path)}: {text.count(chr(10))} tokens")


if __name__ == "__main__":
    main()
