// $0..$9 in a replacement, which upstream's own suite never reaches: make
// expands a bare `$n` in a recipe to nothing, so every test that meant to type
// one typed an empty string instead. replace1 reads `/butt/$0hole` in the
// Makefile and runs `/butt/hole`.
//
// replace_match() (eh.cpp) is upstream's and unchanged, so this is here to
// prove the port did not break something no golden covers.

import { boot, put, rm, keys, tick, submit, chdir, is, ok, H } from "./ehlib.mjs";

await boot("ehreplace");
submit("cd /tmp");
tick(2);
chdir("/tmp");

const dec = new TextDecoder();
const file = () => {
    const b = H.store.files.get("/tmp/a.txt");
    return b === undefined ? null : dec.decode(b);
};

// Each: the buffer, the keystrokes, and what the file must hold afterwards.
const CASES = [
    ["$0 is the whole match", "qwerty butt\n", "/butt/$0hole\nW\nQ", "qwerty butthole\n"],
    ["$1 is the first group", "alpha beta\n", "/(al)(pha)/$2$1\nW\nQ", "phaal beta\n"],
    ["a group that did not take part is empty", "xy\n", "/(a)?x/[$1]\nW\nQ", "[]y\n"],
    ["$0 twice", "ab\n", "/ab/$0$0\nW\nQ", "abab\n"],
    // [0-9], not \\d: POSIX ERE has no \\d, and eh's own search() would unescape
    // it to a literal d before the pattern ever reached regcomp().
    ["text around the groups", "2026-09-12\n",
     "/([0-9]+)-([0-9]+)-([0-9]+)/$3.$2.$1\nW\nQ", "12.09.2026\n"],
];

for (const [what, before, script, after] of CASES) {
    rm("/tmp/a.txt");
    put("/tmp/a.txt", before);
    submit("eh a.txt");
    tick(3);
    keys(script);
    tick(3);
    is(what, file(), after);
}

ok(`${CASES.length} replacements with $n backreferences`);
