# _contextvars: ContextVar, Token, Context.run and copy_context.
import _contextvars as c
def r(x):
    return repr(x).split(" at 0x")[0]
print(sorted(n for n in dir(c) if not n.startswith('__')))
v = c.ContextVar('v')
w = c.ContextVar('w', default=5)
print(r(v), v.name, w.get(), v.get(9), r(w))
try:
    v.get()
except LookupError as e:
    print(type(e).__name__, e.args[0] is v)
t = v.set(1)
print(r(t), t.var is v, t.old_value, t.MISSING, c.Token.MISSING, v.get())
print(t.old_value is c.Token.MISSING)
t2 = v.set(2)
print(t2.old_value)
v.reset(t2)
print(v.get())
for f in (lambda: v.reset(t2), lambda: w.reset(t), lambda: v.reset(1),
          lambda: v.set(), lambda: v.get(1, 2), lambda: c.ContextVar('a', 1),
          lambda: c.ContextVar(1), lambda: c.ContextVar(), lambda: c.Context(1),
          lambda: c.Token(), lambda: hash(c.Context())):
    try:
        f()
    except Exception as e:
        print(type(e).__name__, str(e).split(" at 0x")[0])
v.reset(t)
print(v.get(None))
ctx = c.copy_context()
print(type(ctx).__name__, type(ctx).__module__, len(ctx))
v.set(3)
print(ctx.get(v), v in ctx, len(c.copy_context()))
def f(a, b=0):
    v.set(a + b)
    return v.get(), c.copy_context()[v]
print(ctx.run(f, 10, b=1), v.get(), ctx[v], list(ctx) == [v], list(ctx.keys()) == [v],
      list(ctx.values()), [(k is v, x) for k, x in ctx.items()])
try:
    ctx.run(lambda: ctx.run(f, 1))
except RuntimeError as e:
    print(str(e).split(" at ")[0])
def boom():
    v.set(99)
    raise KeyError("x")
try:
    ctx.run(boom)
except KeyError as e:
    print("caught", e, v.get(), ctx[v])
print(ctx.run(v.get))
for k in (w, 'x'):
    try:
        ctx[k]
    except (KeyError, TypeError) as e:
        print(type(e).__name__, str(e).split(" at ")[0])
try:
    'a' in ctx
except TypeError as e:
    print(e)
try:
    ctx['a'] = 1
except TypeError as e:
    print(e)
print(c.ContextVar[int], len(c.Context()))
n = c.Context()
print(n.run(v.get, 7), n.get(v), n.get(v, 8))
print(hash(v) == hash(v))
with v.set(40) as tok:
    print(v.get(), tok.var is v)
print(v.get())
cp = ctx.copy()
print(cp == ctx, cp is ctx, cp[v], cp != ctx, ctx == 1)
for base in (c.ContextVar, c.Context, c.Token):
    try:
        class X(base):
            pass
    except TypeError as e:
        print(e)
print(c.ContextVar.__module__, c.Context.__name__)
match ctx:
    case {}:
        print("mapping")
def gen():
    yield v.get(0)
    yield v.get(0)
g = gen()
print(next(g), c.Context().run(next, g))
print(r(c.ContextVar('a', default=None)))
