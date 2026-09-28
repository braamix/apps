// Demangling. Every symbol name in the SDK's archives and the fixtures'
// objects, through `ld --dump-demangle` on Braam, must read as llvm-cxxfilt
// reads it: both are LLVM's Itanium demangler, one of them ported.

import { execFileSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { boot, die, get, manifest, ok, plant, run } from "./wasmlib.mjs";

const m = manifest();
const bin = dirname(m.objdump);
const files = new Set();
for (const fx of Object.values(m.fixtures))
    for (const f of [...fx.objects, ...fx.archives, ...fx.libs])
        files.add(f);
const names = new Set();
for (const f of files)
    for (const n of execFileSync(join(bin, "llvm-nm"), ["-j", f], { encoding: "utf8" }).split("\n"))
        if (n)
            names.add(n);
const list = [...names].sort();
const want = execFileSync(join(bin, "llvm-cxxfilt"), { input: list.join("\n") + "\n",
                                                       encoding: "utf8" });

const H = await boot();
plant(H, "/bin/ld", new Uint8Array(readFileSync(m.ld)));
plant(H, "/tmp/names", list.join("\n") + "\n");
run(H, "cd /tmp; ld --dump-demangle names >o 2>e; echo $? >s");
if (get(H, "/tmp/s") !== "0\n")
    die(`ld fails: ${get(H, "/tmp/e")}`);
const got = get(H, "/tmp/o").split("\n"), exp = want.split("\n");
const bad = [];
for (let i = 0; i < list.length && bad.length < 10; i++)
    if (got[i] !== exp[i])
        bad.push(`${list[i]}\n  ld:          ${got[i]}\n  llvm-cxxfilt: ${exp[i]}`);
if (bad.length)
    die("\n" + bad.join("\n"));
const mangled = list.filter((n) => n.startsWith("_Z")).length;
ok(`${list.length} names read as llvm-cxxfilt reads them, ${mangled} of them mangled`);
