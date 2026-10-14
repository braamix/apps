// The compiler builds itself on Braam. braam.cmake writes its own build out as
// selfhost.sh; run there with the installed cc, as, ld and ar, it compiles the
// runtime and the five programs, and every object, libc.a and every program
// must be the host build's, byte for byte. The self-built compiler is then
// installed over the package's and compiles the book's programs.

import { execFileSync } from "node:child_process";
import { mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { BOOK } from "./book.mjs";
import { BUILD, TREE, fixture } from "./ccfix.mjs";
import { check_suites } from "./suites.mjs";
import { SDK } from "../../../test/sdk.mjs";

const { die, boot } = fixture("selfhost");
const session = await boot();
const { H, put, text, sh, store } = session;

const T = "/tmp/cc", O = "/tmp/sh";
for (const f of execFileSync("git", ["ls-files", "--cached", "--others", "--exclude-standard"],
                             { cwd: TREE, encoding: "utf8" }).split("\n"))
    if (/\.[chs]$/.test(f))
        put(`${T}/${f}`, new Uint8Array(readFileSync(join(TREE, f))));
// The SDK's math sources, at math/ beside the tree, as csources has them.
for (const dir of ["musl", "cvt"]) {
    const from = join(SDK, "share/braam/src/math", dir);
    for (const f of readdirSync(from))
        if (/\.[ch]$/.test(f))
            put(`${T}/math/${dir}/${f}`, new Uint8Array(readFileSync(join(from, f))));
}
const script = `T=${T}\nO=${O}\nPATH=$PATH:${store}/libexec\ncd $T\n` +
               readFileSync(join(BUILD, "selfhost.sh"), "utf8");
const r = sh(script);
if (r.err)
    die(`selfhost.sh: status ${r.status}\n${r.err.split("\n").slice(0, 20).join("\n")}`);

// What selfhost.sh builds, which the host built at the same names.
const outputs = [...readFileSync(join(BUILD, "selfhost.sh"), "utf8")
                     .matchAll(/(?:-o|qcs?) \$O\/(\S+\.(?:o|a|wasm))/g)].map((m) => m[1])
                    .filter((f, k, all) => all.indexOf(f) === k);
// SELFHOST_OUT=<dir> keeps what Braam built, to be compared by hand.
if (process.env.SELFHOST_OUT)
    for (const f of outputs) {
        const b = H.store.files.get(`${O}/${f}`);
        if (b) {
            mkdirSync(dirname(join(process.env.SELFHOST_OUT, f)), { recursive: true });
            writeFileSync(join(process.env.SELFHOST_OUT, f), b);
        }
    }
const bad = [];
for (const f of outputs) {
    const ours = H.store.files.get(`${O}/${f}`);
    if (!ours)
        bad.push(`${f}: not built on Braam`);
    else if (Buffer.compare(Buffer.from(ours), readFileSync(join(BUILD, f))) !== 0)
        bad.push(`${f}: differs`);
}
if (bad.length)
    die(`${bad.length} of ${outputs.length} differ:\n` + bad.join("\n"));

// The self-built compiler in place of the package's.
const lib = `${store}/share/vcc/wasm32-braam/lib`;
for (const [from, to] of [["vcc.wasm", "bin/cc"], ["cpp.wasm", "libexec/vcpp"],
                          ["parse.wasm", "libexec/vparse"], ["lower.wasm", "libexec/vlower"],
                          ["genwasm.wasm", "libexec/vgenwasm"]])
    H.store.files.set(`${store}/${to}`, H.store.files.get(`${O}/${from}`));
for (const f of ["crt0.o", "libc.a"])
    H.store.files.set(`${lib}/${f}`, H.store.files.get(`${O}/lib/${f}`));
const book = check_suites(session, die, "book", ...BOOK);

console.log(`selfhost ok: ${outputs.length} objects, libc.a and programs as the host built ` +
            `them; the self-built compiler: ${book.replace(/^book ok: /, "")}`);
