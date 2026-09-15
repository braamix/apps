// Classes: what upstream's class*, subclass_native* and special_methods tests
// cannot reach from inside the language.
//
// Two things are asked here and nowhere else. A special method written in
// Python is a *call*, and the VM has to make it where a frame can be pushed --
// so the question is whether the machinery survives depth, garbage and an
// exception thrown through it. And the collector now has a cycle it did not
// have before: a class holds its methods, a method holds its class.

import { boot, put, run, script, ok, die, same } from "./pylib.mjs";

await boot("pyclass");

let bad = 0;
function check(what, got, want) {
    if (!same(what, got, want)) bad++;
}

// C3 over a diamond, which is the whole reason the linearisation is not just
// a depth-first walk: D sees B before C, and C before A.
{
    const r = script(
        "class A:\n    def who(self): return 'A'\n" +
        "class B(A):\n    def who(self): return 'B' + super().who()\n" +
        "class C(A):\n    def who(self): return 'C' + super().who()\n" +
        "class D(B, C):\n    def who(self): return 'D' + super().who()\n" +
        "print(D().who())\n" +
        "print([c.__name__ for c in [D, B, C, A, object]])\n");
    check("C3 over a diamond", r.out + r.err,
          "DBCA\n['D', 'B', 'C', 'A', 'object']\n");
}

// An operator on a class is a Python call the VM makes at the opcode; a
// thousand of them must not grow the native stack, the way a key function
// must not.
{
    const r = script(
        "class N:\n" +
        "    def __init__(self, v): self.v = v\n" +
        "    def __add__(self, o): return N(self.v + o.v)\n" +
        "    def __len__(self): return self.v\n" +
        "acc = N(0)\n" +
        "for i in range(1000):\n" +
        "    acc = acc + N(i)\n" +
        "print(acc.v, len(acc))\n");
    check("a thousand operator calls", r.out + r.err, "499500 499500\n");
}

// The reflected call, and NotImplemented meaning it is the other one's turn.
// One class on both sides gets no reflected call: it would be the same method.
{
    const r = script(
        "class L:\n" +
        "    def __sub__(self, o):\n" +
        "        print('L.__sub__')\n" +
        "        return NotImplemented\n" +
        "class R:\n" +
        "    def __rsub__(self, o):\n" +
        "        print('R.__rsub__')\n" +
        "        return 'answered'\n" +
        "print(L() - R())\n" +
        "try:\n" +
        "    L() - L()\n" +
        "except TypeError as e:\n" +
        "    print('TypeError')\n");
    check("NotImplemented and the reflected call", r.out + r.err,
          "L.__sub__\nR.__rsub__\nanswered\nL.__sub__\nTypeError\n");
}

// A `for` over a class: __iter__ and __next__ are calls, and the StopIteration
// that ends the loop is caught by the continuation rather than unwound.
{
    const r = script(
        "class Count:\n" +
        "    def __init__(self, n): self.n = n; self.i = 0\n" +
        "    def __iter__(self): return self\n" +
        "    def __next__(self):\n" +
        "        if self.i >= self.n: raise StopIteration\n" +
        "        self.i = self.i + 1\n" +
        "        return self.i\n" +
        "print([x for x in Count(4)])\n" +
        "t = 0\n" +
        "for x in Count(500):\n" +
        "    t = t + x\n" +
        "print(t)\n");
    check("iterating a class", r.out + r.err, "[1, 2, 3, 4]\n125250\n");
}

// An exception raised inside a special method unwinds through the VM's own
// continuation and is caught where it should be.
{
    const r = script(
        "class Bad:\n" +
        "    def __getitem__(self, k): raise KeyError(k)\n" +
        "try:\n" +
        "    Bad()['x']\n" +
        "except KeyError as e:\n" +
        "    print('KeyError', e.args)\n" +
        "print('still here')\n");
    check("an exception out of __getitem__", r.out + r.err,
          "KeyError ('x',)\nstill here\n");
}

