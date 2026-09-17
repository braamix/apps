# signal over _signal: handlers set, raised, and put back.
import signal

print(signal.SIGINT, signal.SIGTERM, signal.SIGWINCH, repr(signal.SIGINT), signal.Signals(15).name)
print(signal.getsignal(signal.SIGINT) is signal.default_int_handler)
print(signal.getsignal(signal.SIGTERM), signal.SIG_DFL, signal.SIG_IGN, repr(signal.Handlers(1)))
try:
    signal.raise_signal(signal.SIGINT)
except KeyboardInterrupt as e:
    print("KeyboardInterrupt", e.args)

seen = []
def handler(signum, frame):
    seen.append((signum, type(frame).__name__))

old = signal.signal(signal.SIGTERM, handler)
print(old, signal.getsignal(signal.SIGTERM) is handler)
signal.raise_signal(signal.SIGTERM)
print(seen)
signal.signal(signal.SIGINT, handler)
signal.raise_signal(signal.SIGINT)
signal.raise_signal(signal.SIGINT)
print(seen[1:])
signal.signal(signal.SIGINT, signal.SIG_IGN)
signal.raise_signal(signal.SIGINT)
print("ignored", signal.getsignal(signal.SIGINT))
signal.signal(signal.SIGINT, signal.default_int_handler)
signal.signal(signal.SIGTERM, signal.SIG_DFL)

def raiser(signum, frame):
    raise RuntimeError("from handler %d" % signum)

signal.signal(signal.SIGWINCH, raiser)
try:
    signal.raise_signal(signal.SIGWINCH)
    print("not reached")
except RuntimeError as e:
    print(e)
signal.signal(signal.SIGWINCH, signal.SIG_DFL)

for bad in ((0, handler), (signal.SIGTERM, 5), (1000, signal.SIG_DFL)):
    try:
        signal.signal(*bad)
    except (ValueError, TypeError) as e:
        print(type(e).__name__, e)
print(signal.raise_signal(0))
try:
    signal.raise_signal(1000)
except OSError as e:
    print(type(e).__name__, e.errno)
print(signal.SIGINT in signal.valid_signals(), isinstance(signal.valid_signals(), set))
print(type(signal.set_wakeup_fd(-1)).__name__, signal.NSIG > 15)
print(signal.Signals.SIGINT is signal.SIGINT, int(signal.SIGTERM), signal.SIGTERM == 15)
