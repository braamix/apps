// The command line past -c, -m and -i, and the PYTHON* variables: each flag is
// checked by what the program sees of it. A command line here has sixty
// characters, so the programs are files planted under short names.

import { boot, put, run, same, ok } from "./pylib.mjs";

await boot("pyflags");
let bad = 0;
const check = (what, got, want) => {
    if (!same(what, got, want)) bad++;
};

put("/tmp/f.py",
    "import sys\n" +
    "f = sys.flags\n" +
    "print(f.optimize, f.verbose, f.bytes_warning, f.quiet, f.no_site,\n" +
    "      f.ignore_environment, f.isolated, f.safe_path, f.dev_mode,\n" +
    "      f.int_max_str_digits, f.inspect)\n" +
    "print(sys.warnoptions, sys._xoptions)\n");
put("/tmp/o.py",
    "def f():\n" +
    "    'doc'\n" +
    "print(__debug__, f.__doc__)\n" +
    "assert 0, 'kept'\n");
put("/tmp/p.py", "import sys\nprint(sys.path[:2])\n");

const flags = (tail, env = "") => run(tail, null, env).out;

check("the defaults", flags("/tmp/f.py"),
    "0 0 0 0 0 0 0 False False 4300 0\n[] {}\n");
check("-O -v -b -q -B -s", flags("-O -v -b -q -B -s /tmp/f.py"),
    "1 1 1 1 0 0 0 False False 4300 0\n[] {}\n");
check("-OO -vv -bb -S", flags("-OOvvbbS /tmp/f.py"),
    "2 2 2 0 1 0 0 False False 4300 0\n[] {}\n");
check("-I", flags("-I /tmp/f.py"),
    "0 0 0 0 0 1 1 True False 4300 0\n[] {}\n");
check("-W and -X", flags("-W error -X dev -X a=1 /tmp/f.py"),
    "0 0 0 0 0 0 0 False True 4300 0\n['default', 'error'] {'dev': True, 'a': '1'}\n");
check("-X int_max_str_digits", flags("-X int_max_str_digits=0 /tmp/f.py"),
    "0 0 0 0 0 0 0 False False 0 0\n[] {'int_max_str_digits': '0'}\n");

// The environment, and -E ignoring it.
check("PYTHONOPTIMIZE", flags("/tmp/f.py", "PYTHONOPTIMIZE=2"),
    "2 0 0 0 0 0 0 False False 4300 0\n[] {}\n");
check("PYTHONWARNINGS", flags("/tmp/f.py", "PYTHONWARNINGS=ignore,once"),
    "0 0 0 0 0 0 0 False False 4300 0\n['ignore', 'once'] {}\n");
check("PYTHONWARNINGS before -W", flags("-Werror /tmp/f.py", "PYTHONWARNINGS=ignore"),
    "0 0 0 0 0 0 0 False False 4300 0\n['ignore', 'error'] {}\n");
check("PYTHONDEVMODE", flags("/tmp/f.py", "PYTHONDEVMODE=1"),
    "0 0 0 0 0 0 0 False True 4300 0\n['default'] {}\n");
check("-E", flags("-E /tmp/f.py", "PYTHONOPTIMIZE=1"),
    "0 0 0 0 0 1 0 False False 4300 0\n[] {}\n");
check("PYTHONINTMAXSTRDIGITS", flags("/tmp/f.py", "PYTHONINTMAXSTRDIGITS=700"),
    "0 0 0 0 0 0 0 False False 700 0\n[] {}\n");

{
    const r = run("-X int_max_str_digits=5 -c 1");
    check("a bad digit limit", r.err,
        "python: -X int_max_str_digits: invalid limit; must be >= 640 or 0 for unlimited\n");
    check("is refused", String(r.status), "1");
}

// -O drops asserts and __debug__, -OO docstrings as well.
{
    const r = run("/tmp/o.py");
    check("no -O", r.out, "True doc\n");
    check("the assert is kept", r.err.split("\n").slice(-2).join("\n"), "AssertionError: kept\n");
}
check("-O", flags("-O /tmp/o.py"), "False doc\n");
check("-OO", flags("-OO /tmp/o.py"), "False None\n");
check("PYTHONOPTIMIZE", flags("/tmp/o.py", "PYTHONOPTIMIZE=1"), "False doc\n");