// A class of one's own deriving from an exception: raising it, catching it by
// its own name and by its base, and the arguments surviving __init__.
{
    const r = script(
        "class AppError(Exception):\n" +
        "    def __init__(self, code):\n" +
        "        self.code = code\n" +
        "        super().__init__(code)\n" +
        "try:\n" +
        "    raise AppError(7)\n" +
        "except Exception as e:\n" +
        "    print(type(e).__name__, e.code, e.args, isinstance(e, AppError))\n" +
        "try:\n" +
        "    raise AppError(1)\n" +
        "except AppError:\n" +
        "    print('by its own name')\n");
    check("a user exception class", r.out + r.err,
          "AppError 7 (7,) True\nby its own name\n");
}

// An uncaught one prints its own name, not its base's.
{
    const r = script("class Boom(RuntimeError): pass\nraise Boom('why')\n");
    if (!r.err.endsWith("Boom: why\n")) die(`an uncaught user exception: ${JSON.stringify(r.err)}`);
    check("its status", String(r.status), "1");
}

// The collector, with classes in the heap: a class holds its methods and each
// method holds the class back, so every instance is a cycle. At every
// allocation, which is what turns a missing Root into a wrong answer.
{
    put("/tmp/c.py",
        "class Node:\n" +
        "    def __init__(self, v, nxt): self.v = v; self.nxt = nxt\n" +
        "    def total(self):\n" +
        "        return self.v + (self.nxt.total() if self.nxt else 0)\n" +
        "    def __repr__(self): return 'Node(' + str(self.v) + ')'\n" +
        "n = None\n" +
        "for i in range(12):\n" +
        "    n = Node(i, n)\n" +
        "print(n.total(), n)\n");
    const r = run("/tmp/c.py", null, "PY_GC_STRESS=1");
    check("classes under gc stress", r.out + r.err, "66 Node(11)\n");
}

// __repr__ inside a container. The container's own repr is C++ and cannot call
// Python, so what is shown is a copy with the text already in it.
{
    const r = script(
        "class P:\n" +
        "    def __init__(self, x): self.x = x\n" +
        "    def __repr__(self): return 'P(' + str(self.x) + ')'\n" +
        "    def __str__(self): return 'p' + str(self.x)\n" +
        "p = [P(1), (P(2), {3: P(4)})]\n" +
        "print(p)\n" +
        "print(repr(p) == str(p), str(P(5)), repr(P(5)))\n" +
        "print(p[0].x, p[1][1][3].x)\n");
    check("__repr__ inside a container", r.out + r.err,
          "[P(1), (P(2), {3: P(4)})]\nTrue p5 P(5)\n1 4\n");
}

// A property is a getter the VM runs, and its setter is another.
{
    const r = script(
        "class T:\n" +
        "    def __init__(self): self._v = 0\n" +
        "    @property\n" +
        "    def v(self): return self._v * 2\n" +
        "    @v.setter\n" +
        "    def v(self, x): self._v = x + 1\n" +
        "t = T()\n" +
        "print(t.v)\n" +
        "t.v = 10\n" +
        "print(t.v, t._v)\n");
    check("a property both ways", r.out + r.err, "0\n22 11\n");
}

// Deep recursion through __getattr__ is a RecursionError, not a trap: the
// frames are ours whichever way a call is reached.
{
    const r = script(
        "class Deep:\n" +
        "    def __getattr__(self, n): return getattr(self, n)\n" +
        "try:\n" +
        "    Deep().nope\n" +
        "except RecursionError:\n" +
        "    print('RecursionError')\n");
    check("recursion through __getattr__", r.out + r.err, "RecursionError\n");
}

if (bad) {
    console.error(`pyclass: ${bad} checks failed`);
    process.exit(1);
}
ok("classes, the MRO, special methods and exceptions of one's own");
