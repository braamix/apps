// hello.c, a bit of arithmetic, -s, usage, a missing file, a syntax error.

import { join } from "node:path";
import { boot, die, get, ok, parse_opt, plant, plant_file, run, SHARE } from "./c4lib.mjs";

const H = await boot(parse_opt(process.argv, []));

plant_file(H, "/tmp/h.c", join(SHARE, "hello.c"));
plant(H, "/tmp/add.c", `#include <stdio.h>
int main()
{
  printf("%d\\n", 2 + 2 * 3);
  return 0;
}
`);
plant(H, "/tmp/bad.c", "int main() { 1 }\n");

run(H, "c4 /tmp/h.c >/tmp/g");
const hello = get(H, "/tmp/g");
if (!hello.startsWith("hello, world\n"))
    die("hello.c did not print hello, world:\n" + JSON.stringify(hello));
const hexit = /^exit\(0\) cycle = (\d+)\n$/.exec(hello.slice("hello, world\n".length));
if (!hexit)
    die("hello.c did not exit 0 with a cycle count:\n" + JSON.stringify(hello));
if (Number(hexit[1]) <= 0)
    die("hello.c cycle count was not positive");

run(H, "c4 /tmp/add.c >/tmp/g");
const add = get(H, "/tmp/g");
if (!add.startsWith("8\n"))
    die("2+2*3 did not print 8:\n" + JSON.stringify(add));
if (!/^exit\(0\) cycle = \d+\n$/.test(add.slice(2)))
    die("add.c did not exit 0:\n" + JSON.stringify(add));

run(H, "c4 -s /tmp/h.c >/tmp/g");
const dump = get(H, "/tmp/g");
for (const op of ["ENT ", "IMM ", "PSH ", "PRTF", "ADJ ", "LEV "])
    if (!dump.includes(op))
        die("-s dump is missing " + JSON.stringify(op) + ":\n" + dump);
if (dump.includes("hello, world\nexit("))
    die("-s ran the program:\n" + dump);
if (!dump.includes("printf"))
    die("-s dump has no source:\n" + dump);

run(H, "c4 >/tmp/g");
const usage = get(H, "/tmp/g");
if (usage !== "usage: c4 [-s] [-d] file ...\n")
    die("usage was not upstream's:\n" + JSON.stringify(usage));

run(H, "c4 /tmp/no-c4.c >/tmp/g");
const miss = get(H, "/tmp/g");
if (miss !== "could not open(/tmp/no-c4.c)\n")
    die("missing file was not reported:\n" + JSON.stringify(miss));

run(H, "c4 /tmp/bad.c >/tmp/g");
const bad = get(H, "/tmp/g");
if (!bad.includes("semicolon expected"))
    die("a missing semicolon was not reported:\n" + JSON.stringify(bad));

ok("hello, add, -s, usage, errors");
