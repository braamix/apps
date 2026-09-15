// import: what upstream's own tests do not reach.
//
// Loading a module reads a file and runs its body. So one import parks the VM
// on the driver and then pushes a frame, and an import inside an import inside
// an import must cost the native stack nothing. That is ground rules 1 and 2
// at once. The search path, `__file__`, `__path__` and the cache are this
// port's own arrangement, and upstream tests none of them.

import { boot, put, run, ok, die, same, H, CORE } from "./pylib.mjs";
import { join } from "node:path";

const { linkBytes } = await import(join(CORE, "web/fs.js"));

await boot("pyimport");

let bad = 0;
function check(what, got, want) {
    if (!same(what, got, want)) bad++;
}

// One run with a module tree planted beside it.
function withModules(files, source, env = "") {
    for (const [path, text] of Object.entries(files)) put("/tmp/" + path, text);
    put("/tmp/c.py", source);
    const r = run("/tmp/c.py", null, env);
    return r.out + r.err;
}

// A chain of fifty modules, each importing the next. Every one parks on a file
// read and then pushes a frame, so fifty nested say that neither grows the
// native stack.
{
    const files = {};
    for (let i = 0; i < 50; i++)
        files[`chain${i}.py`] =
            i === 49 ? "DEPTH = 0\n" : `import chain${i + 1}\nDEPTH = chain${i + 1}.DEPTH + 1\n`;
    check("fifty nested imports",
          withModules(files, "import chain0\nprint(chain0.DEPTH)\n"), "49\n");
}

// The module object: its name, its file, its namespace, and a package's path.
{
    const files = {
        "m/__init__.py": "V = 1\n",
        "m/leaf.py": "W = 2\n",
    };
    check("what a module carries",
          withModules(files,
              "import m.leaf\n" +
              "print(m.__name__, m.leaf.__name__)\n" +
              "print(m.__file__, m.leaf.__file__)\n" +
              "print(m.__path__)\n" +
              "print(sorted([k for k in m.__dict__ if not k.startswith('__')]))\n" +
              "print(m.__dict__['V'], hasattr(m.leaf, '__path__'))\n"),
          "m m.leaf\n/tmp/m/__init__.py /tmp/m/leaf.py\n['/tmp/m']\n" +
          "['V', 'leaf']\n1 False\n");
}

// The cache: a body runs once however many ways it is asked for, and the same
// object comes back every time.
{
    const files = { "once.py": "print('body')\nV = []\n" };
    check("a body runs once",
          withModules(files,
              "import once\n" +
              "import once as again\n" +
              "from once import V\n" +
              "import sys\n" +
              "print(once is again, V is once.V, sys.modules['once'] is once)\n"),
          "body\nTrue True True\n");
}

// A body that raises: the half-built module leaves the cache, so the next
// import runs it again.
{
    const files = { "bad.py": "print('trying')\nraise ValueError('no')\n" };
    check("a module that raises",
          withModules(files,
              "import sys\n" +
              "for i in range(2):\n" +
              "    try:\n" +
              "        import bad\n" +
              "    except ValueError as e:\n" +
              "        print('caught', e, 'bad' in sys.modules)\n"),
          "trying\ncaught no False\ntrying\ncaught no False\n");
}

// A directory with no __init__.py is a namespace package: a __path__ and
// nothing else, with no body to run.
{
    const files = { "ns/mod.py": "V = 7\n" };
    check("a namespace package",
          withModules(files,
              "import ns.mod\n" +
              "print(ns.mod.V, ns.__path__, hasattr(ns, '__file__'))\n"),
          "7 ['/tmp/ns'] False\n");
}

// Relative imports, counted back from the importing module's own package.
{
    const files = {
        "r/__init__.py": "TOP = 'top'\n",
        "r/one.py": "V = 1\n",
        "r/sub/__init__.py": "",
        "r/sub/two.py": "from .. import TOP\nfrom ..one import V\nfrom . import three\n" +
                        "W = TOP + str(V) + three.X\n",
        "r/sub/three.py": "X = 'three'\n",
    };
    check("relative imports", withModules(files, "from r.sub.two import W\nprint(W)\n"),
          "top1three\n");
}

// `import *`, with and without an __all__ to narrow it.
{
    const files = {
        "s1.py": "A = 1\n_B = 2\nC = 3\n",
        "s2.py": "A = 1\n_B = 2\nC = 3\n__all__ = ['A', '_B']\n",
    };
    check("import star",
          withModules(files,
              "from s1 import *\n" +
              "print(A, C, '_B' in dir() if False else 'no dir')\n" +
              "from s2 import *\n" +
              "print(A, _B)\n"),
          "1 3 no dir\n1 2\n");
}

// sys.path: the directory the program was read from comes first, and a program
// may put its own directory on it.
{
    const files = { "deeper/far.py": "V = 'far'\n" };
    check("sys.path",
          withModules(files,
              "import sys\n" +
              "print(sys.path[0])\n" +
              "try:\n" +
              "    import far\n" +
              "except ImportError:\n" +
              "    print('not yet')\n" +
              "sys.path.append('/tmp/deeper')\n" +
              "import far\n" +
              "print(far.V, far.__file__)\n"),
          "/tmp\nnot yet\nfar /tmp/deeper/far.py\n");
}

// A module printing while it loads. The driver is asked to write before it is
// asked to read, so the output keeps the program's order.
{
    const files = { "loud.py": "print('inside loud')\n" };
    check("output around a read",
          withModules(files, "print('before')\nimport loud\nprint('after')\n"),
          "before\ninside loud\nafter\n");
}

// The whole of it again with a collection at every allocation. An import holds
// a half-built module, a candidate list and a source across allocations, none
// of it on the C++ stack.
{
    const files = {
        "g/__init__.py": "V = 'pkg'\n",
        "g/mod.py": "from . import V\nW = V + '!'\n",
        "gns/mod.py": "Z = 1\n",
    };
    const src = "import g.mod\nimport gns.mod\nfrom g.mod import W\n" +
                "import sys\nprint(W, g.mod.__name__, gns.__path__, len(sys.modules) > 3)\n";
    const plain = withModules(files, src);
    const under = withModules(files, src, "PY_GC_STRESS=1");
    check("the same under gc stress", under, plain);
    check("and what it said", plain, "pkg! g.mod ['/tmp/gns'] True\n");
}

// The shipped library, found the way mbasic finds its examples. pkg writes
// /pkg/bin/python as a symlink into the store, and readlink does not follow
// the leaf, so one syscall recovers the prefix. Last, because it moves the
// planted binary.
{
    const store = "/pkg/store/python-0.1-r0";
    const binary = H.store.files.get("/bin/py");
    put(`${store}/bin/python`, binary);
    put(`${store}/share/lib/shipped.py`, "GREETING = 'from the store'\n");
    for (const d of ["/pkg", "/pkg/bin"]) H.store.dirs.add(d);
    H.store.files.set("/pkg/bin/python", linkBytes(`${store}/bin/python`));
    H.store.files.set("/bin/py", binary);

    put("/tmp/c.py", "import sys\nprint(sys.path)\nimport shipped\nprint(shipped.GREETING)\n");
    const r = run("/tmp/c.py");
    check("the library in the store", r.out + r.err,
          `['/tmp', '${store}/share/lib']\nfrom the store\n`);
}

if (bad) die(`${bad} checks failed`);
ok("modules, packages, the search path, and the loader under the collector");
