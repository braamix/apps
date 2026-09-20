// Boot the kernel, plant python, run one command with its streams redirected
// to files, read back what it wrote.
//
// A pipe rather than the grid: down a pipe nothing echoes, so the transcript is
// exactly what the program printed -- which is what makes it comparable byte
// for byte with CPython's.

import { existsSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { join, resolve, dirname, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { HARNESS, KERNEL, ROOTFS, SHARE } from "../../../test/sdk.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
export const APPS = resolve(HERE, "../../..");
export { HARNESS, SHARE };

export const opt = {
    kernel: KERNEL,
    rootfs: ROOTFS,
    binary: join(APPS, "build/lang/python/python.wasm"),
    bless: "",
    shard: "",
};
for (const a of process.argv.slice(2)) {
    const m = /^--(\w+)(?:=(.*))?$/.exec(a);
    if (m && m[1] in opt) opt[m[1]] = m[2] === undefined ? "1" : m[2];
}

export let name = "python";

export function die(msg) {
    console.error(`${name}: ${msg}`);
    process.exit(1);
}

const enc = new TextEncoder();
const dec = new TextDecoder();

export let H;

// Checked before the harness is imported: it exits the process itself and
// would not say what to build.
export async function boot(caseName) {
    name = opt.shard ? `${caseName} ${opt.shard}` : caseName;
    for (const [what, path, how] of [
        ["kernel", opt.kernel, "make"],
        ["rootfs", opt.rootfs, "make"],
        ["python", opt.binary, "make"],
    ]) {
        if (!existsSync(path)) {
            console.error(`${caseName}: no ${what} at ${path} — run \`${how}\``);
            process.exit(1);
        }
    }

    H = await import(HARNESS);
    await H.init(opt.kernel, opt.rootfs);
    H.kernel().init(0);
    if (H.run(0) !== -1) die("the kernel did not settle after boot");
    H.regrid(80, 24, "resize returned no screen descriptor");
    if (!H.store.files.has("/bin/sh")) die("the archive did not unpack");
    // Planted, not packed: exec takes any path carrying a stamp. `py`,
    // because a command line here has sixty characters to live in.
    H.store.files.set("/bin/py", new Uint8Array(readFileSync(opt.binary)));
    plant_lib();
    return H;
}

// The library CPython lends, where an installed package keeps it: python
// finds a store directory named python-* with lib inside, and puts that
// on sys.path after the program's own directory.
export const LIB = join(HERE, "..", "lib");
export const STORE_LIB = "/pkg/store/python-0/lib";

function plant_lib() {
    const walk = (at) => {
        for (const e of readdirSync(at)) {
            const p = join(at, e);
            if (statSync(p).isDirectory()) walk(p);
            else if (p.endsWith(".py")) put(`${STORE_LIB}/${relative(LIB, p)}`, readFileSync(p));
        }
    };
    walk(LIB);
}

export function put(path, text) {
    // The directories too: the store keeps them in a set of their own, and
    // without them `stat` says the path is not there. An import looking for a
    // namespace package asks exactly that question.
    for (let i = path.indexOf("/", 1); i > 0; i = path.indexOf("/", i + 1))
        H.store.dirs.add(path.slice(0, i));
    H.store.files.set(path, typeof text === "string" ? enc.encode(text) : text);
}

export function get(path) {
    const b = H.store.files.get(path);
    return b === undefined ? null : dec.decode(b);
}

export function rm(path) {
    H.store.files.delete(path);
}

let clock = 1;

// One run. `tail` is the rest of the command line; stdout and stderr go to
// files, stdin to /tmp/i when given, and `env` is the shell's `VAR=x` prefix,
// which reaches this child alone. The clock is driven, not read, so run until
// the kernel is idle rather than once.
export function run(tail, stdin = null, env = "") {
    rm("/tmp/o");
    rm("/tmp/e");
    let cmd = `${env ? env + " " : ""}py ${tail}`;
    if (stdin !== null) {
        put("/tmp/i", stdin);
        cmd += " </tmp/i";
    }
    cmd += " >/tmp/o 2>/tmp/e";
    // The keyboard is Channel<Key, 64> and type() posts a whole line without
    // checking, so the line and its ENTER have to fit in 64 keys.
    if (cmd.length > 63)
        die(`command line too long for the harness keyboard: ${cmd}`);

    let now = (clock += 100);
    H.type(cmd);
    H.press(H.KEY.ENTER);
    let i = 0;
    for (let delay = H.run(now); delay !== -1; delay = H.run(now)) {
        now += delay > 0 ? delay : 1;
        if (++i > 200000) die(`the run did not finish: ${cmd}`);
    }
    clock = now;

    const s = H.screen();
    const row = H.row(s, s.cursor_y);
    let status = 0;
    while (status < 200 && row !== H.prompt(status)) status++;
    if (status === 200)
        die(`the shell did not get its prompt back: ${JSON.stringify(row)}`);

    return { out: get("/tmp/o") ?? "", err: get("/tmp/e") ?? "", status };
}

// As `python prog.py`.
export function script(source, stdin = null) {
    put("/tmp/c.py", source);
    return run("/tmp/c.py", stdin);
}

// One assertion, reported as a diff.
export function same(what, got, want) {
    if (got === want) return true;
    const a = String(want).split("\n");
    const b = String(got).split("\n");
    for (let i = 0; i < Math.max(a.length, b.length); i++)
        if (a[i] !== b[i]) {
            console.error(`${name}: ${what} differs at line ${i + 1}`);
            console.error(`  want ${JSON.stringify(a[i])}`);
            console.error(`  got  ${JSON.stringify(b[i])}`);
            return false;
        }
    console.error(`${name}: ${what} differs in length: want ${a.length}, got ${b.length}`);
    return false;
}

// A transcript against the file beside this one that recorded it. --bless
// writes the file; without it a difference is the failure, line by line.
export function golden(file, text) {
    const path = join(HERE, file);
    if (opt.bless) {
        writeFileSync(path, text);
        console.error(`${name}: blessed ${file}`);
        return;
    }
    if (!existsSync(path)) die(`no golden at ${path} \u2014 run with --bless`);
    const want = readFileSync(path, "utf8");
    if (text === want) return;
    same(file, text, want);
    process.exit(1);
}

export function ok(msg = "") {
    console.log(`${name} ok${msg ? ": " + msg : ""}`);
}

// `--shard=i/n` keeps every nth row from i, so one long list runs as n tests
// side by side. Round robin and not blocks: what a row costs varies by two
// orders of magnitude and a block of neighbours would not balance.
export function shard(rows) {
    if (!opt.shard) return rows;
    const m = /^(\d+)\/(\d+)$/.exec(opt.shard);
    if (!m) die(`--shard wants i/n, got ${opt.shard}`);
    const i = Number(m[1]), n = Number(m[2]);
    if (i < 1 || i > n) die(`--shard ${opt.shard}: i is 1 to n`);
    return rows.filter((_, k) => k % n === i - 1);
}

// The second run of every case, collecting at every allocation, is opt-in:
// `make test STRESS=1` sets PY_STRESS and the Makefile adds pystress.mjs to
// the list with it. A collection walks the whole live heap, so under stress a
// case that imports the library costs a hundred to a thousand times the plain
// run -- 140 ms against 38 s for stdlib/argparses.py -- which is the whole of
// what this suite used to spend.
export const stress = !!process.env.PY_STRESS;

// What an `ok` line says about that second run, so a log states which of the
// two ran.
export const under_gc = stress ? ", and to themselves under a collector that never waits" : "";

// Every .py in a directory run against the golden the host's CPython wrote
// for it -- plainly, and under PY_STRESS again collecting at every allocation.
// This is the strongest comparison there is: the same program, the two
// interpreters, byte for byte. A case in one of these directories may
// therefore use nothing this interpreter has not got. tools/mkfmt.py writes
// the goldens.
export function against_cpython(dir, only) {
    let bad = 0, ran = 0, lines = 0;
    for (const name of shard(readdirSync(dir).filter((f) => f.endsWith(".py")).sort())) {
        if (only.length && !only.includes(name)) continue;
        const exp = join(dir, name + ".exp");
        if (!existsSync(exp)) die(`${name}: no golden — run tools/mkfmt.py on it`);

        put("/tmp/c.py", readFileSync(join(dir, name)));
        const want = readFileSync(exp, "utf8");
        const r = run("/tmp/c.py");
        ran++;
        lines += want.split("\n").length - 1;
        if (!same(name, r.out + r.err, want)) {
            bad++;
            continue;
        }
        if (!stress) continue;
        const under = run("/tmp/c.py", null, "PY_GC_STRESS=1");
        if (!same(`${name} under gc stress`, under.out + under.err, want)) bad++;
    }
    return { bad, ran, lines };
}
