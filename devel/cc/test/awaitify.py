#!/usr/bin/env python3
"""A C program's output, awaited: on Braam printf is a coroutine.

    awaitify.py [--dry] file...

Each function that reaches an output call becomes coro(braam_call *), and its
calls and the output calls get an await.  What it cannot do is reported: output
in a deferred statement, in a coroutine of another yield type, a printing
function used as a value, putch and putbyte.  --dry reports without writing.
"""
import re, sys
OUT = {'printf','fprintf','vfprintf','vprintf','puts','putchar','fputs','fputc','putc','fwrite','perror'}
KW = {'if','while','for','switch','return','sizeof','defer','await','yield','_Alignof','_Static_assert',
      'co_alloca','co_init','co_resume','co_value','co_result','co_cancel','co_destroy','_Generic','coro','co_frame','coro_ptr'}
PRE = 'coro(braam_call *) '
def blank(s):
    out = list(s); i = 0; n = len(s)
    while i < n:
        c = s[i]
        if s.startswith('//', i):
            j = s.find('\n', i); j = n if j < 0 else j
            for k in range(i, j): out[k] = ' '
            i = j
        elif s.startswith('/*', i):
            j = s.find('*/', i + 2); j = n if j < 0 else j + 2
            for k in range(i, j):
                if s[k] != '\n': out[k] = ' '
            i = j
        elif c in '"\'':
            j = i + 1
            while j < n and s[j] != c:
                j += 2 if s[j] == '\\' else 1
            for k in range(i + 1, min(j, n)): out[k] = ' '
            i = j + 1
        elif c == '#' and (i == 0 or s[i-1] == '\n' or s[:i].rsplit('\n', 1)[-1].strip() == ''):
            j = i
            while True:
                e = s.find('\n', j); e = n if e < 0 else e
                if e > 0 and s[e-1] == '\\': j = e + 1; continue
                break
            for k in range(i, e): out[k] = ' '
            i = e
        else:
            i += 1
    return ''.join(out)
def match_paren(b, i, o, c):
    d = 0
    for k in range(i, len(b)):
        if b[k] == o: d += 1
        elif b[k] == c:
            d -= 1
            if d == 0: return k
    return -1
def parse(s):
    b = blank(s)
    funcs = {}   # name -> (declstart, namepos, bodystart, bodyend, yieldtype)
    protos = []  # (declstart, name)
    depth = 0; i = 0; n = len(b); stmt_start = 0
    while i < n:
        c = b[i]
        if c == '{':
            if depth == 0:
                # a function body? look back for ') ' and a name before '('
                k = i - 1
                while k >= 0 and b[k].isspace(): k -= 1
                if k >= 0 and b[k] == ')':
                    # find matching '('
                    d = 0; j = k
                    while j >= 0:
                        if b[j] == ')': d += 1
                        elif b[j] == '(':
                            d -= 1
                            if d == 0: break
                        j -= 1
                    m = re.search(r'([A-Za-z_]\w*)\s*$', b[:j])
                    head = b[stmt_start:j]
                    if m and '=' not in head and m.group(1) not in KW:
                        e = match_paren(b, i, '{', '}')
                        y = re.search(r'coro\s*\(([^)]*\)?)\)', b[stmt_start:j])
                        funcs[m.group(1)] = (stmt_start, m.start(1), i, e, y.group(1).strip() if y else None)
                        i = e + 1; stmt_start = i
                        continue
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0: stmt_start = i + 1
        elif c == ';' and depth == 0:
            seg = b[stmt_start:i]
            m = re.search(r'([A-Za-z_]\w*)\s*\([^()]*(\([^()]*\)[^()]*)*\)\s*$', seg)
            if m and '=' not in seg[:m.start()] and not seg.strip().startswith('typedef'):
                protos.append((stmt_start, m.group(1)))
            stmt_start = i + 1
        i += 1
    return b, funcs, protos
def calls_in(b, s0, e0):
    for m in re.finditer(r'(?<![\w.>])([A-Za-z_]\w*)\s*\(', b[s0:e0]):
        if m.group(1) not in KW:
            yield s0 + m.start(1), m.group(1)
def run(path, dry):
    s = open(path).read()
    b, funcs, protos = parse(s)
    P = set()
    for f, (ds, ns, bs, be, y) in funcs.items():
        if any(c in OUT for _, c in calls_in(b, bs, be)): P.add(f)
    ch = True
    while ch:
        ch = False
        for f, (ds, ns, bs, be, y) in funcs.items():
            if f not in P and any(c in P for _, c in calls_in(b, bs, be)):
                P.add(f); ch = True
    report = []
    edits = []  # (pos, text)
    for f in P:
        ds, ns, bs, be, y = funcs[f]
        if y is not None and y.replace(' ', '') != 'braam_call*':
            report.append(f'{path}: {f} is coro({y}) and prints')
        if y is None:
            k = ds
            while k < ns and b[k].isspace(): k += 1
            m = re.match(r'(static\s+)', b[k:ns])
            if m: k += m.end()
            edits.append((k, PRE))
        # defer with output
        body = b[bs:be]
        for m in re.finditer(r'\bdefer\b[^;]*\b(\w+)\s*\(', body):
            if m.group(1) in OUT or m.group(1) in P:
                report.append(f'{path}: {f}: defer of output')
        for pos, c in calls_in(b, bs, be):
            if c in OUT or c in P:
                if not b[:pos].rstrip().endswith('await'):
                    edits.append((pos, 'await '))
    for ds, name in protos:
        if name in P and funcs.get(name, (None,)*5)[4] is None:
            k = ds
            while b[k].isspace(): k += 1
            if not s[k:].startswith('coro'):
                m = re.match(r'((?:static|extern)\s+)', b[k:])
                if m: k += m.end()
                edits.append((k, PRE))
    # names of P used other than called
    for f, (ds, ns, bs, be, y) in funcs.items():
        for m in re.finditer(r'(?<![\w.>])([A-Za-z_]\w*)\b(?!\s*\()', b[bs:be]):
            if m.group(1) in P:
                report.append(f'{path}: {m.group(1)} used as a value in {f}')
        if f not in P:
            for pos, c in calls_in(b, bs, be):
                if c in OUT: report.append(f'{path}: output in plain {f}')
    # output at file scope outside any function (e.g. in a macro): report
    for m in re.finditer(r'#\s*define[^\n]*\b(printf|puts|putchar|fputs|fprintf)\s*\(', s):
        report.append(f'{path}: macro with output')
    for m in re.finditer(r'\b(putch|putbyte)\b', b):
        report.append(f'{path}: uses {m.group(1)}'); break
    for pos, t in sorted(edits, reverse=True):
        s = s[:pos] + t + s[pos:]
    if not dry:
        open(path, 'w').write(s)
    return P, report
dry = sys.argv[1] == '--dry'
for p in sys.argv[2 if dry else 1:]:
    P, rep = run(p, dry)
    for r in rep: print(r)
