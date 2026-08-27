// The regular expression engine and the two commands built on it, :g and :s.
//
// ex_re.cpp is the piece of the port that changed least -- it is pure
// computation with no system call anywhere in it -- so this is as much a check
// that nothing was disturbed in the K&R conversion as that it works.

import { boot, ex, put, is, ok } from "./exlib.mjs";

await boot("exregex");

const FILE = "alpha\nbeta\ngamma\nab12cd\nAlpha\n";
const HEAD = `"/tmp/f" 5 lines, 30 characters`;

function run(what, script, want) {
    put("/tmp/f", FILE);
    is(what, ex("/tmp/f", script), HEAD + (want ? "\n" + want : ""));
}

// ^ and $ anchor; . is any character; * is zero or more of what precedes it,
// so b*a matches a bare a and every line here has one.
run("anchors", "g/^a/p\nq\n", "alpha\nab12cd");
run("dollar", "g/a$/p\nq\n", "alpha\nbeta\ngamma\nAlpha");
run("dot", "g/g.mma/p\nq\n", "gamma");
run("star", "g/b*a/p\nq\n", "alpha\nbeta\ngamma\nab12cd\nAlpha");

// Character classes, including a range and a negated one.
run("class", "g/[0-9][0-9]/p\nq\n", "ab12cd");
run("negated class", "g/[^a-z]/p\nq\n", "ab12cd\nAlpha");

// The word boundaries ex took from Toronto ed.
run("word start", "g/\\<be/p\nq\n", "beta");
run("word end", "g/al\\>/p\nq\n", "");

// Substitute: two groups put back in the other order, the whole match with &,
// and g for every occurrence on the line.
run("groups", "1s/\\(al\\)\\(pha\\)/\\2\\1/\n1p\nq!\n", "phaal\nphaal");
run("ampersand", "2s/e/&&/\n2p\nq!\n", "beeta\nbeeta");
run("global flag", "3s/m/M/g\n3p\nq!\n", "gaMMa\ngaMMa");

// The two options that change what a pattern means. Under nomagic a dot is
// itself and a backslash restores it.
run("ignorecase", "set ic\ng/^alpha$/p\nq\n", "alpha\nAlpha");
run("nomagic", "set nomagic\ng/g.mma/p\ng/g\\.mma/p\nq\n", "gamma");

// :v is :g inverted, and every line here holds an a.
run("v", "v/a/p\nq\n", "");

// A pattern that matches nothing says so and leaves the buffer alone.
run("no match", "1,$s/zzz/x/\n%p\nq\n",
    "Substitute pattern match failed\nalpha\nbeta\ngamma\nab12cd\nAlpha");

ok("BRE constructs, :s with groups, :g, :v, ignorecase and nomagic");
