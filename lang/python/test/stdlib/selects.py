# select.poll and select.select over pipes, and the selectors and subprocess
# layers above them. Nothing prints a descriptor number: the numbers differ
# between a CPython on a desktop and this one, so what is checked is which
# object came back and what it was ready for.
import os
import select
import selectors
import subprocess


def say(*args, end="\n"):
    print(*args, end=end, flush=True)


say(select.POLLIN, select.POLLPRI, select.POLLOUT, select.POLLERR,
    select.POLLHUP, select.POLLNVAL)

# --- poll: a pipe before and after a write, and at its end
r, w = os.pipe()
p = select.poll()
p.register(r, select.POLLIN)
say("quiet", p.poll(0))
os.write(w, b"one")
say("ready", [(fd == r, ev & select.POLLIN != 0) for fd, ev in p.poll(0)])
say("read", os.read(r, 10))
os.close(w)
say("eof", [(fd == r, ev & (select.POLLIN | select.POLLHUP) != 0) for fd, ev in p.poll(0)])
p.unregister(r)
say("gone", p.poll(0))
os.close(r)

# --- poll: what it refuses
p = select.poll()
try:
    p.modify(0, select.POLLIN)
except OSError as e:
    say("modify", type(e).__name__)
try:
    p.unregister(0)
except KeyError:
    say("unregister", "KeyError")
try:
    p.register(object())
except TypeError as e:
    say("register", e)

# --- poll: a file is always ready, and an object with fileno() is taken
with open("/tmp/selects.txt", "w") as f:
    f.write("body")
with open("/tmp/selects.txt") as f:
    p = select.poll()
    p.register(f, select.POLLIN)
    say("file", [ev & select.POLLIN != 0 for _, ev in p.poll(0)])
os.remove("/tmp/selects.txt")

# --- select: the objects it was given come back, and only the ready ones
r, w = os.pipe()
say("select quiet", select.select([r], [w], [], 0) == ([], [w], []))
os.write(w, b"two")
say("select ready", select.select([r], [w], [], 0) == ([r], [w], []))
os.read(r, 10)
say("select empty", select.select([], [], [], 0))
os.close(r)
os.close(w)


class Named:
    def __init__(self, fd):
        self.fd = fd

    def fileno(self):
        return self.fd

    def __repr__(self):
        return "Named"


r, w = os.pipe()
os.write(w, b"three")
say("select object", select.select([Named(r)], [], [], 0))
os.read(r, 10)
os.close(r)
os.close(w)

try:
    select.select([object()], [], [], 0)
except TypeError as e:
    say("select type", e)
try:
    select.select([], [], [], -1)
except ValueError as e:
    say("select negative", e)

# --- selectors, which picks PollSelector now that select.poll exists
say("selector", selectors.PollSelector is selectors.DefaultSelector or
    hasattr(selectors, "PollSelector"))
s = selectors.PollSelector()
r, w = os.pipe()
key = s.register(r, selectors.EVENT_READ, "the reader")
say("selector quiet", s.select(0))
os.write(w, b"four")
for k, ev in s.select(0):
    say("selector ready", k.data, ev == selectors.EVENT_READ, os.read(k.fd, 10))
s.unregister(r)
s.close()
os.close(r)
os.close(w)

# --- subprocess: two and three pipes at once, which is what poll buys
p = subprocess.Popen(["sh", "-c", "cat; echo err >&2"], stdin=subprocess.PIPE,
                     stdout=subprocess.PIPE, stderr=subprocess.PIPE)
say("communicate", p.communicate(b"body\n"), p.returncode)

say("capture", subprocess.run(["sh", "-c", "echo out; echo oops >&2"],
                              capture_output=True))

# Enough to fill a pipe several times over in both directions at once, which
# is the deadlock one pipe at a time cannot avoid.
body = b"a line of text\n" * 8000
p = subprocess.Popen(["cat"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
out, err = p.communicate(body)
say("through cat", len(out), out == body, err)
