#!/usr/bin/env python3
"""K&R -> C++20 for the ex sources.

The converter this port was made with. It ran once, over tmp/ex/, and the .cpp
files beside it are the source from here on; what it did not do -- the edits
that needed a decision -- was done by hand afterwards. It is kept as the record
of which half of the port was mechanical, and running it again would overwrite
every hand edit since.

Upstream has no prototypes at all: every definition is `name(a, b)` followed by
declarations for a and b, with the return type implicit int. The prototypes in
ex.h and ex_vis.h are the authority, so this reads them and rewrites each
definition's header to match. A definition whose name is not declared is
reported rather than guessed at.

Then the smaller passes:

  register       gone, it is an error in C++17
  bool           -> exbool, since bool is a keyword and inopen holds -1
  delete/inline  renamed, likewise
  CTRL(c)        -> CTRL('c'), which a pre-ANSI preprocessor did not need
  #ifdef         resolved: one arm of each is reachable, and the dead ones
                 do not balance their braces against the live one
  error(...)     -> THROW(...), the return shape picked from the return type
  co_await       inserted at every call of something that is itself a Task

The error pass is the interesting one. Upstream's error() never returned -- it
longjmp'd -- so what a converted call site returns is never used: the caller's
CHK sees ex_thrown first. Zero is therefore always safe.

Not a general converter -- it knows only the shapes this tree uses.
"""

import re
import sys
import os

HERE = os.path.dirname(os.path.abspath(__file__))
VI = os.path.dirname(HERE)

PROTO = re.compile(
    r'^(?P<ret>(?:Task<[^;]*?>|[A-Za-z_][A-Za-z0-9_]*)\s*\**)\s*'
    r'(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\((?P<args>[^;]*)\)\s*;', re.M)

# Some definitions put the return type on a line of its own above the name,
# which is where `line *` and `char *` ones always are.
KNR = re.compile(
    r'^(?:(?:struct\s+)?[A-Za-z_][A-Za-z0-9_]*\s*\*+\s*\n)?'
    r'(?P<name>[A-Za-z_][A-Za-z0-9_]*)\((?P<args>[^)]*)\)\s*\n'
    # A parameter declaration, indented or not: cmdmac's are at column 0.
    r'(?P<decls>(?:[ \t]*[A-Za-z_][^\n{;]*;[ \t]*(?:/\*[^\n]*)?\n)*)'
    r'\{', re.M)

# Renames forced by C++ keywords. `delete` is a real called function
# (ex_cmdsub.c:101); `inline` is both a parameter of address() and a static
# buffer in ex_get.c.
RENAME = {
    'delete': 'exdelete',
    'inline': 'incurs',
    'beep': 'obeep',       # ex_vis.h had `#define beep obeep`
}

# The routines that record an error and, upstream, never came back.
#
# merror, smerror and merror1 are deliberately NOT here: they only print the
# message, and error() and serror() call them on the way to error_end(). Taking
# them for throwers turns `error()` into "print and carry on", which is a subtle
# and thoroughly confusing way for an editor to behave.
THROWERS = ('error', 'serror', 'cerror', 'filioerr', 'syserror')


def load_protos(paths):
    protos = {}
    for p in paths:
        text = open(p).read()
        text = re.sub(r',\s*\n\s+', ', ', text)
        for m in PROTO.finditer(text):
            protos[m.group('name')] = (m.group('ret').strip(),
                                       m.group('args').strip())
    return protos


def zero_for(ret):
    """What a function of this return type should hand back on a throw."""
    if ret == 'void':
        return None
    if ret.endswith('*'):
        return '0'
    return '0'


def throw_pass(src, spans):
    """Rewrite each error() call that stands as a whole statement.

    `spans` maps a character offset range to the enclosing return type, so the
    right one of the four macros is chosen.
    """
    def ret_at(pos):
        for start, end, ret, is_task in spans:
            if start <= pos < end:
                return ret, is_task
        return None, False

    out = []
    i = 0
    call = re.compile(r'(?P<indent>[ \t]*)(?P<name>%s)\s*\('
                      % '|'.join(THROWERS))
    while True:
        m = call.search(src, i)
        if not m:
            out.append(src[i:])
            break
        # Only a call standing as its own statement: the character before the
        # indent must end a statement or open a block.
        j = m.start('indent') - 1
        while j >= 0 and src[j] in ' \t':
            j -= 1
        if j < 0 or src[j] not in ';{}\n:':
            out.append(src[i:m.end()])
            i = m.end()
            continue

        # Find the matching close paren and the semicolon after it.
        depth = 0
        k = m.end() - 1
        while k < len(src):
            if src[k] == '(':
                depth += 1
            elif src[k] == ')':
                depth -= 1
                if depth == 0:
                    break
            elif src[k] in '"\'':
                q = src[k]
                k += 1
                while k < len(src) and src[k] != q:
                    if src[k] == '\\':
                        k += 1
                    k += 1
            k += 1
        if k >= len(src) or src[k + 1:k + 2] != ';':
            out.append(src[i:m.end()])
            i = m.end()
            continue

        ret, is_task = ret_at(m.start())
        if ret is None:
            out.append(src[i:m.end()])
            i = m.end()
            continue

        body = src[m.start('name'):k + 1]
        z = zero_for(ret)
        if is_task:
            macro = 'COTHROW(%s)' % body if z is None \
                else 'COTHROWV(%s, %s)' % (z, body)
        else:
            macro = 'THROW(%s)' % body if z is None \
                else 'THROWV(%s, %s)' % (z, body)
        out.append(src[i:m.start('indent')])
        out.append(m.group('indent') + macro + ';')
        i = k + 2
    return ''.join(out)