// sys.path: PYTHONPATH after the program's directory, which -P and
// PYTHONSAFEPATH leave off.
check("sys.path", flags("/tmp/p.py"), "['/tmp', '/pkg/store/python-0/lib']\n");
check("PYTHONPATH", flags("/tmp/p.py", "PYTHONPATH=/a:/b"), "['/tmp', '/a']\n");
check("-P", flags("-P /tmp/p.py"), "['/pkg/store/python-0/lib']\n");
check("PYTHONSAFEPATH", flags("/tmp/p.py", "PYTHONSAFEPATH=1"), "['/pkg/store/python-0/lib']\n");

// site: imported unless -S, and what it puts in builtins.
put("/tmp/s.py", "import sys\nprint('site' in sys.modules, repr(exit), repr(quit))\n");
check("site", flags("/tmp/s.py"),
    "True Use exit() or Ctrl-D (i.e. EOF) to exit Use quit() or Ctrl-D (i.e. EOF) to exit\n");
{
    const r = run("-S /tmp/s.py");
    check("-S", r.err.split("\n").slice(-2).join("\n"), "NameError: name 'exit' is not defined\n");
}

// -v says what each import loads, from where.
{
    put("/tmp/v.py", "import keyword\n");
    const lines = run("-v /tmp/v.py").err.split("\n");
    check("-v, the code object", String(lines.includes(
        "# code object from '/pkg/store/python-0/lib/keyword.py'")), "true");
    check("-v, the module", String(lines.includes(
        "import 'keyword' # from '/pkg/store/python-0/lib/keyword.py'")), "true");
    check("-v, a built-in", String(lines.includes("import 'posix' # built-in")), "true");
    const t = run("-X importtime /tmp/v.py").err.split("\n");
    check("-X importtime, the header", t[0], "import time: self [us] | cumulative | imported package");
    check("-X importtime, a line", String(t.includes("import time:         0 |          0 | keyword")),
        "true");
}

// -b warns, -bb raises.
{
    put("/tmp/b.py", "x = str(b'a')\ny = b'a' == 'a'\nprint('done')\n");
    const r = run("-b /tmp/b.py");
    check("-b", r.out + r.err,
        "done\n" +
        "/tmp/b.py:1: BytesWarning: str() on a bytes instance\n" +
        "  x = str(b'a')\n" +
        "/tmp/b.py:2: BytesWarning: Comparison between bytes and string\n" +
        "  y = b'a' == 'a'\n");
    const s = run("-bb /tmp/b.py");
    check("-bb", s.err.split("\n").slice(-2).join("\n"), "BytesWarning: str() on a bytes instance\n");
    check("-bb status", String(s.status), "1");
    check("no -b", flags("/tmp/b.py"), "done\n");
}

// PYTHONSTARTUP runs before the first prompt, over __main__.
{
    put("/tmp/st.py", "greeting = 'from startup'\n");
    const r = run("-i", "print(greeting)\n", "PYTHONSTARTUP=/tmp/st.py");
    check("PYTHONSTARTUP", r.out, "from startup\n");
    const e = run("-i -E", "print(1)\n", "PYTHONSTARTUP=/tmp/st.py");
    check("-E ignores it", e.out, "1\n");
}

// faulthandler writes the stack straight to the descriptor.
{
    put("/tmp/h.py",
        "import faulthandler\n" +
        "def f():\n" +
        "    faulthandler.dump_traceback(all_threads=False)\n" +
        "f()\n" +
        "print(faulthandler.is_enabled(), faulthandler.enable(),\n" +
        "      faulthandler.is_enabled(), faulthandler.disable())\n");
    const r = run("/tmp/h.py");
    check("faulthandler", r.out + r.err,
        "False None True True\n" +
        "Stack (most recent call first):\n" +
        '  File "/tmp/h.py", line 3 in f\n' +
        '  File "/tmp/h.py", line 4 in <module>\n');
}

if (bad) {
    console.error(`pyflags: ${bad} checks failed`);
    process.exit(1);
}
ok("the flags, the PYTHON* variables, site, -v, -b and faulthandler");
