// Search, regexps and replace. The regexps are what carry regex.c, all 6.6
// thousand lines of it, and re_search_2 is what reads across the gap.

import { boot, le, put, keys, press, screen, status, cursor, tick, is, ok } from "./lelib.mjs";

await boot("lesearch");
put("/tmp/f", "alpha\nbeta\ngamma\ndelta\n");
le("/tmp/f");

press("^f");
tick(1);
is("the prompt", status(), "Search forwards:");
keys("g", "a", "m");
press("ENTER");
tick(2);
is("a literal search lands", cursor(), "0,2");

// A regexp, which is the default: `.` is not a literal dot.
press("^f");
tick(1);
keys("d", ".", "l", "t", "a");
press("ENTER");
tick(2);
is("a regexp search lands", cursor(), "0,3");

// Replace, with the whole file confirmed at once. From the top: replace
// searches forward, and the regexp above left the cursor on the last line.
keys("^g", "b");
press("^r");
tick(1);
keys("e", "t");
press("ENTER");
tick(1);
keys("X", "Y");
press("ENTER");
tick(1);
press("*");
tick(3);
is("replace all", screen(4),
`alpha
bXYa
gamma
delta`);
is("and says how many", status(), "1 replacement.");

ok("search, regexps and replace");