def blank_comments(src):
    """Same text, same length, comment and literal bodies blanked.

    Scanning braces over the raw source does not work: an apostrophe in a
    comment ("Don\'t want to set...", ex_addr.c) reads as a character literal
    and swallows the rest of the function.
    """
    out = list(src)
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == '/' and i + 1 < n and src[i + 1] == '*':
            j = src.find('*/', i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != '\n':
                    out[k] = ' '
            i = j
        elif c in '"\'':
            j = i + 1
            while j < n and src[j] != c:
                if src[j] == '\\':
                    j += 1
                j += 1
            j = min(j + 1, n)
            for k in range(i + 1, j - 1):
                if out[k] != '\n':
                    out[k] = ' '
            i = j
        else:
            i += 1
    return ''.join(out)


def function_spans(src, protos):
    """(start, end, rettype, is_task) for each converted definition."""
    scan = blank_comments(src)
    spans = []
    for m in re.finditer(r'^(?P<ret>[^\n]*?)\b(?P<name>[A-Za-z_][A-Za-z0-9_]*)'
                         r'\([^\n]*\)\s*\n\{', src, re.M):
        name = m.group('name')
        if name not in protos:
            continue
        ret = protos[name][0]
        # Body extent: from the brace to its match.
        depth = 0
        k = scan.index('{', m.start())
        start = k
        while k < len(scan):
            if scan[k] == '{':
                depth += 1
            elif scan[k] == '}':
                depth -= 1
                if depth == 0:
                    break
            k += 1
        is_task = ret.startswith('Task<')
        inner = ret[len('Task<'):-1].strip() if is_task else ret
        spans.append((start, k, inner, is_task))
    return spans


# The configuration, resolved rather than carried. Upstream chose between a
# PDP-11 and a VAX, between four tty drivers and between half a dozen optional
# features with #ifdef; exactly one arm of each is reachable here, and the dead
# ones have to go before anything else looks at the source -- their braces do
# not balance against the live arm, so a brace scan walks straight past the end
# of a function (tagfind is where it shows).
#
# VMUNIX is off because its arms reach for stdio, which there is none of; the
# limits it chose are in ex_tune.h outright. CBREAK is on because the keyboard
# hands over one key at a time. The rest are features that went.
DEFINED = {
    'CBREAK': True,
    'CHDIR': True,
    'VMUNIX': False,
    'CRYPT': False,
    'LISPCODE': False,
    'LISP': False,
    'TRACE': False,
    'V6': False,
    'USG3TTY': False,
    'SIGTSTP': False,
    'TIOCLGET': False,
    'TIOCSETC': False,
    'VFORK': False,
    'UCVISUAL': False,
    'BEEHIVE': False,   # a terminal with no escape key; f1 stood in for one
    'ADEBUG': False,
    'EATQS': False,
    'RDEBUG': False,
    'MDEBUG': False,
    'lint': False,
    'vax': False,
    'pdp11': False,
}


def ifdef_pass(src):
    out = []
    # stack of (known, keeping) for each conditional we are inside
    stack = []
    for line in src.split('\n'):
        # `# ifdef` with the hash indented is as common as `#ifdef` here.
        t = re.sub(r'^#\s*', '#', line.strip())
        if t.startswith('#ifdef ') or t.startswith('#ifndef '):
            neg = t.startswith('#ifndef ')
            name = t.split()[1] if len(t.split()) > 1 else ''
            if name in DEFINED:
                on = DEFINED[name] != neg
                stack.append((True, on))
                continue
            stack.append((False, True))
        elif t.startswith('#if '):
            stack.append((False, True))
        elif t.startswith('#else') and stack:
            known, on = stack[-1]
            stack[-1] = (known, not on if known else True)
            if known:
                continue
        elif t.startswith('#endif') and stack:
            known, _ = stack.pop()
            if known:
                continue
        if all(on for known, on in stack if known):
            out.append(line)
    return '\n'.join(out)


# The two headers that were replaced outright.
INCLUDES = {
    'ex_tty.h': 'ex_screen.h',
    'ex_temp.h': 'ex_buf.h',
}


def include_pass(src):
    for old, new in INCLUDES.items():
        src = src.replace('#include "%s"' % old, '#include "%s"' % new)
    return src


def coro_pass(src, spans, protos):
    """Inside a Task<> body: return -> co_return, and co_await every call of
    something that is itself a Task<>."""
    tasks = sorted((n for n, (r, a) in protos.items() if r.startswith('Task<')),
                   key=len, reverse=True)
    call = re.compile(r'(?<![\w.>])(?P<name>%s)\s*\(' % '|'.join(tasks))
    scan = blank_comments(src)

    pieces = []
    at = 0
    for start, end, ret, is_task in spans:
        pieces.append(src[at:start])
        body = src[start:end + 1]
        bscan = scan[start:end + 1]
        if is_task:
            out, i = [], 0
            while True:
                m = call.search(bscan, i)
                if not m:
                    out.append(body[i:])
                    break
                out.append(body[i:m.start('name')])
                # Already awaited?
                pre = body[max(0, m.start('name') - 9):m.start('name')]
                out.append('' if pre.endswith('co_await ') else 'co_await ')
                out.append(body[m.start('name'):m.end()])
                i = m.end()
            # After the splice, not before: co_return would shift every
            # offset bscan was measured at. Driven off the blanked copy, so
            # that "[Hit return to continue]" stays a message.
            body = ''.join(out)
            bs2 = blank_comments(body)
            pieces2, last = [], 0
            for r in re.finditer(r'\breturn\b', bs2):
                pieces2.append(body[last:r.start()])
                pieces2.append('co_return')
                last = r.end()
            pieces2.append(body[last:])
            body = ''.join(pieces2)
        pieces.append(body)
        at = end + 1
    pieces.append(src[at:])
    return ''.join(pieces)


def convert(path, protos, missing):
    src = open(path).read()

    def repl(m):
        name = m.group('name')
        if name not in protos:
            missing.add(name)
            return m.group(0)
        ret, args = protos[name]
        # A default argument belongs to the declaration alone.
        args = re.sub(r'\s*=\s*[^,)]+', '', args)
        sep = '' if ret.endswith('*') else ' '
        return f'{ret}{sep}{name}({args or "void"})\n{{'

    # The SCCS id was read by what(1); nothing reads it here, and it is an
    # unused static in every file.
    src = re.sub(r'^static\s+char\s+\*sccsid\s*=[^;]*;\n', '', src, flags=re.M)

    # CTRL(v) meant ('v' & 037) only because a pre-ANSI preprocessor
    # substituted inside a character constant. Quote the argument, or every
    # one of the forty-odd of them becomes ('c' & 037), which is 3.
    src = re.sub(r"\bCTRL\(([^()'\s])\)", r"CTRL('\1')", src)

    src = ifdef_pass(src)

    for old, new in RENAME.items():
        src = re.sub(r'\b%s\b' % old, new, src)

    # A return type on a line of its own, above a K&R definition.
    src = re.sub(r'^(int|short|char|line|exbool)\s*\n(?=[A-Za-z_]\w*\()',
                 '', src, flags=re.M)

    src, n = KNR.subn(repl, src)

    # `register x;` is implicit int; stripping register alone leaves a bare
    # name and a syntax error.
    src = re.sub(r'^(\s*)register\s+([A-Za-z_]\w*)\s*;', r'\1int \2;',
                 src, flags=re.M)
    src = re.sub(r'\bregister\s+', '', src)
    src = re.sub(r'\bbool\b', 'exbool', src)

    src = throw_pass(src, function_spans(src, protos))
    src = coro_pass(src, function_spans(src, protos), protos)
    src = include_pass(src)
    return src, n


def main():
    protos = load_protos([os.path.join(VI, f) for f in
                          ('ex.h', 'ex_vis.h', 'ex_screen.h', 'ex_buf.h')])
    missing = set()
    for path in sys.argv[1:]:
        out, n = convert(path, protos, missing)
        sys.stdout.write(out)
        print(f'/* {os.path.basename(path)}: {n} definitions */',
              file=sys.stderr)
    if missing:
        print('no prototype for: ' + ' '.join(sorted(missing)),
              file=sys.stderr)


if __name__ == '__main__':
    main()
