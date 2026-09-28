// The archive symbol table, which ld ignores as wasm-ld does. Every fixture
// with archives links to the same bytes, loading the same members, when its
// archives have no symbol table, and when the table is stale: it names a
// member that does not define the name, or leaves a member out.

import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { basename, join } from "node:path";
import { boot, die, FLAGS, linkers, manifest, ok, plant } from "./wasmlib.mjs";

const m = manifest();
const H = await boot();
const { tmp: ldtmp, inputs, wasm_ld, ld } = linkers(H, m);

const bad = [];
const same = (what, a, b) => {
    if (a !== b)
        bad.push(`${what}:\n  got:  ${JSON.stringify(a)}\n  want: ${JSON.stringify(b)}`);
};

const tmp = mkdtempSync(join(tmpdir(), "ld-symdef-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));

// An archive's members, extracted into a directory of their own, in order.
function members(archive) {
    const dir = mkdtempSync(join(tmp, "x-"));
    execFileSync(m.ar, ["x", archive], { cwd: dir });
    const order = execFileSync(m.ar, ["t", archive]).toString().split("\n").filter(Boolean);
    return order.map((n) => ({ name: n, bytes: readFileSync(join(dir, n)), dir }));
}

// ---------------------------------------------------------------- no index

let relinked = 0;
for (const [name, fx] of Object.entries(m.fixtures)) {
    if (!fx.archives.length)
        continue;
    const args = [...FLAGS, "--trace", ...inputs(fx), "-o", "p"];
    const want = ld(args);
    const ref = H.store.files.get("/tmp/p");
    const renamed = new Map();
    for (const a of fx.archives) {
        const b = "noidx_" + basename(a);
        const ms = members(a);
        execFileSync(m.ar, ["rcS", "--format=gnu", join(tmp, b), ...ms.map((x) => x.name)],
                     { cwd: ms[0].dir });
        plant(H, `/tmp/${b}`, new Uint8Array(readFileSync(join(tmp, b))));
        renamed.set(basename(a), b);
    }
    H.store.files.delete("/tmp/p");
    const got = ld(args.map((x) => renamed.get(x) ?? x));
    same(`${name} without an index: status`, got.status, want.status);
    same(`${name} without an index: stderr`, got.err, want.err);
    same(`${name} without an index: --trace`, got.out.replaceAll("noidx_", ""), want.out);
    const out = H.store.files.get("/tmp/p");
    if (!ref || !out || !Buffer.from(out).equals(Buffer.from(ref)))
        bad.push(`${name} without an index: the output differs`);
    relinked++;
}

// ---------------------------------------------------------------- stale

// A GNU archive of `ms`, its `/` naming each [symbol, member index].
function gnu_ar(ms, symbols) {
    const header = (name, size) =>
        Buffer.from(name.padEnd(16) + "0".padEnd(12) + "0".padEnd(6) + "0".padEnd(6) +
                    "644".padEnd(8) + String(size).padEnd(10) + "`\n", "latin1");
    const pad = (b) => (b.length & 1 ? Buffer.concat([b, Buffer.from("\n")]) : b);
    const names = Buffer.concat(symbols.map(([s]) => Buffer.from(s + "\0")));
    const table = Buffer.alloc(4 + 4 * symbols.length);
    const body = pad(Buffer.concat([table, names]));
    const offs = [];
    let at = 8 + 60 + body.length;
    for (const x of ms) {
        offs.push(at);
        at += 60 + x.bytes.length + (x.bytes.length & 1);
    }
    table.writeUInt32BE(symbols.length, 0);
    symbols.forEach(([, i], k) => table.writeUInt32BE(offs[i], 4 + 4 * k));
    const symtab = pad(Buffer.concat([table, names]));
    return Buffer.concat([Buffer.from("!<arch>\n"), header("/", table.length + names.length),
        symtab, ...ms.flatMap((x) => [header(x.name + "/", x.bytes.length), pad(x.bytes)])]);
}

const fx = m.fixtures.archives;
// Names that fit a header; nothing needs one_unused.c.obj.
const one = members(fx.archives.find((a) => a.endsWith("libfxa_one.a")))
    .filter((x) => x.name.length < 16);
const idx = (n) => one.findIndex((x) => x.name === n);
const objs = inputs(fx).slice(0, fx.objects.length);
const rest = inputs(fx).slice(fx.objects.length + 1);
const args = [...FLAGS, "--trace", ...objs, "stale.a", ...rest];

H.store.files.delete("/tmp/p");
ld([...FLAGS, ...inputs(fx), "-o", "p"]);
const ref = H.store.files.get("/tmp/p");

function ignored(what, symbols) {
    const b = gnu_ar(one, symbols);
    plant(H, "/tmp/stale.a", new Uint8Array(b));
    writeFileSync(join(ldtmp, "stale.a"), b);
    H.store.files.delete("/tmp/p");
    const got = ld([...args, "-o", "p"]);
    const want = wasm_ld(args);
    same(`${what}: status`, got.status, 0);
    same(`${what}: stderr`, got.err, want.err);
    same(`${what}: --trace`, got.out, want.out);
    const out = H.store.files.get("/tmp/p");
    if (!ref || !out || !Buffer.from(out).equals(Buffer.from(ref)))
        bad.push(`${what}: the output differs`);
}

// one_a is said to be in one_c.c.obj.
ignored("a stale index", [["one_a", idx("one_c.c.obj")], ["one_c", idx("one_c.c.obj")]]);
// one_c.c.obj is left out.
ignored("a member left out", [["one_a", idx("one_a.c.obj")]]);

if (bad.length)
    die("\n" + bad.join("\n"));
ok(`${relinked} fixtures link alike without an index, and with a stale one`);
