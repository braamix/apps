# contextvars, as CPython's own module re-exports the native one.
import contextvars

v = contextvars.ContextVar("v", default=0)


def work(n):
    v.set(n)
    return v.get()


ctx = contextvars.copy_context()
print(ctx.run(work, 5), v.get(), ctx[v], contextvars.Context is type(ctx), contextvars.Token)
