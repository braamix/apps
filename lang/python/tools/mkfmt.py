#!/usr/bin/env python3
"""Write the expected output of a formatting test, using the host's CPython.

    tools/mkfmt.py test/format/spec.py [more...]

These cases are ours, not an upstream's: each is a program that prints, and the
golden is what CPython prints for it. That makes the comparison as strong as it
can be -- the same program, the two interpreters, byte for byte -- and it is
why they are written to run unchanged on both.

So a case may not use what this interpreter has not got: an integer past 2**30,
a generator, eval, or a module beyond sys. A case that needs one of those is
not a formatting test.
"""

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def die(msg):
    sys.exit(f"mkfmt: {msg}")


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    for path in sys.argv[1:]:
        if not os.path.isfile(path):
            die(f"{path}: no such case")
        r = subprocess.run([sys.executable, os.path.abspath(path)],
                           capture_output=True, cwd=ROOT)
        if r.returncode != 0:
            die(f"{path}: CPython exited {r.returncode}\n"
                f"{r.stderr.decode(errors='replace')}")
        if r.stderr:
            die(f"{path}: CPython wrote to stderr\n{r.stderr.decode(errors='replace')}")
        text = r.stdout.decode("utf-8")
        with open(path + ".exp", "w", encoding="utf-8") as f:
            f.write(text)
        print(f"mkfmt: {os.path.basename(path)}: {text.count(chr(10))} lines "
              f"(CPython {sys.version_info.major}.{sys.version_info.minor})")


if __name__ == "__main__":
    main()
