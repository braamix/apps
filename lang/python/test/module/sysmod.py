# sys, time, errno, gc, _random and _types. Only what is the same on every
# implementation is printed: a size, a clock reading and a collection count
# are this interpreter's own.
import sys
import time
import errno
import gc
from _random import Random

print(sys.__name__, type(sys.path), type(sys.argv), type(sys.modules))
print(sys.byteorder in ("little", "big"), sys.maxunicode, isinstance(sys.maxsize, int))
print(sys.version_info[:2], sys.version_info >= (3, 0), sys.version_info < (9, 0))
print(sys.version_info.major, sys.version_info.minor, sys.version_info.releaselevel)
print(len(sys.version_info), sys.version_info[3], sys.version_info[:2] == (3, 9))
print(sys.float_info.mant_dig, sys.float_info.dig, sys.float_info.radix)
print(sys.float_info.max, sys.float_info.min, sys.float_info.epsilon)
print(sys.float_info.max_exp, sys.float_info.min_exp, len(sys.float_info) >= 11)
print(sys.flags.optimize, sys.flags.debug, type(sys.implementation.name))
print(isinstance(sys.getrecursionlimit(), int), sys.getsizeof([]) > 0)
print(sys.intern("abc") == "abc", sys.getdefaultencoding())
print(sys.exc_info(), "builtins" in sys.builtin_module_names)
try:
    raise ValueError("x")
except ValueError:
    print(sys.exc_info()[0].__name__, sys.exc_info()[1].args)
print(sys.exc_info()[0])

n = sys.getrecursionlimit()
sys.setrecursionlimit(200)
print(sys.getrecursionlimit())
sys.setrecursionlimit(n)

print(sys.stdout.writable(), sys.stdout.readable(), sys.stdin.readable())
print(sys.stdout.name, sys.stderr.name, sys.stdin.name, sys.stdout.mode)
print(sys.stdout.encoding, sys.stdout.closed, sys.stdout.fileno())
print(sys.stdout is sys.__stdout__, sys.stderr is sys.__stderr__)
sys.stdout.write("written\n")
sys.stdout.writelines(["a", "b", "\n"])
sys.stdout.flush()


class Sink:
    def __init__(self):
        self.seen = []

    def write(self, text):
        self.seen.append(text)
        return len(text)

    def flush(self):
        pass


# print writes its line in one call here and a piece at a time in CPython, so
# what is compared is the text rather than the calls.
held = sys.stdout
sys.stdout = Sink()
print("captured", 1, 2)
print("more", end="!")
kept = sys.stdout.seen
sys.stdout = held
print(repr("".join(kept)))

into = Sink()
print("to a file", 5, file=into, sep="-")
print("and again", file=into, end="")
print(repr("".join(into.seen)))
print("flushing", flush=True)

print(type(time.time()), type(time.monotonic()), isinstance(time.time_ns(), int))
print(time.gmtime(0).tm_year, time.gmtime(0).tm_mon, time.gmtime(0).tm_mday)
print(time.gmtime(0).tm_hour, time.gmtime(0).tm_wday, time.gmtime(0).tm_yday)
print(tuple(time.gmtime(0)), len(time.gmtime(0)))
print(time.gmtime(1234567890).tm_year, time.gmtime(1234567890).tm_mon)
print(time.gmtime(1234567890).tm_mday, time.gmtime(1234567890).tm_hour)
print(time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(0)))
print(time.strftime("%a %b %j %p %I %%", time.gmtime(1234567890)))
print(time.asctime(time.gmtime(0)), time.ctime(0) == time.asctime(time.localtime(0)))
print(time.gmtime(-86400).tm_year, time.gmtime(-86400).tm_mon, time.gmtime(-86400).tm_mday)
print(time.gmtime(951782400).tm_year, time.gmtime(951782400).tm_yday)

print(errno.ENOENT, errno.EINVAL, errno.EPERM, errno.EIO)
print(errno.errorcode[errno.ENOENT], errno.errorcode[errno.EIO])
print(errno.EWOULDBLOCK == errno.EAGAIN, type(errno.errorcode))

print(gc.isenabled(), gc.collect() >= 0, len(gc.get_count()), len(gc.get_threshold()))
gc.disable()
print(gc.isenabled())
gc.enable()
print(gc.isenabled(), isinstance(gc.get_objects(), list))
print(gc.garbage, gc.is_tracked([]), gc.is_tracked(1))

r = Random()
r.seed(42)
print([r.getrandbits(32) for _ in range(4)])
r.seed(42)
print(["%.15f" % r.random() for _ in range(3)])
r.seed(1234)
print(r.getrandbits(1) in (0, 1), r.getrandbits(0), r.getrandbits(100) < 2 ** 100)
state = r.getstate()
first = [r.random() for _ in range(3)]
r.setstate(state)
print(first == [r.random() for _ in range(3)], len(state))
r.seed(0)
print(r.getrandbits(64), r.getrandbits(3))
