// The csources package as a user runs it: installed, made in its store
// directory with the installed cc and make, and checked against the cc package
// byte for byte. Then a second make has nothing to do, and the compiler it
// built compiles and links hello.c from where it is.

import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fixture } from "../../cc/test/ccfix.mjs";

const { die, boot } = fixture("csources", ["devel/make", "devel/csources"]);
const { H, run, sh } = await boot();

let r = run("pkg install csources");
if (r.status !== 0 || !/Installing make /.test(r.out) || !/Installing csources /.test(r.out))
    die(`pkg install csources: status ${r.status}\n${r.out}${r.err}`);

// CSOURCES_OUT=<dir> keeps what make built in obj/, to be compared by hand.
const keep = () => {
    if (!process.env.CSOURCES_OUT)
        return;
    for (const [f, b] of H.store.files) {
        const m = f.match(/^\/pkg\/store\/csources-[^/]+\/obj\/(.*)$/);
        if (m) {
            mkdirSync(dirname(join(process.env.CSOURCES_OUT, m[1])), { recursive: true });
            writeFileSync(join(process.env.CSOURCES_OUT, m[1]), b);
        }
    }
};

const make = (args) => sh(`(cd /pkg/store/csources-*/ && make ${args})`);
r = make("-j4 check");
keep();
if (r.status !== 0 || !/obj\/ is the cc package, byte for byte\.\n$/.test(r.out))
    die(`make check: status ${r.status}\n${r.out.split("\n").slice(-20).join("\n")}\n${r.err}`);
const compiles = (r.out.match(/^cc .* -c -o /gm) ?? []).length;

r = make("");
if (r.status !== 0 || /^(cc|as|ld|ar|cp) /m.test(r.out))
    die(`the second make: status ${r.status}\n${r.out}${r.err}`);

r = sh("(cd /pkg/store/csources-*/obj && bin/cc /pkg/store/cc-*/share/hello.c -o /tmp/hi)");
if (r.status !== 0)
    die(`obj/bin/cc hello.c: status ${r.status}\n${r.out}${r.err}`);
r = run("/tmp/hi csources");
if (r.status !== 0 || r.out !== "Hello, csources!\n")
    die(`hello: ${JSON.stringify(r)}`);

const objs = [...H.store.files.keys()].filter((f) => /^\/pkg\/store\/csources-[^/]+\/obj\/.*\.o$/.test(f));
console.log(`csources ok: ${compiles} compiled by cc, ${objs.length} objects; ` +
            "obj/ is the cc package, byte for byte");
