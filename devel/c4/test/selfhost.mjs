// The famous demo: compile c4.c and run that to compile hello.c.

import { join } from "node:path";
import { boot, die, get, ok, parse_opt, plant_file, run, SHARE } from "./c4lib.mjs";

const H = await boot(parse_opt(process.argv, []));

plant_file(H, "/tmp/c.c", join(SHARE, "c4.c"));
plant_file(H, "/tmp/h.c", join(SHARE, "hello.c"));

run(H, "c4 /tmp/c.c /tmp/h.c >/tmp/g");
const out = get(H, "/tmp/g");
if (!out.startsWith("hello, world\n"))
    die("self-host did not print hello, world:\n" + JSON.stringify(out));
const rest = out.slice("hello, world\n".length);
if (!/^exit\(0\) cycle = \d+\nexit\(0\) cycle = \d+\n$/.test(rest))
    die("self-host did not print both exit lines:\n" + JSON.stringify(out));

ok("c4.c hello.c");
