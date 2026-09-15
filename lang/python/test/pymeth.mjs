// Methods on the built-in types: what upstream's own tests do not reach.
//
// Three things are asked here and nowhere else. A method reached through a
// subclass acts on the built-in inside the instance, which is a different path
// from `[].append`. `list.sort(key=)` is a continuation, so it must survive
// depth and an exception thrown out of the key. And every native here
// allocates, so the whole surface is run again under PY_GC_STRESS.

import { boot, script, run, put, ok, die, same } from "./pylib.mjs";

await boot("pymeth");

let bad = 0;
function check(what, got, want) {
    if (!same(what, got, want)) bad++;
}

// A method through a subclass of a built-in. self is the instance, and the
// native has to reach the list, str or dict inside it.
{
    const r = script(
        "class L(list):\n    pass\n" +
        "class S(str):\n    pass\n" +
        "class D(dict):\n    pass\n" +
        "x = L([3, 1, 2])\n" +
        "x.append(4)\n" +
        "x.sort()\n" +
        "print(x, x.count(1), x.index(3))\n" +
        "print(S('abC').upper(), S('a,b').split(','), S('ab').startswith('a'))\n" +
        "d = D()\n" +
        "d['k'] = 1\n" +
        "print(list(d.keys()), d.get('k'), d.pop('k', 9), d.get('k', 9))\n");
    check("a method through a subclass", r.out + r.err,
          "[1, 2, 3, 4] 1 2\nABC ['a', 'b'] True\n['k'] 1 1 9\n");
}

// The unbound form, which is the same native with self spelled out.
{
    const r = script(
        "print(str.upper('qq'), list.count([1, 1], 1), dict.get({'a': 2}, 'a'))\n" +
        "f = 'abc'.upper\n" +
        "print(f(), f())\n");
    check("a method reached through its type", r.out + r.err, "QQ 2 2\nABC ABC\n");
}

// sort(key=) is a continuation: the key is Python and the VM drives it. A
// thousand calls must not grow the native stack. That is ground rule 2.
{
    const r = script(
        "l = list(range(1000))\n" +
        "l.sort(key=lambda x: -x)\n" +
        "print(l[0], l[-1], len(l))\n" +
        "w = ['bb', 'a', 'ccc', 'dd']\n" +
        "w.sort(key=len)\n" +
        "print(w)\n" +
        "w.sort(key=len, reverse=True)\n" +
        "print(w)\n");
    check("a thousand key calls", r.out + r.err,
          "999 0 1000\n['a', 'bb', 'dd', 'ccc']\n['ccc', 'bb', 'dd', 'a']\n");
}

// An exception out of the key unwinds through the continuation. The list is
// left as it was, not half sorted.
{
    const r = script(
        "l = [3, 1, 2]\n" +
        "def boom(x):\n" +
        "    if x == 1:\n" +
        "        raise ValueError('no')\n" +
        "    return x\n" +
        "try:\n" +
        "    l.sort(key=boom)\n" +
        "except ValueError as e:\n" +
        "    print('caught', e)\n" +
        "print(l)\n");
    check("an exception out of a key", r.out + r.err, "caught no\n[3, 1, 2]\n");
}

// map and filter are eager here; README says so. They are still their own
// once-only iterables, not lists.
{
    const r = script(
        "m = map(lambda x: x * 2, [1, 2, 3])\n" +
        "print(type(m).__name__, list(m), list(m))\n" +
        "print(list(map(lambda a, b: a + b, [1, 2, 3], [10, 20])))\n" +
        "f = filter(lambda x: x > 1, [1, 2, 3])\n" +
        "print(type(f).__name__, list(f))\n" +
        "print(list(filter(None, [0, 1, '', 2, None])))\n" +
        "print(list(zip([1, 2, 3], 'ab')), list(zip()))\n" +
        "print(list(reversed([1, 2, 3])), list(reversed(range(3))))\n");
    check("map, filter, zip and reversed", r.out + r.err,
          "map [2, 4, 6] []\n[11, 22]\nfilter [2, 3]\n[1, 2]\n" +
          "[(1, 'a'), (2, 'b')] []\n[3, 2, 1] [2, 1, 0]\n");
}

