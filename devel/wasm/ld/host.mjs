// ld on the host: `node host.mjs <ld.wasm> <ld's arguments>...`, which is
// what `clang --ld-path` runs under `make LINKER=ld`. The kernel's headers are
// wasm32's alone, so there is no native ld. This boots the SDK's harness,
// plants ld.wasm and every input into its /tmp, runs the link there, and
// copies the output back. Its stdout, stderr and status are ld's.

import { readFileSync, writeFileSync, existsSync } from "node:fs";
import { basename, join, resolve } from "node:path";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

// Flags whose value may be the next word.
const VALUED = new Set(["-o", "-L", "-l", "-e", "-u", "-m", "-z", "--output", "--entry",
                        "--export", "--undefined", "--why-extract", "--error-limit",
                        "--initial-memory", "--max-memory", "--global-base", "--braam-abi",
                        "--braam-pages"]);

function fail(msg) {
    process.stderr.write(`ld: error: ${msg}\n`);
    process.exit(1);
}

// @file expanded here, so every input is seen.
function expand(args) {
    const out = [];
    for (const a of args) {
        if (!a.startsWith("@")) {
            out.push(a);
            continue;
        }
        let text;
        try {
            text = readFileSync(a.slice(1), "utf8");
        } catch (e) {
            fail(`cannot open ${a.slice(1)}: ${e.code}`);
        }
        out.push(...text.split(/\s+/).filter((w) => w));
    }
    return out;
}

const [wasm, ...rest] = process.argv.slice(2);
if (!wasm)
    fail("usage: host.mjs <ld.wasm> <arguments>...");
const words = expand(rest);

// Every file is planted flat in /tmp, under its base name if that is free.
const planted = new Map(); // host path -> store path
const taken = new Set(["/tmp/rsp", "/tmp/o", "/tmp/e", "/tmp/s", "/tmp/why"]);
function place(path) {
    const host = resolve(path);
    if (planted.has(host))
        return planted.get(host);
    let at = `/tmp/${basename(host)}`;
    for (let k = 1; taken.has(at); k++)
        at = `/tmp/${k}.${basename(host)}`;
    taken.add(at);
    planted.set(host, at);
    return at;
}

const dirs = [];
let output = "a.out", why = null;
const args = [];
for (let i = 0; i < words.length; i++) {
    let a = words[i], v = null;
    if (VALUED.has(a) && i + 1 < words.length) {
        v = words[++i];
    } else if (a.startsWith("--") && a.includes("=") && VALUED.has(a.slice(0, a.indexOf("=")))) {
        v = a.slice(a.indexOf("=") + 1);
        a = a.slice(0, a.indexOf("="));
    } else if (/^-[oLl]./.test(a)) {
        v = a.slice(2);
        a = a.slice(0, 2);
    }
    if (a === "-o" || a === "--output") {
        output = v;
    } else if (a === "-L") {
        dirs.push(v);
    } else if (a === "-l") {
        // Searched here, where the directories are.
        const found = dirs.map((d) => join(d, `lib${v}.a`)).find((p) => existsSync(p));
        args.push(found ? place(found) : `-l${v}`);
    } else if (a === "--why-extract") {
        why = v;
        args.push(v === "-" ? "--why-extract=-" : "--why-extract=/tmp/why");
    } else if (v !== null && a.startsWith("--")) {
        args.push(`${a}=${v}`);
    } else if (v !== null) {
        args.push(a, v);
    } else if (!a.startsWith("-") && a !== "") {
        if (!existsSync(a))
            fail(`cannot open ${a}: ENOENT`);
        args.push(place(a));
    } else {
        args.push(a);
    }
}
// The module is named after the output, so it keeps its base name.
const out = `/tmp/${basename(output)}`;
if (taken.has(out))
    fail(`an input is named as the output is: ${basename(output)}`);
args.push("-o", out);

const H = await import(HARNESS);
await H.init(KERNEL, ROOTFS);
H.kernel().init(0);
let now = 1;
if (H.run(0) !== -1)
    fail("the harness's shell did not start");
H.regrid(80, 24, "resize returned no screen descriptor");
const put = (at, bytes) => H.store.files.set(at, bytes);
put("/bin/ld", new Uint8Array(readFileSync(wasm)));
for (const [host, at] of planted)
    put(at, new Uint8Array(readFileSync(host)));
put("/tmp/rsp", new TextEncoder().encode(args.join("\n") + "\n"));
H.submit("cd /tmp; ld @rsp >o 2>e; echo $? >s", now++);
if (H.run(now++) !== -1)
    fail("the harness did not settle");

const got = (at) => H.store.files.get(at);
const text = (at) => new TextDecoder().decode(got(at) ?? new Uint8Array());
process.stdout.write(text("/tmp/o"));
process.stderr.write(text("/tmp/e"));
if (why && why !== "-" && got("/tmp/why"))
    writeFileSync(why, got("/tmp/why"));
const status = Number(text("/tmp/s"));
if (status === 0) {
    if (!got(out))
        fail("the link left no output");
    writeFileSync(output, got(out));
}
process.exit(Number.isInteger(status) && text("/tmp/s") ? status : 1);
