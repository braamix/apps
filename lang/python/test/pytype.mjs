// The rest of the type system: the descriptor protocol, __slots__, the
// metaclasses, the finalizers and the abstract base classes.
//
// Two halves. The cases under test/type/ are programs that print, run by
// CPython and by this interpreter and compared byte for byte -- which is the
// strongest ruler there is, and what says a descriptor here resolves the way
// one there does. What cannot be written that way is asserted here instead:
// a finalizer runs when the collector gets to it rather than when the last
// name goes, so its *timing* is this port's and only its *effect* is Python's;
// and the abstract base classes need CPython's own abc.py planted beside the
// case, which is the first library module this tree borrows.

import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, put, script, ok, die, same, against_cpython } from "./pylib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const LIB = join(HERE, "..", "lib");
const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

await boot("pytype");

let bad = 0;
function check(what, got, want) {
    if (!same(what, got, want)) bad++;
}

// ---------------------------------------------------------- against CPython

// CPython's own abc.py, byte for byte, over the native _abc; lib/manifest.txt
// records where it came from. A case may import it.
put("/tmp/abc.py", readFileSync(join(LIB, "abc.py")));

const { bad: differ, ran, lines } = against_cpython(join(HERE, "type"), only);
bad += differ;
if (!ran) die(only.length ? "no case matched" : "no cases under test/type/");

// ------------------------------------------------------------- the finalizers

// The collector is what runs a __del__, so the program has to make garbage
// before one is owed. `churn` is that, and it is why this is not a golden.
const CHURN = "def churn():\n" +
              "    for i in range(20000):\n" +
              "        x = [i] * 10\n";

{
    const r = script(CHURN +
        "class C:\n" +
        "    def __init__(s, n): s.n = n\n" +
        "    def __del__(s): print('del', s.n)\n" +
        "C(1)\n" +
        "churn()\n" +
        "print('after')\n" +
        "C(2)\n" +
        "churn()\n" +
        "print('end')\n");
    check("__del__ runs, once per object", r.out + r.err,
          "del 1\nafter\ndel 2\nend\n");
}

// Resurrection: a __del__ that stores self keeps the object, and is never
// owed a second time. That is the rule the sweep has to state, because an
// object it has already finalized is an ordinary object again.
{
    const r = script(CHURN +
        "kept = []\n" +
        "class R:\n" +
        "    def __del__(s):\n" +
        "        print('del')\n" +
        "        kept.append(s)\n" +
        "R()\n" +
        "churn()\n" +
        "print('kept', len(kept))\n" +
        "kept.clear()\n" +
        "churn()\n" +
        "churn()\n" +
        "print('end')\n");
    check("a finalizer may resurrect, and runs once", r.out + r.err,
          "del\nkept 1\nend\n");
}

// An exception out of a finalizer is reported and goes no further: there is
// no statement it came from for a handler to belong to.
{
    const r = script(CHURN +
        "class B:\n" +
        "    def __del__(s): raise ValueError('inside')\n" +
        "B()\n" +
        "churn()\n" +
        "print('still here')\n");
    check("a finalizer's exception is reported", r.err,
          "Exception ignored in a finalizer:\nValueError: inside\n");
    check("and the program carries on", r.out, "still here\n");
    check("and still succeeds", String(r.status), "0");
}

// A generator dropped at a yield owes its `finally` a run, which is the other
// thing a finalizer is for.
{
    const r = script(CHURN +
        "def g():\n" +
        "    try:\n" +
        "        yield 1\n" +
        "        yield 2\n" +
        "    finally:\n" +
        "        print('finally')\n" +
        "it = g()\n" +
        "print(next(it))\n" +
        "it = None\n" +
        "churn()\n" +
        "print('end')\n");
    check("a dropped generator runs its finally", r.out + r.err,
          "1\nfinally\nend\n");
}

// A generator that ran out needs no close, and a never-started one has no
// `finally` to run.
{
    const r = script(CHURN +
        "def g():\n" +
        "    try:\n" +
        "        yield 1\n" +
        "    finally:\n" +
        "        print('finally')\n" +
        "print(list(g()))\n" +
        "h = g()\n" +
        "h = None\n" +
        "churn()\n" +
        "print('end')\n");
    check("an exhausted generator is not closed twice", r.out + r.err,
          "finally\n[1]\nend\n");
}

// ------------------------------------------------------------ weak references

