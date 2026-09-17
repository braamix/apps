# _thread for one thread: the two locks, _local, and the names threading
# reads.
import _thread as t
print(t.LockType, t.lock is t.LockType, t.error is RuntimeError, t.TIMEOUT_MAX)
l = t.allocate_lock()
print(type(l) is t.LockType, type(l).__name__, type(l).__module__)
print(repr(l).split(" at ")[0], l.acquire(), repr(l).split(" at ")[0], l.locked())
print(l.acquire(False), l.acquire(timeout=0), l.acquire(True, 0.01), l.acquire(blocking=0))
l.release()
for f in (l.release, lambda: l.acquire(False, 1), lambda: l.acquire(timeout=-2),
          lambda: l.acquire(timeout=1e100), lambda: l.acquire(timeout="x"),
          lambda: l.release(1), lambda: t.allocate_lock(1), lambda: t._local(1),
          lambda: t._local(a=1), lambda: t.stack_size(10)):
    try:
        f()
    except Exception as e:
        print(type(e).__name__, e)
with l:
    print(l.locked())
print(l.locked(), l.acquire_lock(), l.locked_lock(), l.release_lock())
print(type(t.get_ident()), type(t.get_native_id()), t.get_ident() == t.get_ident())
r = t.RLock()
print(repr(r).split(" at ")[0])
r.acquire(); r.acquire()
print(repr(r).split(" owner=")[0], r._is_owned(), r._recursion_count(), r.locked())
st = r._release_save()
print(r._recursion_count(), st[0])
r._acquire_restore(st)
print(r._recursion_count())
r.release(); r.release()
try:
    r.release()
except RuntimeError as e:
    print(e)
with r:
    with r:
        print(r._recursion_count())
print(r._is_owned())
x = t._local()
x.a = 1
print(x.a, x.__dict__)
try:
    x.b
except AttributeError as e:
    print(e)
del x.a
try:
    del x.a
except AttributeError as e:
    print(e)
class M(t._local):
    def __init__(s, v):
        s.v = v
m = M(3)
print(m.v, m.__dict__, isinstance(m, t._local))
class RR(t.RLock):
    pass
q = RR()
with q:
    print(q._is_owned())
print(t._count(), t.stack_size(), t.stack_size(0))
print(type(t._local).__name__, t._local.__name__, t._local.__module__, t.RLock.__name__)
