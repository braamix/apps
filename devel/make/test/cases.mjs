// make's cases: each <case>.args is one run in /tmp/t, where every fixture
// here is planted, and <case>.expected is its stdout and stderr together;
// <case>.status the exit status when it is not 0, <case>.in its stdin, and
// <case>.env NAME=value words for its environment.  The
// first twenty-six are the BESM-6 port's; README.md, "Tests", says what the
// rest add.  --bless writes what came out as the expectation.

import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, utimesSync,
         writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const APPS = resolve(HERE, "../../..");
const BINARY = join(APPS, "build/devel/make/make.wasm");
const AR = join(APPS, "build/devel/wasm/ar/ar.wasm");
const DIR = "/tmp/t";
const bless = process.argv.includes("--bless");
const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

function die(msg) {
    console.error("make: " + msg);
    process.exit(1);
}

for (const [what, path] of [["kernel", KERNEL], ["rootfs", ROOTFS], ["binary", BINARY],
                            ["ar", AR]])
    if (!existsSync(path))
        die(`no ${what} at ${path} — run \`make\``);

const H = await import(HARNESS);
await H.init(KERNEL, ROOTFS);
H.kernel().init(0);
if (H.run(0) !== -1)
    die("the shell did not park on the keyboard");
H.regrid(80, 24, "resize returned no screen descriptor");
H.store.files.set("/bin/make", new Uint8Array(readFileSync(BINARY)));
H.store.files.set("/bin/ar", new Uint8Array(readFileSync(AR)));
H.store.dirs.add(DIR);

const enc = (s) => (typeof s === "string" ? new TextEncoder().encode(s) : s);
const get = (path) => {
    const b = H.store.files.get(path);
    return b === undefined ? undefined : new TextDecoder().decode(b);
};

// A file and its time: the clock is frozen, so whatever a case compares is
// set here.  Times are well before the service clock's, which is "now".
const T0 = 1781000000000;
function put(path, bytes, mtime = T0) {
    H.store.files.set(path, enc(bytes));
    H.store.mtimes.set(path, mtime);
    for (let d = dirname(path); d !== "/"; d = dirname(d))
        H.store.dirs.add(d);
}

let now = 1;
function sh(cmd) {
    if (cmd.length > 60)
        die(`the command line is ${cmd.length} keys, and the ring holds 64`);
    H.submit(cmd, now++);
    if (H.run(now++) !== -1)
        die("the kernel did not settle after: " + cmd);
}

// System V's ar, as devel/wasm's ar writes it: the symbol table first, then
// the long names, then the members.  Only the layout matters to make.
function archive(members, symbols) {
    const pad = (s, n) => s.padEnd(n, " ");
    const hdr = (name, date, size) =>
        pad(name, 16) + pad(String(date), 12) + pad("0", 6) + pad("0", 6) + pad("644", 8) +
        pad(String(size), 10) + "`\n";
    let longnames = "";
    const names = members.map((m) => {
        if (m.name.length <= 15)
            return m.name + "/";
        const at = longnames.length;
        longnames += m.name + "/\n";
        return "/" + at;
    });
    const symnames = symbols.map((s) => s.name + "\0").join("");
    const symsize = 4 + 4 * symbols.length + symnames.length;
    let off = 8;
    if (symbols.length)
        off += 60 + symsize + (symsize & 1);
    if (longnames)
        off += 60 + longnames.length + (longnames.length & 1);
    const at = [];
    for (const m of members) {
        at.push(off);
        off += 60 + m.data.length + (m.data.length & 1);
    }
    const be = (v) => String.fromCharCode((v >>> 24) & 255, (v >>> 16) & 255, (v >>> 8) & 255,
                                          v & 255);
    let s = "!<arch>\n";
    if (symbols.length) {
        s += hdr("/", 0, symsize) + be(symbols.length);
        for (const sym of symbols)
            s += be(at[sym.member]);
        s += symnames + (symsize & 1 ? "\n" : "");
    }
    if (longnames)
        s += hdr("//", "", longnames.length) + longnames + (longnames.length & 1 ? "\n" : "");
    members.forEach((m, i) => {
        s += hdr(names[i], m.date, m.data.length) + m.data + (m.data.length & 1 ? "\n" : "");
    });
    return Uint8Array.from(s, (c) => c.charCodeAt(0));
}

