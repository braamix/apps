// A guest while(1) cut short. The loop never parks, so the burst's zero-length
// sleep is the only place a signal can be collected.
//
// `kill -INT` rather than a typed ^C: run() pumps the kernel until it is idle,
// and a foreground compute loop never is, so there is no window to type in. A
// background job with a second command line queued behind it gives the shell
// its turn between two bursts.

import { boot, die, ok, parse_opt, plant } from "./c4lib.mjs";

const H = await boot(parse_opt(process.argv, []));

plant(H, "/tmp/s.c", "int main() { while (1) ; }\n");

for (const line of ["c4 /tmp/s.c &", "kill -INT %1"]) {
    if (line.length > 60)
        die(`the command line is ${line.length} keys, and the ring holds 64`);
    H.type(line);
    H.press(H.KEY.ENTER);
}

if (H.run(100) !== -1)
    die("the program left the kernel with work to do");

const s = H.screen();
const flat = H.rows(s).join(" ").replace(/\s+/g, " ");
const shown = () => JSON.stringify(H.rows(s).filter((l) => l.trim()));

if (!flat.includes("interrupt c4"))
    die("the job did not end as an interrupt: " + shown());

ok("interrupted a guest loop");
