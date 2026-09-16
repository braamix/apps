"""The reference interpreter every golden writer uses, and the record of which
one wrote each golden.

    PYTHON=tmp/cpython-build/python.exe tools/mkfmt.py test/exec/lazy.py

$PYTHON names the interpreter; the one on PATH is the default. A tool that
imports CPython's own modules in-process -- mklex.py, mkast.py -- re-executes
itself under it; the others run it as a child.

test/goldens.txt says which interpreter wrote each golden of our own cases, as
the manifest's exp column does for MicroPython's. A golden recorded under one
interpreter is not rewritten by another unless --regen says so, so a new host
cannot change the ruler by accident.
"""

import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
RECORD = os.path.join(ROOT, "test", "goldens.txt")

HEAD = """\
# Which CPython wrote each golden of our own cases. tools/pyref.py keeps it;
# the manifest's exp column says the same for MicroPython's.
#
#   golden       the case, relative to test/
#   interpreter  cpython<major>.<minor>
#
# A case missing here was written by hand or blessed from this interpreter.
"""


def interpreter():
    """The absolute path of the reference interpreter."""
    want = os.environ.get("PYTHON") or "python3"
    path = shutil.which(want) or (os.path.abspath(want) if os.path.isfile(want) else None)
    if not path:
        sys.exit(f"pyref: no interpreter at {want}")
    return os.path.abspath(path)


def tag_of(path):
    r = subprocess.run([path, "-c", "import sys; print(sys.version_info[0], sys.version_info[1])"],
                       capture_output=True, text=True, check=True)
    major, minor = r.stdout.split()
    return f"cpython{major}.{minor}"


def reexec():
    """Run the rest of this tool under $PYTHON when that is not us."""
    want = interpreter()
    if os.path.realpath(want) != os.path.realpath(sys.executable):
        os.execv(want, [want] + sys.argv)


def tag():
    """This interpreter's tag, for a tool running in-process."""
    return f"cpython{sys.version_info.major}.{sys.version_info.minor}"


def read():
    rows = {}
    if os.path.isfile(RECORD):
        with open(RECORD, encoding="utf-8") as f:
            for line in f:
                t = line.split()
                if t and not t[0].startswith("#"):
                    rows[t[0]] = t[1]
    return rows


def key_of(case):
    return os.path.relpath(os.path.abspath(case), os.path.join(ROOT, "test"))


def check(case, want, regen):
    """Refuse to overwrite a golden another interpreter wrote."""
    had = read().get(key_of(case))
    if had and had != want and not regen and os.path.isfile(case + ".exp"):
        sys.exit(f"pyref: {key_of(case)} was written by {had}, not {want}; "
                 f"pass --regen to rewrite it")


def record(case, tag):
    rows = read()
    rows[key_of(case)] = tag
    width = max(len(k) for k in rows)
    with open(RECORD, "w", encoding="utf-8") as f:
        f.write(HEAD)
        for k in sorted(rows):
            f.write(f"{k.ljust(width)}  {rows[k]}\n")


def args():
    """The case paths, and whether --regen was given."""
    rest = [a for a in sys.argv[1:] if a != "--regen"]
    return rest, len(rest) != len(sys.argv) - 1