// What every case starts from: the fixtures, older than anything a case makes.
function plant_fixtures() {
    for (const p of [...H.store.files.keys()])
        if (p.startsWith(DIR + "/"))
            H.store.files.delete(p), H.store.mtimes.delete(p);
    for (const f of readdirSync(HERE))
        if (/\.(mk|c|h|in)$/.test(f))
            put(`${DIR}/${f}`, readFileSync(join(HERE, f)));
    put(`${DIR}/libmk.a`, archive([{ name: "mkzork.o", date: T0 / 1000, data: "obj\n" }],
                                  [{ name: "mkzork", member: 0 }]));
}

// What a case needs beyond the fixtures, by name.
const SETUP = {
    // Stale and fresh are decided by the times alone.
    uptodate: () => {
        put(`${DIR}/a.o`, "", T0 + 1000);
        put(`${DIR}/prog`, "", T0 + 2000);
    },
    stale: () => {
        put(`${DIR}/prog`, "", T0 + 2000);
        put(`${DIR}/a.o`, "", T0 + 3000);
    },
    qok: () => {
        for (const f of ["a.o", "b.o"])
            put(`${DIR}/${f}`, "", T0 + 1000);
        put(`${DIR}/prog`, "", T0 + 2000);
    },
    touchrun: () => put(`${DIR}/a.o`, "x", T0 + 1000),
    // An archive devel/wasm's ar wrote, the long name in its `//' member, and
    // -U for the dates make compares.  They are whole seconds: hdr.h is no
    // newer than its member, the other source is.
    realar: () => {
        put(`${DIR}/a-rather-long-member.c`, "int x;\n", T0);
        sh(`cd ${DIR}; ar crU libr.a a-rather-long-member.c hdr.h`);
        sh("cd /");
        put(`${DIR}/a-rather-long-member.c`, "int x;\n", T0 + 5000);
        if (!get(`${DIR}/libr.a`)?.includes("//"))
            die("ar wrote no long-name table");
    },
    // No makefile at all: the built-in .c: rule makes hello from hello.c.
    single: () => put(`${DIR}/hello.c`, "int x;\n"),
    singlecmd: () => put(`${DIR}/hello.c`, "int x;\n"),
    // Files named like the phony targets, newer than anything: ignored.
    phony: () => {
        put(`${DIR}/prog`, "", T0 + 1000);
        put(`${DIR}/all`, "", T0 + 5000);
        put(`${DIR}/clean`, "", T0 + 5000);
    },
    // Three hundred includes in a row, each a parse loop await that parks on a
    // read; and a chain fifty deep.
    incmany: () => {
        let mk = "";
        for (let i = 0; i < 300; i++) {
            put(`${DIR}/i${i}.mk`, `LAST = ${i}\n`);
            mk += `include i${i}.mk\n`;
        }
        put(`${DIR}/many.mk`, mk + "all:\n\techo [$(LAST)]\n");
    },
    incdeep: () => {
        for (let i = 0; i < 50; i++)
            put(`${DIR}/c${i}.mk`, i < 49 ? `include c${i + 1}.mk\n` : "DEEP = 49\n");
        put(`${DIR}/c0.mk`, "include c1.mk\nall:\n\techo [$(DEEP)]\n");
    },
    // Past every limit v7 had: 600 targets on one rule line and in the name
    // table, a 3 KB line, a 3 KB $? and 600 arguments to a command run
    // directly; and a 200-character name through an inference rule.
    limits: () => {
        const t = Array.from({ length: 600 }, (_, i) => `t${String(i).padStart(3, "0")}`);
        for (const n of t)
            put(`${DIR}/${n}`, "", T0);
        put(`${DIR}/limits.mk`, `T = ${t.join(" ")}\nall: $(T)\n\techo $?\n` +
            `\techo $(words $(foreach t,$(T),$(t).o $(t).c))\n$(T):\n`);
    },
    wildcard: () => {
        for (const f of ["z.x", "a.x", ".hidden"])
            put(`${DIR}/sub/${f}`, "");
    },
    patrule: () => {
        put(`${DIR}/bb.c`, "");
        put(`${DIR}/sub/x.c`, "");
    },
    vpath: () => {
        put(`${DIR}/src/main.c`, "");
        put(`${DIR}/other/util.c`, "");
        put(`${DIR}/inc/defs.h`, "");
    },
    vpathsuf: () => put(`${DIR}/src/main.c`, ""),
    vpathtgt: () => {
        put(`${DIR}/other/util.c`, "", T0);
        put(`${DIR}/other/lib.a`, "", T0 + 5000);
    },
    vpathold: () => {
        put(`${DIR}/other/util.c`, "", T0 + 5000);
        put(`${DIR}/other/lib.a`, "", T0);
    },
    orderonly: () => {
        put(`${DIR}/a.o`, "", T0 + 1000);
        put(`${DIR}/prog`, "", T0 + 2000);
    },
    delerr: () => {
        put(`${DIR}/src`, "", T0 + 1000);
        put(`${DIR}/old`, "was", T0);
    },
    longname: () => put(`${DIR}/${"x".repeat(200)}.c`, "", T0),
    // Two -C, the first with its directory attached.
    chdir: () => put(`${DIR}/cdir/deeper/makefile`, "all:\n\tpwd\n"),
    interrupt: () => put(`${DIR}/src`, "", T0 + 1000),
    precious: () => put(`${DIR}/src`, "", T0 + 1000),
    longmember: () => put(`${DIR}/libl.a`, archive(
        [{ name: "short.o", date: T0 / 1000, data: "s" },
         { name: "a-rather-long-member.o", date: T0 / 1000 + 5, data: "long" }], [])),
};

