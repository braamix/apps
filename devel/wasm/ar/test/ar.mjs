// ar, held to llvm-ar --format=gnu byte for byte: each mode on the fixtures'
// objects, programs, a text member, and a name past 15 characters. Every SDK archive
// taken apart and put back together is the SDK's own. Then what the bytes
// mean: ld links archives ar made as wasm-ld does. Then the text ar prints,
// against golden files, a round trip, and the errors.

import { execFileSync } from "node:child_process";
import { mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync }
    from "node:fs";
import { tmpdir } from "node:os";
import { basename, dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { boot, FLAGS, get, linkers, manifest, plant, run, sections }
    from "../../ld/test/wasmlib.mjs";

function die(msg) {
    console.error("ar: " + msg);
    process.exit(1);
}

const HERE = dirname(fileURLToPath(import.meta.url));
const GOLDEN = join(HERE, "golden");
const m = manifest();
const AR = join(dirname(m.ld), "../ar/ar.wasm");
const tmp = mkdtempSync(join(tmpdir(), "ar-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));

const H = await boot();
plant(H, "/bin/ar", new Uint8Array(readFileSync(AR)));
const bad = [];
const same = (a, b) => a && b && Buffer.compare(Buffer.from(a), Buffer.from(b)) === 0;

// Command lines of any length, run in /tmp; stdout and stderr of them all,
// and the last one's status.
function sh(cmd) {
    for (const k of ["/tmp/o", "/tmp/e", "/tmp/s"])
        H.store.files.delete(k);
    const lines = cmd.split("\n").map((l) => `${l} >>/tmp/o 2>>/tmp/e`).join("\n");
    plant(H, "/tmp/c", `cd /tmp\n${lines}\necho $? >/tmp/s\n`);
    run(H, "sh /tmp/c");
    return { out: get(H, "/tmp/o") ?? "", err: get(H, "/tmp/e") ?? "",
             status: Number(get(H, "/tmp/s")) };
}

// A file in /tmp on Braam and in tmp on the host.
function put(name, bytes) {
    const b = typeof bytes === "string" ? Buffer.from(bytes) : Buffer.from(bytes);
    plant(H, `/tmp/${name}`, new Uint8Array(b));
    writeFileSync(join(tmp, name), b);
}

const llvm_ar = (args, cwd = tmp) => execFileSync(m.ar, args, { cwd, maxBuffer: 1 << 28 });

// ------------------------------------------------------------ llvm-ar's bytes

const LONG = "a_member_name_longer_than_sixteen.o";
const data = m.fixtures.data.objects;
put(LONG, readFileSync(data[0]));
put("short.o", readFileSync(data[1]));
put("one.o", readFileSync(m.fixtures.hello.objects[0]));
put("notes.txt", "a member that is not an object\n");
put("prog.wasm", readFileSync(m.fixtures.export.reference));
execFileSync(join(dirname(m.objdump), "llvm-strip"),
             ["--keep-section=braam", "prog.wasm", "-o", join(tmp, "bare.wasm")], { cwd: tmp });
put("bare.wasm", readFileSync(join(tmp, "bare.wasm")));

// Each step runs ar on Braam and llvm-ar on the host, then compares.
const STEPS = [
    ["rc", "x.a", `rc x.a ${LONG} short.o notes.txt`, ["rc", "x.a", LONG, "short.o", "notes.txt"]],
    ["r of a new member", "x.a", "r x.a one.o", ["r", "x.a", "one.o"]],
    ["r of a member there", "x.a", "r x.a short.o", ["r", "x.a", "short.o"], () =>
        put("short.o", readFileSync(m.fixtures.ctors.objects[0]))],
    ["m -b", "x.a", "m -b short.o x.a one.o", ["mb", "short.o", "x.a", "one.o"]],
    ["m -a", "x.a", `m -a one.o x.a ${LONG}`, ["ma", "one.o", "x.a", LONG]],
    ["m", "x.a", "m x.a notes.txt", ["m", "x.a", "notes.txt"]],
    ["d", "x.a", "d x.a one.o", ["d", "x.a", "one.o"]],
    ["qc", "y.a", `qc y.a one.o ${LONG}`, ["qc", "y.a", "one.o", LONG]],
    ["qc again", "y.a", "qc y.a short.o", ["qc", "y.a", "short.o"]],
    ["S", "z.a", `rcS z.a short.o ${LONG} one.o`, ["rcS", "z.a", "short.o", LONG, "one.o"]],
    ["s", "z.a", "s z.a", ["s", "z.a"]],
    ["programs", "p.a", "rc p.a prog.wasm bare.wasm one.o",
     ["rc", "p.a", "prog.wasm", "bare.wasm", "one.o"]],
];
let steps = 0;
for (const [what, name, ours, theirs, before] of STEPS) {
    if (before)
        before();
    const r = sh(`ar ${ours}`);
    llvm_ar([theirs[0], "--format=gnu", ...theirs.slice(1)]);
    if (r.status !== 0 || r.err)
        bad.push(`${what}: status ${r.status}: ${r.err}`);
    else if (!same(H.store.files.get(`/tmp/${name}`), readFileSync(join(tmp, name))))
        bad.push(`${what}: not llvm-ar's bytes`);
    steps++;
}
if (H.store.files.has("/tmp/x.a.ar"))
    bad.push("a replaced archive left its .ar behind");

// ------------------------------------------------------------ the SDK

// Each archive's members extracted by ar, then put back in their order.
let archives = 0, symbols = 0;
for (const f of readdirSync(m.sdk_libs).filter((n) => /^libbraam_.*\.a$/.test(n)).sort()) {
    const lib = readFileSync(join(m.sdk_libs, f));
    const names = llvm_ar(["t", join(m.sdk_libs, f)]).toString().split("\n").filter(Boolean);
    const dir = f.replace(/\.a$/, "");
    plant(H, `/tmp/${f}`, new Uint8Array(lib));
    const r = sh(`mkdir ${dir}\ncd ${dir}\nar x ../${f}\nar rc ../re_${f} ${names.join(" ")}`);
    if (r.status !== 0 || r.err)
        bad.push(`${f}: status ${r.status}: ${r.err}`);
    else if (!same(H.store.files.get(`/tmp/re_${f}`), lib))
        bad.push(`${f}: rebuilt, it is not the SDK's archive`);
    archives++;
    if (lib.subarray(8, 10).toString() === "/ ")
        symbols += lib.readUInt32BE(68);
}

// ------------------------------------------------------------ what the bytes mean

// The archives fixture, its archives made by ar, links as wasm-ld links it.
{
    const fx = m.fixtures.archives;
    const dir = join(tmp, "fx");
    mkdirSync(dir);
    const archives_made = [];
    for (const a of fx.archives) {
        const x = join(dir, "x-" + basename(a));
        mkdirSync(x);
        llvm_ar(["x", a], x);
        const names = llvm_ar(["t", a]).toString().split("\n").filter(Boolean);
        for (const n of names)
            put(n, readFileSync(join(x, n)));
        const r = sh(`ar rc ${basename(a)} ${names.join(" ")}`);
        if (r.status !== 0)
            die(`ar rc ${basename(a)}: ${r.err}`);
        const to = join(dir, basename(a));
        writeFileSync(to, H.store.files.get(`/tmp/${basename(a)}`));
        archives_made.push(to);
    }
    const { tmp: out, inputs: place, wasm_ld, ld } = linkers(H, m);
    const args = [...FLAGS.filter((f) => f !== "--import-memory"),
                  ...place({ objects: fx.objects, archives: archives_made, libs: fx.libs })];
    H.store.files.delete("/tmp/out.wasm");
    const ours = ld(["--no-import-memory", ...args, "-o", "out.wasm"]);
    const theirs = wasm_ld(args);
    if (ours.status !== 0 || theirs.status !== 0) {
        bad.push(`linking archives ar made: ld ${ours.status} ${ours.err}` +
                 `, wasm-ld ${theirs.status} ${theirs.err}`);
    } else {
        // ld's output is wasm-ld's but for the braam section.
        const a = sections(H.store.files.get("/tmp/out.wasm")).filter((s) => s.name !== "braam");
        const b = sections(new Uint8Array(readFileSync(join(out, "out.wasm"))));
        const key = (s) => `${s.id}:${s.name ?? ""}:${Buffer.from(s.body).toString("hex")}`;
        if (a.map(key).join() !== b.map(key).join())
            bad.push("linking archives ar made: ld's output is not wasm-ld's");
    }
}

// ------------------------------------------------------------ text

// Upstream's formats, which llvm-ar does not share.
const TEXT = [
    ["t", "ar t x.a"],
    ["tv", "ar tv x.a"],
    ["t_named", `ar t x.a notes.txt ${LONG} nosuch.o`],
    ["pv", "ar pv x.a notes.txt"],
    ["v", "ar rcv v.a one.o notes.txt\nar rv v.a one.o\nar dv v.a notes.txt"],
];
for (const [name, cmd] of TEXT) {
    const r = sh(cmd);
    const got = `status ${r.status}\n--- stdout\n${r.out}--- stderr\n${r.err}`;
    const file = join(GOLDEN, `${name}.out`);
    if (process.env.BLESS) {
        mkdirSync(GOLDEN, { recursive: true });
        writeFileSync(file, got);
    }
    let want = null;
    try {
        want = readFileSync(file, "utf8");
    } catch {}
    if (got !== want)
        bad.push(`${cmd}: not ${name}.out:\n${got}`);
}

// ------------------------------------------------------------ round trip

// -x then -q gives the archive back, and -x -C keeps what is there.
{
    const names = sh("ar t x.a").out.split("\n").filter(Boolean);
    const r = sh(`mkdir rt\ncd rt\nar x ../x.a\nar qc ../rt.a ${names.join(" ")}`);
    if (r.status !== 0 || r.err)
        bad.push(`round trip: status ${r.status}: ${r.err}`);
    else if (!same(H.store.files.get("/tmp/rt.a"), H.store.files.get("/tmp/x.a")))
        bad.push("round trip: -x then -q is not the archive");
    plant(H, "/tmp/rt/notes.txt", "mine");
    sh("cd rt\nar xC ../x.a notes.txt");
    if (get(H, "/tmp/rt/notes.txt") !== "mine")
        bad.push("-x -C overwrote a file");
}

// ------------------------------------------------------------ errors

{
    const ERRORS = [
        ["ar t nothere.a", "ar: fatal: Failed to open 'nothere.a': not found\n"],
        ["ar t notes.txt", "ar: warning: Unrecognized archive format\n"],
        ["ar d nothere.a one.o",
         "ar: warning: nothere.a: no such file\nar: warning: one.o: not found in archive\n"],
        ["ar -ra x.a", null],
        ["ar -r -a p -b x.a one.o", "ar: fatal: only one of -a and -[bi] options allowed\n"],
        ["ar -t -r x.a", "ar: fatal: Can't specify both -r and -t\n"],
        ["ar -x -c x.a", "ar: fatal: Option -c is not permitted in mode -x\n"],
        ["ar r x.a nosuch.o", "ar: warning: can't open file: nosuch.o: not found\n"],
    ];
    for (const [cmd, msg] of ERRORS) {
        const r = sh(cmd);
        const ok = msg === null ? r.err.startsWith("Usage:\n    ar -r ") : r.err === msg;
        if (r.status !== 1 || !ok)
            bad.push(`${cmd}: status ${r.status}: ${JSON.stringify(r.err)}, expected ` +
                     (msg === null ? "the usage" : JSON.stringify(msg)));
    }

    // A temporary name already taken is an error, and the archive is kept.
    const before = H.store.files.get("/tmp/x.a");
    plant(H, "/tmp/x.a.ar", "mine");
    const r = sh("ar r x.a one.o");
    if (r.status !== 1 || r.err !== "ar: warning: Failed to open 'x.a': already exists\n")
        bad.push(`a taken .ar: status ${r.status}: ${JSON.stringify(r.err)}`);
    if (!same(H.store.files.get("/tmp/x.a"), before) || get(H, "/tmp/x.a.ar") !== "mine")
        bad.push("a taken .ar: a file was touched");
}

if (bad.length)
    die(`${bad.length} failures:\n  ` + bad.join("\n  "));
console.log(`ar ok: ${steps} steps as llvm-ar writes them; ${archives} SDK archives, ` +
            `${symbols} symbols, rebuilt; what the bytes mean; text; round trip; errors`);
