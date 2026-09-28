// Step 1: reading. `wlink --dump` on every fixture object and archive, every
// member of every SDK archive, and archives in the formats the SDK does not
// use, must print what `llvm-objdump -t -r` prints for them, byte for byte.
// Then inputs wlink must refuse, and the message it refuses them with.

import { execFileSync } from "node:child_process";
import { mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync, copyFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { basename, dirname, join } from "node:path";
import { boot, die, get, manifest, ok, plant, run, sections } from "./wlinklib.mjs";

const m = manifest();

// Every input once, by absolute path.
const inputs = new Set();
for (const fx of Object.values(m.fixtures))
    for (const p of [...fx.objects, ...fx.archives])
        inputs.add(p);
for (const f of readdirSync(m.sdk_libs).sort())
    if (/^libbraam_.*\.a$/.test(f))
        inputs.add(join(m.sdk_libs, f));

// GNU long names and BSD names, which llvm-ar writes on request.
const tmp = mkdtempSync(join(tmpdir(), "wlink-dump-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));
const some = m.fixtures.data.objects;
copyFileSync(some[0], join(tmp, "a_member_name_longer_than_sixteen.o"));
copyFileSync(some[1], join(tmp, "short.o"));
for (const format of ["gnu", "bsd"]) {
    const ar = join(tmp, `fmt_${format}.a`);
    execFileSync(m.ar, ["rc", `--format=${format}`, ar,
        "a_member_name_longer_than_sixteen.o", "short.o"], { cwd: tmp });
    inputs.add(ar);
}

const names = new Map();
for (const p of inputs) {
    const b = basename(p);
    if (names.has(b))
        die(`two inputs are named ${b}: ${names.get(b)} and ${p}`);
    names.set(b, p);
}

const objdump = (p) =>
    execFileSync(m.objdump, ["-t", "-r", basename(p)], { cwd: dirname(p), maxBuffer: 1 << 28 })
        .toString("utf8");
const want = [...inputs].map(objdump).join("");

const H = await boot();
plant(H, "/bin/wlink", new Uint8Array(readFileSync(m.wlink)));

function wlink(args) {
    for (const k of ["/tmp/o", "/tmp/e", "/tmp/s"])
        H.store.files.delete(k);
    run(H, `cd /tmp; wlink --dump ${args} >o 2>e; echo $? >s`);
    return { out: get(H, "/tmp/o") ?? "", err: get(H, "/tmp/e") ?? "",
             status: Number(get(H, "/tmp/s")) };
}

for (const [b, p] of names)
    plant(H, `/tmp/${b}`, new Uint8Array(readFileSync(p)));
plant(H, "/tmp/list", [...names.keys()].join("\n") + "\n");

const got = wlink("@list");
if (got.status !== 0)
    die(`--dump exited ${got.status}: ${got.err}`);
if (got.out !== want) {
    const a = got.out.split("\n"), b = want.split("\n");
    let i = 0;
    while (i < a.length && a[i] === b[i])
        i++;
    let file = "";
    for (let j = i; j >= 0 && !file; j--)
        if (/\tfile format wasm$/.test(b[j] ?? ""))
            file = b[j];
    die(`--dump differs from llvm-objdump at line ${i + 1}, in ${file}\n` +
        `  wlink:   ${JSON.stringify(a[i])}\n  objdump: ${JSON.stringify(b[i])}`);
}
const members = want.split("\n").filter((l) => /\tfile format wasm$/.test(l)).length;

// ---------------------------------------------------------------- refusals

const hello = () => new Uint8Array(readFileSync(m.fixtures.hello.objects[0]));

function section(bytes, name) {
    const s = sections(bytes).find((x) => x.id === 0 && x.name === name);
    if (!s)
        die(`the hello object has no ${name} section`);
    return s.body.byteOffset;
}

const bad = [];

function refuse(what, bytes, message) {
    plant(H, "/tmp/bad.o", bytes);
    const r = wlink("bad.o");
    if (r.status !== 1)
        bad.push(`${what}: exit status ${r.status}`);
    if (r.err !== message + "\n")
        bad.push(`${what}: said ${JSON.stringify(r.err)}\n  expected ${JSON.stringify(message)}`);
}

refuse("bitcode", new Uint8Array([0x42, 0x43, 0xc0, 0xde, 0x35, 0x14, 0, 0]),
    "wlink: error: bad.o: LLVM bitcode (from -flto); wlink links wasm objects only");

refuse("a linked module", new Uint8Array(readFileSync(m.fixtures.hello.reference)),
    "wlink: error: bad.o: no linking section, so not a relocatable object (clang -c makes one)");

{
    const b = hello();
    b[section(b, "linking")] = 1;
    refuse("linking version 1", b,
        "wlink: error: bad.o: linking +0x0: version 1; wlink reads version 2");
}
{
    // The first entry's type byte: after the target index and the count.
    const b = hello();
    const at = section(b, "reloc.CODE") + 2;
    b[at] = 99;
    refuse("an unknown relocation", b,
        "wlink: error: bad.o: reloc.CODE +0x2: unknown relocation type 99");
    b[at] = 11;
    refuse("a PIC relocation", b,
        "wlink: error: bad.o: reloc.CODE +0x2: R_WASM_MEMORY_ADDR_REL_SLEB: " +
        "position-independent code (-fPIC) is not linked here");
}
{
    const b = hello();
    // Where the last section's header starts.
    let head = 8;
    for (let at = 8; at < b.length;) {
        head = at++;
        let size = 0, shift = 0, x;
        do {
            x = b[at++];
            size += (x & 0x7f) * 2 ** shift;
            shift += 7;
        } while (x & 0x80);
        at += size;
    }
    refuse("a truncated object", b.subarray(0, b.length - 3),
        `wlink: error: bad.o: section at file offset 0x${head.toString(16)}: ` +
        "runs past the end of the file");
}
{
    const b = new TextEncoder().encode("!<arch>\nshort.o/        0           0     0     644     99        `\n");
    refuse("a truncated archive", b,
        "wlink: error: bad.o: member at 0x8: member runs past the end of the archive");
}

if (bad.length)
    die("refusals:\n" + bad.join("\n"));
ok(`--dump matches llvm-objdump over ${inputs.size} files, ${members} objects; ` +
   "7 refusals");