// ^C, as kill -INT: make in the background, parked in wait_child() on a sleep
// that the frozen clock never ends, and the kill queued behind it.
// A background job's number counts up for the life of the shell.
const INTERRUPT = new Set(["interrupt", "precious"]);
let job = 0;

// What a case is checked for beyond its output, by name.
const CHECK = {
    delerr: () => get(`${DIR}/out`) === undefined && get(`${DIR}/old`) === "was" ||
        "out was kept, or old was not",
    shellfn: () => get(`${DIR}/made`) === "hi\n" || "made was not written",
    shell: () => get(`${DIR}/shout`) === "through the shell\n" || "shout was not written",
    touchrun: () => get(`${DIR}/prog`) !== undefined || "prog was not created",
    interrupt: () => get(`${DIR}/keep`) === undefined || "keep was not removed",
    precious: () => get(`${DIR}/keep`) === "new\n" || "keep was not kept",
};

// GNU make on the host, where there is one: each case named here runs under -n
// on both as well, over the same files at the same times, and the commands
// must agree.  The messages, which are v7's here, are not compared.
const GMAKE_CASES = new Set([
    "assign", "autovars", "basic", "cmdline", "contcmd", "contdef", "dcolon", "default",
    "env", "envflag", "ignoresome", "include", "incdeep", "incmany", "lastline", "limits",
    "longname", "macros", "dirfile", "special", "shellassign",
    "phony", "silent", "silentsome", "substref", "suffix",
    "cond", "define", "export", "exportall", "funcs", "shellfn", "wildcard",
    "orderonly", "autolists", "goals", "goalsnamed", "defgoal",
    "patrule", "patcancel", "patcancel2", "static",
    "vpath", "vpathsuf", "vpathtgt", "vpathold", "funcs2", "eval", "tvars", "tvarscmd", "longopts", "mflong", "oneshell", "oneshell2",
]);
const GMAKE = (() => {
    for (const g of ["gmake", "make"])
        try {
            const v = execFileSync(g, ["--version"], { encoding: "utf8" });
            if (/^GNU Make [4-9]/.test(v))
                return g;
        } catch {}
    return null;
})();

// What -n says, less what is only a message.
const commands = (out) => out.split("\n").filter((l) => l !== "" && l !== "Stop." &&
    !/^(g?make|Make)(\[\d+\])?: /.test(l) && !/is up to date\.$/.test(l) &&
    !/^(line \d+|[\w./-]+:\d+): /.test(l)).join("\n"); // a $(warning)

