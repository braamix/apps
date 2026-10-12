// The C library on Braam. test/libc.c's output must be the host's byte for byte,
// the same source built by the host's compiler against its own C library; and
// test/sys.c, the half that asks Braam, must print what it should.

import { execFileSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const APPS = resolve(dirname(fileURLToPath(import.meta.url)), "../../..");
const BUILD = join(APPS, "build");

function die(msg) {
    console.error("libc: " + msg);
    process.exit(1);
}

const H = await import(HARNESS);
await H.init(KERNEL, ROOTFS);
H.kernel().init(0);
if (H.run(0) !== -1)
    die("the shell did not park on the keyboard");
H.regrid(80, 24, "resize returned no screen descriptor");
H.store.files.set("/bin/libctest", new Uint8Array(readFileSync(join(BUILD, "devel/cc/libctest.wasm"))));
H.store.files.set("/bin/sys", new Uint8Array(readFileSync(join(BUILD, "devel/cc/systest.wasm"))));

let now = 1;
const text = (path) => new TextDecoder().decode(H.store.files.get(path) ?? new Uint8Array());

function submit(line) {
    if (line.length > 60)
        die(`the command line is ${line.length} keys, and the ring holds 64`);
    H.submit(line, now++);
    if (H.run(now++) !== -1)
        die("the kernel did not settle after: " + line);
}

// The pure half, against the host.
const want = execFileSync(join(BUILD, "host-cc/libctest"), { encoding: "utf8" });
submit("libctest >/tmp/o 2>/tmp/e; echo $? >/tmp/s");
const got = text("/tmp/o");
if (text("/tmp/s") !== "0\n" || text("/tmp/e") !== "")
    die(`libctest: status ${text("/tmp/s").trim()}, stderr ${JSON.stringify(text("/tmp/e"))}`);
if (got !== want) {
    const a = want.split("\n"), b = got.split("\n");
    const k = a.findIndex((line, i) => line !== b[i]);
    die(`line ${k + 1} differs from the host's:\n  host:  ${a[k]}\n  braam: ${b[k]}`);
}
const lines = want.split("\n").length - 1;

// The half that asks Braam, through a redirection and at the terminal.
const SYS = `env /home x 1
isatty 1 0 0 1
time 1 1
localtime 1
access 0 -1
realpath /bin/sh /home /tmp/d /tmp / - /tmp d/up/d -1
mkstemps 1 1 /tmp/sys .c 1
mkstemp bad -1 1
child sys 7 x
child sys 0 given
spawn 0 1 1 7
spawnp 0 1 0
spawn missing 1
waitpid nothing -1
`;
submit("mkdir /tmp/d; ln -s .. /tmp/d/up; ln -s d/up/d /tmp/l");
submit("SYS_TEST=x sys >/tmp/o 2>/tmp/e; echo $? >/tmp/s");
if (text("/tmp/o") !== SYS || text("/tmp/s") !== "6\n" || text("/tmp/e") !== "")
    die(`sys:\n${text("/tmp/o")}stderr ${JSON.stringify(text("/tmp/e"))}, status ${text("/tmp/s")}`);
submit("sys");
if (!H.rows(H.screen()).includes("isatty 1 1 0 1"))
    die("sys at the terminal:\n" + H.rows(H.screen()).join("\n"));

console.log(`libc ok: ${lines} lines as the host's, and sys`);
