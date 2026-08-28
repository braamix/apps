// The two places LE runs a command: the shell escape and the block filter.
//
// Upstream forked for both. Here spawn() takes the descriptors as an argument,
// and the filter's two pipes are temp files -- one task cannot park on both
// ends of a pipeline, so a filter driven through pipes would deadlock the
// moment either filled.

import { boot, le, put, press, keys, screen, status, tick, is, ok, H } from "./lelib.mjs";

await boot("lespawn");
put("/tmp/f", "beta\nalpha\ngamma\n");
le("/tmp/f");

// A filter: mark the first two lines and pass them through cat.
press("F5");
keys("DOWN", "DOWN");
press("F6");
press("F4");
press("|");
tick(2);
is("the prompt", status(), "Pipe through:");
keys("c", "a", "t");
press("ENTER");
tick(10);
is("cat gives the block back unchanged", screen(3),
`beta
alpha
gamma`);

// What the child says on stderr goes in the box upstream showed it in: the
// editor holds the screen, so it cannot go to the console.
keys("^g", "b");
press("F5");
keys("DOWN");
press("F6");
press("F4");
press("|");
tick(2);
for (const c of "nosuchcmd")
    press(c);
press("ENTER");
tick(8);
is("a failed filter reports what the shell said",
   H.rows(H.screen()).some((r) => r.includes("nosuchcmd: not found")) ? "yes" : "no", "yes");
press("ENTER");
tick(2);

// The shell escape: the screen goes back, the command runs on the console,
// and a key brings the editor back.
press("F9");
tick(10);
is("the console has the output and the prompt",
   H.rows(H.screen()).some((r) => r.includes("[Press any key to continue]")) ? "yes" : "no", "yes");
press("x");
tick(4);
is("and a key returns to the editor", screen(1), "beta");

ok("a block filter and a shell escape");