function gmake_check(name, args, env) {
    const host = mkdtempSync(join(tmpdir(), "make-gmake-"));
    try {
        for (const [path, bytes] of H.store.files)
            if (path.startsWith(DIR + "/")) {
                const f = join(host, path.slice(DIR.length + 1));
                mkdirSync(dirname(f), { recursive: true });
                writeFileSync(f, bytes);
                const t = (H.store.mtimes.get(path) ?? T0) / 1000;
                utimesSync(f, t, t);
            }
        let theirs;
        try {
            theirs = execFileSync("/bin/sh", ["-c", `${env}${GMAKE} -n --no-print-directory ` +
                                  `${args.replaceAll(DIR, host)} 2>&1`],
                                  { cwd: host, encoding: "utf8",
                                    // SHELL, or GNU make runs a command-line
                                    // != with no shell at all
                                    env: { PATH: process.env.PATH, HOME: "/home",
                                           SHELL: "/bin/sh" } });
        } catch (e) {
            theirs = e.stdout;
        }
        put("/tmp/run", `cd ${DIR}\n${env}make -n ${args} >/tmp/out 2>&1\n`);
        sh("sh /tmp/run");
        const ours = get("/tmp/out") ?? "";
        if (commands(ours) !== commands(theirs))
            return `-n differs from ${GMAKE}:\n--- ${GMAKE}\n${theirs}--- make\n${ours}---`;
        return true;
    } finally {
        rmSync(host, { recursive: true });
    }
}

const names = readdirSync(HERE).filter((f) => f.endsWith(".args")).map((f) => f.slice(0, -5))
    .filter((n) => !only.length || only.includes(n)).sort();
const failed = [];

for (const name of names) {
    plant_fixtures();
    SETUP[name]?.();
    const args = readFileSync(join(HERE, name + ".args"), "utf8").trim()
        .replaceAll("@srcdir@", DIR);
    const stdin = existsSync(join(HERE, name + ".in")) ? `${DIR}/${name}.in` : "/dev/null";
    H.store.files.delete("/tmp/out");
    H.store.files.delete("/tmp/st");
    if (INTERRUPT.has(name)) {
        sh(`cd ${DIR}`);
        job++;
        for (const line of [`make ${args} >/tmp/out 2>&1 &`, `kill -INT %${job}`]) {
            if (line.length > 60)
                die(`the command line is ${line.length} keys, and the ring holds 64`);
            H.type(line);
            H.press(H.KEY.ENTER);
        }
        if (H.run(now++) !== -1)
            die("the kernel did not settle after the interrupt");
        sh(`wait %${job}; echo $? >/tmp/st`);
        sh("cd /");
    } else {
        const env = existsSync(join(HERE, name + ".env"))
            ? readFileSync(join(HERE, name + ".env"), "utf8").trim() + " " : "";
        if (GMAKE && GMAKE_CASES.has(name) && !bless) {
            const g = gmake_check(name, args, env);
            if (g !== true) {
                failed.push(name);
                console.error(`make ${name}: ${g}`);
            }
            H.store.files.delete("/tmp/out");
        }
        put("/tmp/run",
            `cd ${DIR}\n${env}make ${args} <${stdin} >/tmp/out 2>&1\necho $? >/tmp/st\n`);
        sh("sh /tmp/run");
    }

    const out = get("/tmp/out") ?? "";
    const st = (get("/tmp/st") ?? "").trim();
    const want_file = join(HERE, name + ".expected");
    const status_file = join(HERE, name + ".status");
    if (bless) {
        writeFileSync(want_file, out);
        if (st !== "0")
            writeFileSync(status_file, st + "\n");
        else if (existsSync(status_file))
            writeFileSync(status_file, "0\n");
        console.log(`${name}: blessed, status ${st}`);
        continue;
    }
    const want = existsSync(want_file) ? readFileSync(want_file, "utf8") : "";
    const want_st = existsSync(status_file) ? readFileSync(status_file, "utf8").trim() : "0";
    const why = [];
    if (out !== want)
        why.push(`output:\n--- want\n${want}--- got\n${out}---`);
    if (st !== want_st)
        why.push(`status ${st}, want ${want_st}`);
    const c = CHECK[name]?.();
    if (c !== undefined && c !== true)
        why.push(c);
    if (why.length) {
        failed.push(name);
        console.error(`make ${name}: ` + why.join("\n"));
    }
}

if (failed.length)
    die(`${failed.length} of ${names.length} failed: ${failed.join(" ")}`);
console.log(`make ok: ${names.length} cases` +
            (GMAKE ? `, ${names.filter((n) => GMAKE_CASES.has(n)).length} against ${GMAKE} -n`
                   : `; no GNU make 4 here to compare -n with`));
