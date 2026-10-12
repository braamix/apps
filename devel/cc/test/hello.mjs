// hello.c, built from vcc's C by the host compiler: its module surface, then
// run from the shell, its arguments and status through a file, and its output
// on the screen.

import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const APPS = resolve(dirname(fileURLToPath(import.meta.url)), "../../..");
const BINARY = join(APPS, "build/devel/cc/hello.wasm");

function die(msg) {
    console.error("hello: " + msg);
    process.exit(1);
}

function expect(what, got, want) {
    if (got !== want)
        die(`${what}: got ${JSON.stringify(got)}, expected ${JSON.stringify(want)}`);
}

const bytes = new Uint8Array(readFileSync(BINARY));
const m = new WebAssembly.Module(bytes);
expect("imports", WebAssembly.Module.imports(m).map((i) => i.module + "." + i.name).sort().join(" "),
       "env.memory kernel.sys kernel.sys_async");
expect("exports", WebAssembly.Module.exports(m).map((e) => e.name).sort().join(" "),
       "_alloc _free _resume _sig _start");
const sections = WebAssembly.Module.customSections(m, "braam");
expect("braam sections", sections.length, 1);
const meta = new Uint32Array(sections[0]);
expect("magic", meta[0], 0x6d617262);
expect("pages", `${meta[3]} ${meta[4]}`, "4 1600");

const H = await import(HARNESS);
await H.init(KERNEL, ROOTFS);
H.kernel().init(0);
if (H.run(0) !== -1)
    die("the shell did not park on the keyboard");
H.regrid(80, 24, "resize returned no screen descriptor");
H.store.files.set("/bin/hello", bytes);

let now = 1;
const text = (path) => new TextDecoder().decode(H.store.files.get(path) ?? new Uint8Array());

function submit(line) {
    if (line.length > 60)
        die(`the command line is ${line.length} keys, and the ring holds 64`);
    H.submit(line, now++);
    if (H.run(now++) !== -1)
        die("the kernel did not settle after: " + line);
}

submit("hello one two >/tmp/o; echo $? >/tmp/s");
expect("output", text("/tmp/o"), "hello from hello: [one] [two]\n");
expect("status", text("/tmp/s"), "3\n");

submit("hello");
if (!H.rows(H.screen()).includes("hello from hello:"))
    die("the screen has no greeting:\n" + H.rows(H.screen()).join("\n"));

console.log("hello ok");