// The dict views are their own types. They track the dict and do set
// arithmetic. A values view is hashable and the other two are not, as in
// CPython, because only those two define __eq__.
{
    const r = script(
        "d = {'a': 1, 'b': 2}\n" +
        "k = d.keys()\n" +
        "print(k, d.values(), d.items())\n" +
        "d['c'] = 3\n" +
        "print(len(k), list(k))\n" +
        "print(k | {'z'}, k & {'a'}, k - {'a', 'b'}, k ^ {'a', 'z'})\n" +
        "print(k == {'a', 'b', 'c'}, k >= {'a'}, k.isdisjoint({'q'}))\n" +
        "print(type(hash(d.values())).__name__)\n" +
        "for name in ('keys', 'items'):\n" +
        "    try:\n" +
        "        hash(getattr(d, name)())\n" +
        "    except TypeError:\n" +
        "        print('TypeError', name)\n");
    check("the dict views", r.out + r.err,
          "dict_keys(['a', 'b']) dict_values([1, 2]) dict_items([('a', 1), ('b', 2)])\n" +
          "3 ['a', 'b', 'c']\n" +
          "{'a', 'b', 'c', 'z'} {'a'} {'c'} {'b', 'c', 'z'}\n" +
          "True True True\nint\nTypeError keys\nTypeError items\n");
}

// frozenset is hashable and set is not, and an operator takes its type from
// the left operand.
{
    const r = script(
        "f = frozenset([1, 2])\n" +
        "print(f, f | {3}, sorted({3} | f), f & {2})\n" +
        "print(hash(f) == hash(frozenset([2, 1])), {f: 'x'}[frozenset([2, 1])])\n" +
        "print(f == {1, 2}, {1, 2} == f, f <= {1, 2, 3})\n" +
        "try:\n" +
        "    {set(): 1}\n" +
        "except TypeError:\n" +
        "    print('TypeError')\n" +
        "s1 = s2 = {1}\n" +
        "s1 |= {2}\n" +
        "print(s1 is s2, sorted(s1))\n");
    check("frozenset and the set operators", r.out + r.err,
          "frozenset({1, 2}) frozenset({1, 2, 3}) [1, 2, 3] frozenset({2})\n" +
          "True x\nTrue True True\nTypeError\nTrue [1, 2]\n");
}

// bytearray is bytes that can be written. memoryview is a window on one.
{
    const r = script(
        "a = bytearray(b'abc')\n" +
        "a.append(100)\n" +
        "a.extend(b'ef')\n" +
        "a[0] = 65\n" +
        "print(a, a.upper(), a.hex(), a == b'Abcdef')\n" +
        "a[1:3] = b'ZZZ'\n" +
        "print(a, a.pop(), len(a))\n" +
        "m = memoryview(a)\n" +
        "a[0] = 66\n" +
        "print(m[0], bytes(m[1:3]), m.tolist()[:3])\n" +
        "m[0] = 67\n" +
        "print(a[0], bytes(memoryview(b'xy')))\n" +
        "try:\n" +
        "    memoryview(b'xy')[0] = 1\n" +
        "except TypeError:\n" +
        "    print('TypeError')\n");
    check("bytearray and memoryview", r.out + r.err,
          "bytearray(b'Abcdef') bytearray(b'ABCDEF') 416263646566 True\n" +
          "bytearray(b'AZZZde') 102 6\n" +
          "66 b'ZZ' [66, 90, 90]\n" +
          "67 b'xy'\nTypeError\n");
}

