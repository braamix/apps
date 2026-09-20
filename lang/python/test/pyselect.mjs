// Where select differs from CPython's, which test/stdlib/selects.py cannot
// say because every line of that file is compared against CPython's own
// output. All three differences are the kernel's Sys::Poll showing through:
// it watches a descriptor in the one direction that descriptor has, it takes
// at most SYS_POLL_MAX of them at a time, and its whole event set is IN, OUT
// and HUP. Manual.md §7 is where they are written down.

import { boot, script, same, ok } from "./pylib.mjs";

await boot("pyselect");
let bad = 0;
const check = (what, got, want) => {
    if (!same(what, got, want)) bad++;
};

// A pipe end has one direction, and asking after the other is EBADF -- which
// is what register()'s default mask of POLLIN | POLLPRI | POLLOUT does. So a
// program that polls here names the direction it wants.
check("a direction a pipe has not got", script(`
import os, select
r, w = os.pipe()
for fd, mask in ((r, None), (r, select.POLLOUT), (w, select.POLLIN)):
    p = select.poll()
    p.register(fd) if mask is None else p.register(fd, mask)
    try:
        print(p.poll(0))
    except OSError as e:
        print(type(e).__name__, e.errno, e.strerror)
p = select.poll()
p.register(r, select.POLLIN)
print("named", p.poll(0))
`).out,
      "OSError 9 Bad file descriptor\n".repeat(3) +
      "named []\n");

// A descriptor that was closed is the same answer, and it is CPython's.
check("a descriptor that is not open", script(`
import select
p = select.poll()
p.register(4095, select.POLLIN)
try:
    p.poll(0)
except OSError as e:
    print(type(e).__name__, e.errno)
try:
    select.select([4095], [], [], 0)
except OSError as e:
    print(type(e).__name__, e.errno)
`).out, "OSError 9\nOSError 9\n");

// Sixty-four at a time, because each is armed on its channel for the length
// of the call. CPython has no such bound.
check("more than the kernel takes", script(`
import os, select
pipes = [os.pipe() for _ in range(33)]
p = select.poll()
for r, w in pipes:
    p.register(r, select.POLLIN)
    p.register(w, select.POLLOUT)
try:
    p.poll(0)
except ValueError as e:
    print(e)
p.unregister(pipes[-1][0])
p.unregister(pipes[-1][1])
print(len(p.poll(0)))
for r, w in pipes:
    os.close(r)
    os.close(w)
`).out, "too many descriptors to poll: 66\n32\n");

// POLLPRI, POLLERR and POLLNVAL can be asked for and never come back, so a
// descriptor registered for one of them alone is never ready -- not even a
// closed far end, which HUP does report beside a direction that was asked for.
check("the events that never fire", script(`
import os, select
r, w = os.pipe()
os.close(w)
for mask in (select.POLLPRI, select.POLLERR, select.POLLNVAL,
             select.POLLIN | select.POLLPRI):
    p = select.poll()
    p.register(r, mask)
    print([ev for _, ev in p.poll(0)])
os.close(r)
`).out, "[]\n[]\n[]\n[17]\n");

if (bad) {
    console.error(`\npyselect: ${bad} checks failed`);
    process.exit(1);
}
ok("select answers for one direction, 64 descriptors, and three events");
