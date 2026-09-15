// Calls: what upstream's fun_*, closure*, lambda* and scope* tests cannot
// reach from inside the language.
//
// Mostly the rule ground rule 2 states -- a builtin that must call back into
// Python hands the VM a continuation and never re-enters the dispatch loop.
// Whether that holds is a question about the *native* stack, so it is asked
// here, with a callback count no upstream test has.

import { boot, put, run, script, ok, die, same } from "./pylib.mjs";

await boot("pyfun");

let bad = 0;
function check(what, got, want) {
    if (!same(what, got, want)) bad++;
}

// A key function called four thousand times. A co_await is a call and not a
// tail call, and neither is entering a task: if the continuation re-entered
// the loop instead of returning to it, this many turns would trap.
{
    const r = script(
        "xs = list(range(4000))\n" +
        "print(sum(sorted(xs, key=lambda x: -x)[:3]))\n" +
        "print(min(xs, key=lambda x: (x * 13) % 4000))\n");
    check("four thousand callbacks", r.out + r.err, "11994\n0\n");
}

// The sort is stable and the key is called once per item, in order.
{
    const r = script(
        "seen = []\n" +
        "def k(v):\n" +
        "    seen.append if False else seen\n" +
        "    return v % 3\n" +
        "print(sorted([5, 3, 9, 1, 4, 2], key=k))\n" +
        "print(sorted([5, 3, 9, 1], key=k, reverse=True))\n");
    check("key and reverse", r.out + r.err, "[3, 9, 1, 4, 5, 2]\n[5, 1, 3, 9]\n");
}

// An exception out of a key function unwinds the continuation with it, and
// the frame it was waiting on leaves nothing behind.
{
    const r = script(
        "def boom(x):\n" +
        "    raise ValueError(x)\n" +
        "try:\n" +
        "    min([5, 6], key=boom)\n" +
        "except ValueError as e:\n" +
        "    print('caught', e)\n" +
        "print(sorted([2, 1]))\n");
    check("an exception through a continuation", r.out + r.err, "caught 5\n[1, 2]\n");
}

// A key function that recurses into the same builtin: the frame limit answers,
// not the native stack.
{
    const r = script(
        "def r(n):\n" +
        "    return sorted([n], key=lambda x: r(x + 1)[0]) if n < 500 else [n]\n" +
        "try:\n" +
        "    r(0)\n" +
        "except RecursionError:\n" +
        "    print('RecursionError')\n");
    check("recursion through a continuation", r.out + r.err, "RecursionError\n");
}

// And the same with the collector running at every allocation: the
// continuation holds the values, their keys and the function across all of it.
{
    put("/tmp/c.py",
        "xs = list(range(40))\n" +
        "print(sorted(xs, key=lambda x: (x * 7) % 40)[:6])\n" +
        "print(max(xs, key=lambda x: -x), min(xs, key=lambda x: -x))\n");
    const r = run("/tmp/c.py", null, "PY_GC_STRESS=1");
    check("a continuation under gc stress", r.out + r.err,
          "[0, 23, 6, 29, 12, 35]\n0 39\n");
}

// Two ** mappings naming one parameter is a TypeError, whichever way the
// keyword arrives: DictMerge is on every path.
for (const call of ['f(1, **{"b": 2}, **{"b": 3})',
                    'f(**{"b": 2}, b=3, a=1)',
                    'f(a=1, b=2, **{"b": 3})']) {
    const r = script("def f(a, b=None):\n    print(a, b)\n" +
                     `try:\n    ${call}\nexcept TypeError as e:\n    print(e)\n`);
    check(`a duplicate keyword: ${call}`, r.out + r.err,
          "f() got multiple values for keyword argument 'b'\n");
}

// A decorator is a call on the definition, and it stacks bottom up.
{
    const r = script(
        "def tag(name):\n" +
        "    def wrap(f):\n" +
        "        def inner(*a):\n" +
        "            return name + '(' + f(*a) + ')'\n" +
        "        return inner\n" +
        "    return wrap\n" +
        "@tag('outer')\n" +
        "@tag('inner')\n" +
        "def who(x):\n" +
        "    return x\n" +
        "print(who('v'))\n");
    check("stacked decorators", r.out + r.err, "outer(inner(v))\n");
}

// A deleted cell is unbound again, and says so from either side.
{
    const r = script(
        "def f():\n" +
        "    x = 1\n" +
        "    def g():\n" +
        "        nonlocal x\n" +
        "        try:\n" +
        "            print(x)\n" +
        "        except NameError as e:\n" +
        "            print(e)\n" +
        "        try:\n" +
        "            del x\n" +
        "        except NameError as e:\n" +
        "            print(e)\n" +
        "    del x\n" +
        "    g()\n" +
        "f()\n");
    check("a deleted cell", r.out + r.err,
          "free variable 'x' referenced before assignment\n" +
          "free variable 'x' referenced before assignment\n");
}

if (bad) {
    console.error(`pyfun: ${bad} checks failed`);
    process.exit(1);
}
ok("calls, closures, decorators and the builtin-callback rule");