// The str methods that have to count codepoints rather than bytes.
{
    const r = script(
        "s = 'héllo wörld'\n" +
        "print(len(s), s.upper(), s.find('l'), s.rfind('l'), s.index('w'))\n" +
        "print(s.split(), s[1:4], s.center(15, '-'), s.count('l'))\n" +
        "print(s.replace('ö', 'o'), s.title(), s.capitalize())\n" +
        // Not 'ß': the case table has no one-codepoint upper for it, so it is
        // not a letter here. Phase 22 replaces the ranges with the categories.
        "print('é'.isalpha(), '123'.isdigit(), 'ЖЖ'.lower(), 'жж'.upper())\n" +
        "print(s.encode(), s.encode().decode() == s, len(s.encode()))\n");
    check("str methods over codepoints", r.out + r.err,
          "11 HÉLLO WÖRLD 2 9 6\n" +
          "['héllo', 'wörld'] éll --héllo wörld-- 3\n" +
          "héllo world Héllo Wörld Héllo wörld\n" +
          "True True жж ЖЖ\n" +
          "b'h\\xc3\\xa9llo w\\xc3\\xb6rld' True 13\n");
}

// int and float, and the three builtins that answer a shape rather than a
// number.
{
    const r = script(
        "print((255).to_bytes(2, 'big'), int.from_bytes(b'\\x01\\x00', 'big'))\n" +
        "print((-2).to_bytes(4, 'little', signed=True), (5).bit_length())\n" +
        "print((2.5).is_integer(), (2.0).is_integer(), (3.14).hex())\n" +
        "print(float.fromhex('0x1.8p+1'), (0.5).as_integer_ratio())\n" +
        "print(divmod(7, 2), divmod(-7, 2), round(2.675, 2), round(12345, -2))\n" +
        "print(pow(2, 10, 1000), pow(1, 0, 1), sum([1, 2], 10))\n" +
        "print(slice(1, 5, 2).indices(10), slice(1, 2, 3).step, range(1, 9, 2).start)\n");
    check("int, float and the shaped builtins", r.out + r.err,
          "b'\\x00\\xff' 256\n" +
          "b'\\xfe\\xff\\xff\\xff' 3\n" +
          "False True 0x1.91eb851eb851fp+1\n" +
          "3.0 (1, 2)\n" +
          "(3, 1) (-4, 1) 2.67 12300\n" +
          "24 0 13\n" +
          "(1, 5, 2) 3 1\n");
}

// Every method allocates, and most hold a value across the allocation that
// the collector cannot see. Under PY_GC_STRESS every allocation collects, so a
// missing Root gives a wrong answer here instead of a rare crash later.
{
    const src =
        "s = 'a,b,c'\n" +
        "print(s.split(','), '-'.join(s.split(',')), s.replace(',', ';'))\n" +
        "print(s.partition(','), s.upper(), s.strip('ac'), s.center(9, '.'))\n" +
        "l = [3, 1, 2]\n" +
        "l.sort()\n" +
        "l.extend([4])\n" +
        "print(l, l.copy(), l.pop())\n" +
        "d = {'a': 1}\n" +
        "d.update(b=2)\n" +
        "print(sorted(d.items()), d.copy(), dict.fromkeys('xy', 0))\n" +
        "print({1, 2} | {3}, frozenset('ab') - frozenset('a'))\n" +
        "print(bytearray(b'ab').upper(), b'ab'.hex(), (7).to_bytes(2, 'big'))\n" +
        "print(list(map(str, [1, 2])), list(filter(None, [0, 1])))\n";
    put("/tmp/c.py", src);
    const plain = run("/tmp/c.py");
    const stressed = run("/tmp/c.py", null, "PY_GC_STRESS=1");
    check("the same answers under gc stress", stressed.out + stressed.err,
          plain.out + plain.err);
    if (plain.status !== 0) {
        console.error(plain.err);
        bad++;
    }
}

if (bad) die(`${bad} checks failed`);
ok("methods on the built-in types, and the collector under them");
