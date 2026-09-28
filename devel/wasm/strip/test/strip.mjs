// strip, held to llvm-strip --keep-section=braam byte for byte: every
// fixture's objects and reference program, and ld.wasm, in each mode below.
// Then what the bytes mean: a stripped program still runs, ld links
// -g-stripped objects as wasm-ld does, stripping in place is -o's result and
// leaves nothing behind, a second strip changes nothing; and the errors.

import { spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { basename, dirname, join } from "node:path";
import { boot, check_run, FLAGS, get, linkers, manifest, plant, run, sections }
    from "../../ld/test/wasmlib.mjs";

function die(msg) {
    console.error("strip: " + msg);
    process.exit(1);
}

const m = manifest();
const STRIP = join(dirname(m.ld), "../strip/strip.wasm");
const LLVM_STRIP = join(dirname(m.objdump), "llvm-strip");
const tmp = mkdtempSync(join(tmpdir(), "strip-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));

const H = await boot();
plant(H, "/bin/strip", new Uint8Array(readFileSync(STRIP)));
const bad = [];
const same = (a, b) => a && b && Buffer.compare(Buffer.from(a), Buffer.from(b)) === 0;

// A command line of any length, through a script; stdout, stderr, status.
function sh(cmd) {
    for (const k of ["/tmp/o", "/tmp/e", "/tmp/s"])
        H.store.files.delete(k);
    plant(H, "/tmp/c", `${cmd} >/tmp/o 2>/tmp/e; echo $? >/tmp/s\n`);
    run(H, "sh /tmp/c");
    return { out: get(H, "/tmp/o") ?? "", err: get(H, "/tmp/e") ?? "", status: Number(get(H, "/tmp/s")) };
}

// Two custom sections code metadata, which --strip-all keeps, and one not.
function custom(name) {
    const n = new TextEncoder().encode(name);
    return [0, n.length + 3, n.length, ...n, 1, 2];
}
const metadata = new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0, ...custom("metadata.code.hint"),
                                 ...custom("foo"), ...custom("metadata.codex")]);
writeFileSync(join(tmp, "metadata.wasm"), metadata);

// ------------------------------------------------------------ llvm-strip's bytes

const MODES = [[], ["-g"], ["-R", "producers"], ["-g", "-R", "producers"],
               ["--keep-section=name"]];
// What llvm-strip was found to do: -g takes reloc..debug* by name, not by
// target; --keep-section wins over -R; --strip-all keeps metadata.code.*.
const EDGES = [["-g", "--keep-section=.debug_info"], ["-g", "--keep-section=reloc..debug_info"],
               ["--keep-section=producers", "-R", "producers"], ["-g", "-R", "reloc.CODE"],
               ["-R", "metadata.code.hint"], ["-R", "nosuch"]];

const inputs = new Set([m.ld, join(tmp, "metadata.wasm")]);
for (const fx of Object.values(m.fixtures)) {
    for (const o of fx.objects)
        inputs.add(o);
    inputs.add(fx.reference);
}
const debug_objects = new Set(m.fixtures.debug.objects);

let cases = 0;
for (const path of inputs) {
    plant(H, "/tmp/in", new Uint8Array(readFileSync(path)));
    const modes = debug_objects.has(path) || path === m.fixtures.debug.reference ||
        path.endsWith("metadata.wasm") ? [...MODES, ...EDGES] : MODES;
    for (const mode of modes) {
        const want = join(tmp, "want");
        const r = spawnSync(LLVM_STRIP, ["--keep-section=braam", ...mode, path, "-o", want],
                            { encoding: "utf8" });
        if (r.error || r.status !== 0)
            die(`llvm-strip ${mode.join(" ")} ${path}: ${r.error?.message ?? r.stderr}`);
        H.store.files.delete("/tmp/got");
        const got = sh(`strip ${mode.join(" ")} /tmp/in -o /tmp/got`);
        cases++;
        if (got.status !== 0 || got.err)
            bad.push(`${basename(path)} [${mode.join(" ")}]: status ${got.status}: ${got.err}`);
        else if (!same(H.store.files.get("/tmp/got"), readFileSync(want)))
            bad.push(`${basename(path)} [${mode.join(" ")}]: not llvm-strip's bytes`);
    }
}

// ------------------------------------------------------------ what the bytes mean

// A stripped program still runs.
for (const [name, fx] of Object.entries(m.fixtures)) {
    plant(H, "/tmp/in", new Uint8Array(readFileSync(fx.reference)));
    const r = sh("strip /tmp/in -o /tmp/got");
    if (r.status !== 0) {
        bad.push(`${name}: strip fails: ${r.err}`);
        continue;
    }
    for (const b of check_run(H, name, H.store.files.get("/tmp/got")))
        bad.push(`${name} stripped: ${b}`);
}

// ld links -g-stripped objects, placeholders and all, as wasm-ld does.
{
    const dir = join(tmp, "g");
    mkdirSync(dir);
    const objects = [];
    for (const o of m.fixtures.debug.objects) {
        plant(H, "/tmp/in", new Uint8Array(readFileSync(o)));
        const r = sh("strip -g /tmp/in -o /tmp/got");
        if (r.status !== 0)
            die(`strip -g ${o}: ${r.err}`);
        const to = join(dir, basename(o));
        writeFileSync(to, H.store.files.get("/tmp/got"));
        objects.push(to);
    }
    const { tmp: out, inputs: place, wasm_ld, ld } = linkers(H, m);
    const args = [...FLAGS.filter((f) => f !== "--import-memory"),
                  ...place({ objects, libs: m.fixtures.debug.libs })];
    H.store.files.delete("/tmp/out.wasm");
    const ours = ld(["--no-import-memory", ...args, "-o", "out.wasm"]);
    const theirs = wasm_ld(args);
    if (ours.status !== 0 || theirs.status !== 0) {
        bad.push(`linking -g-stripped objects: ld ${ours.status} ${ours.err}` +
                 `, wasm-ld ${theirs.status} ${theirs.err}`);
    } else {
        // ld's output is wasm-ld's but for the braam section.
        const a = sections(H.store.files.get("/tmp/out.wasm")).filter((s) => s.name !== "braam");
        const b = sections(new Uint8Array(readFileSync(join(out, "out.wasm"))));
        const key = (s) => `${s.id}:${s.name ?? ""}:${Buffer.from(s.body).toString("hex")}`;
        if (a.map(key).join() !== b.map(key).join())
            bad.push("linking -g-stripped objects: ld's output is not wasm-ld's");
    }
}

// In place is -o's result, leaves no .strip behind, and a second strip
// changes nothing.
{
    const obj = new Uint8Array(readFileSync(m.fixtures.debug.objects[0]));
    const prog = new Uint8Array(readFileSync(m.fixtures.debug.reference));
    plant(H, "/tmp/p", prog);
    plant(H, "/tmp/q", obj);
    plant(H, "/tmp/in", prog);
    sh("strip -g /tmp/in -o /tmp/g1");
    plant(H, "/tmp/in", obj);
    sh("strip -g /tmp/in -o /tmp/g2");
    const r = sh("strip -g /tmp/p /tmp/q");
    if (r.status !== 0 || r.err)
        bad.push(`in place: status ${r.status}: ${r.err}`);
    if (!same(H.store.files.get("/tmp/p"), H.store.files.get("/tmp/g1")) ||
        !same(H.store.files.get("/tmp/q"), H.store.files.get("/tmp/g2")))
        bad.push("in place is not -o's result");
    if (H.store.files.has("/tmp/p.strip") || H.store.files.has("/tmp/q.strip"))
        bad.push("in place left a .strip behind");
    const once = H.store.files.get("/tmp/p");
    sh("strip -g /tmp/p");
    if (!same(H.store.files.get("/tmp/p"), once))
        bad.push("a second strip changed the file");

    // A temporary name already taken is an error, and nothing is touched.
    plant(H, "/tmp/r", prog);
    plant(H, "/tmp/r.strip", "mine");
    const t = sh("strip /tmp/r");
    if (t.err !== "strip: error: cannot create /tmp/r.strip: already exists\n" || t.status !== 1)
        bad.push(`a taken .strip: status ${t.status}: ${t.err}`);
    if (!same(H.store.files.get("/tmp/r"), prog) || get(H, "/tmp/r.strip") !== "mine")
        bad.push("a taken .strip: a file was touched");
}

// ------------------------------------------------------------ errors

{
    const prog = new Uint8Array(readFileSync(m.fixtures.hello.reference));
    plant(H, "/tmp/notwasm", "hello world");
    plant(H, "/tmp/trunc", new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0, 1, 5, 0]));
    plant(H, "/tmp/bc", new Uint8Array([0x42, 0x43, 0xc0, 0xde, 1, 2, 3, 4]));
    const ERRORS = [
        ["strip /tmp/notwasm", "/tmp/notwasm: not a wasm module"],
        ["strip /tmp/trunc", "/tmp/trunc: section at file offset 0x8: runs past the end of the file"],
        ["strip /tmp/bc", "/tmp/bc: LLVM bitcode (from -flto), not a wasm module"],
        ["strip /tmp/nosuch", "cannot open /tmp/nosuch: not found"],
        ["strip /tmp/a /tmp/b -o /tmp/c", "multiple input files cannot be used in combination with -o"],
        ["strip -R braam /tmp/a", "removing the braam section makes the program unrunnable"],
        ["strip -R CODE /tmp/a", "removing the CODE section makes the module invalid"],
    ];
    for (const [cmd, msg] of ERRORS) {
        const r = sh(cmd);
        if (r.status !== 1 || r.err !== `strip: error: ${msg}\n`)
            bad.push(`${cmd}: status ${r.status}: ${JSON.stringify(r.err)}, expected ${msg}`);
    }

    // One bad file among good ones: the good ones are still stripped.
    plant(H, "/tmp/x1", prog);
    plant(H, "/tmp/x2", prog);
    const r = sh("strip /tmp/x1 /tmp/notwasm /tmp/x2");
    if (r.status !== 1 || r.err !== "strip: error: /tmp/notwasm: not a wasm module\n")
        bad.push(`a bad file among good: status ${r.status}: ${r.err}`);
    if (same(H.store.files.get("/tmp/x1"), prog) || same(H.store.files.get("/tmp/x2"), prog))
        bad.push("a bad file among good: the good ones were not stripped");
    if (get(H, "/tmp/notwasm") !== "hello world")
        bad.push("a bad file among good: the bad one was touched");
}

if (bad.length)
    die(`${bad.length} failures:\n  ` + bad.join("\n  "));
console.log(`strip ok: ${cases} files as llvm-strip strips them, and what the bytes mean`);