{
    const r = script(CHURN +
        "import _weakref\n" +
        "class C: pass\n" +
        "c = C()\n" +
        "r = _weakref.ref(c)\n" +
        "print(r() is c, _weakref.getweakrefcount(c), len(_weakref.getweakrefs(c)))\n" +
        "print(r == _weakref.ref(c), r == _weakref.ref(C()))\n" +
        "def gone(ref): print('callback', ref() is None)\n" +
        "r2 = _weakref.ref(c, gone)\n" +
        "c = None\n" +
        "churn()\n" +
        "print('dead', r() is None, r2() is None)\n");
    check("a weak reference clears, and its callback is told", r.out + r.err,
          "True 1 1\nTrue False\ncallback True\ndead True True\n");
}

// __del__ first and the callback after, which is the order CPython keeps: the
// finalizer must never see a reference that is already dangling.
{
    const r = script(CHURN +
        "import _weakref\n" +
        "class C:\n" +
        "    def __del__(s): print('del')\n" +
        "c = C()\n" +
        "r = _weakref.ref(c, lambda ref: print('callback'))\n" +
        "c = None\n" +
        "churn()\n" +
        "print('end')\n");
    check("__del__ runs before the weakref callback", r.out + r.err,
          "del\ncallback\nend\n");
}

{
    const r = script("import _weakref\n" +
        "try:\n" +
        "    _weakref.ref(1)\n" +
        "except TypeError as e:\n" +
        "    print('TypeError')\n" +
        "class C: pass\n" +
        "print(type(_weakref.ref(C())) is _weakref.ReferenceType)\n");
    check("only what the collector owns is referenceable", r.out + r.err,
          "TypeError\nTrue\n");
}

// ------------------------------------------------------------------------ abc

// CPython's own abc.py, planted above, over the native _abc.
{
    const r = script(
        "from abc import ABC, ABCMeta, abstractmethod\n" +
        "class Drawable(metaclass=ABCMeta):\n" +
        "    @abstractmethod\n" +
        "    def draw(self): ...\n" +
        "class Square(Drawable):\n" +
        "    def draw(self): return 'square'\n" +
        "print(Square().draw())\n" +
        "try:\n" +
        "    Drawable()\n" +
        "except TypeError as e:\n" +
        "    print('TypeError:', e)\n" +
        "print(sorted(Drawable.__abstractmethods__), Square.__abstractmethods__)\n");
    check("an abstract class cannot be instantiated", r.out + r.err,
          "square\n" +
          "TypeError: Can't instantiate abstract class Drawable with abstract method draw\n" +
          "['draw'] []\n");
}

{
    const r = script(
        "from abc import ABC\n" +
        "class Drawable(ABC): pass\n" +
        "class Duck:\n" +
        "    def draw(self): return 'quack'\n" +
        "class Rock: pass\n" +
        "print(isinstance(Duck(), Drawable), issubclass(Duck, Drawable))\n" +
        "Drawable.register(Duck)\n" +
        "print(isinstance(Duck(), Drawable), issubclass(Duck, Drawable))\n" +
        "class Duckling(Duck): pass\n" +
        "print(issubclass(Duckling, Drawable), isinstance(Rock(), Drawable))\n");
    check("a registered class is a virtual subclass, and so are its own",
          r.out + r.err, "False False\nTrue True\nTrue False\n");
}

{
    const r = script(
        "from abc import ABC, get_cache_token\n" +
        "class Named(ABC):\n" +
        "    @classmethod\n" +
        "    def __subclasshook__(cls, C):\n" +
        "        if cls is Named:\n" +
        "            return any('name' in B.__dict__ for B in C.__mro__)\n" +
        "        return NotImplemented\n" +
        "class Has:\n" +
        "    name = 'x'\n" +
        "class Hasnt: pass\n" +
        "print(issubclass(Has, Named), issubclass(Hasnt, Named))\n" +
        "print(isinstance(Has(), Named), isinstance(Hasnt(), Named))\n" +
        "t = get_cache_token()\n" +
        "Named.register(Hasnt)\n" +
        "print(get_cache_token() != t)\n");
    check("__subclasshook__ decides, and registering moves the token",
          r.out + r.err, "True False\nTrue False\nTrue\n");
}

// The whole of it once more with the collector never waiting, because the
// checks park in continuations that hold classes across a call.
{
    const r = script(
        "from abc import ABC, abstractmethod\n" +
        "class A(ABC):\n" +
        "    @abstractmethod\n" +
        "    def f(self): ...\n" +
        "class B(A):\n" +
        "    def f(self): return 1\n" +
        "class C: pass\n" +
        "A.register(C)\n" +
        "print(sum(isinstance(x, A) for x in (B(), C(), object())))\n",
        null);
    check("the checks survive a collector that never waits", r.out + r.err, "2\n");
}

if (bad) {
    console.error(`\npytype: ${bad} checks failed`);
    process.exit(1);
}
ok(`${ran} cases, ${lines} lines identical to CPython's; the finalizers, the ` +
   `weak references and CPython's own abc.py over a native _abc`);
