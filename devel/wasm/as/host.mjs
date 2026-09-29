// as on the host. The kernel's headers are wasm32's alone, so there is no
// native as: this boots the SDK's harness once and runs as.wasm there as
// often as asked, which is what the tests need for thousands of modules.
//
// As a module: `const as = await assembler(wasm)`, then
// `as.run(args, { "a.s": text })` gives { out, err, status, files }, where
// files are what the run left beside its inputs, by name. Other tools,
// `assembler(wasm, { disasm: path })`, are run by `as.run(args, inputs,
// null, "disasm")`.
//
// As a command: `node host.mjs <as.wasm> <as's arguments>...` plants every
// input under its base name, runs as, and copies the outputs into the
// current directory, or to -o. Its stdout, stderr and status are as's.

import { readFileSync, writeFileSync } from "node:fs";
import { basename, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

export async function assembler(wasm, tools = {}) {
    const H = await import(HARNESS);
    await H.init(KERNEL, ROOTFS);
    H.kernel().init(0);
    let now = 1;
    if (H.run(0) !== -1)
        throw new Error("the harness's shell did not start");
    H.regrid(80, 24, "resize returned no screen descriptor");
    H.store.files.set("/bin/as", new Uint8Array(readFileSync(wasm)));
    for (const [name, path] of Object.entries(tools))
        H.store.files.set(`/bin/${name}`, new Uint8Array(readFileSync(path)));
    const bytes = (b) => typeof b === "string" ? new TextEncoder().encode(b) : new Uint8Array(b);
    const text = (at) => new TextDecoder().decode(H.store.files.get(at) ?? new Uint8Array());
    let n = 0;

    // Each run in a directory of its own, removed after.
    function run(args, inputs = {}, stdin = null, tool = "as") {
        const dir = `/tmp/as${n++}`;
        H.store.dirs.add(dir);
        for (const [name, b] of Object.entries(inputs))
            H.store.files.set(`${dir}/${name}`, bytes(b));
        const quote = (w) => "'" + w.replace(/'/g, "'\\''") + "'";
        const redirect = stdin === null ? "" : ` </tmp/i`;
        if (stdin !== null)
            H.store.files.set("/tmp/i", bytes(stdin));
        H.store.files.set("/tmp/c", bytes(`cd ${dir}; ${tool} ${args.map(quote).join(" ")}` +
                                          `${redirect} >/tmp/o 2>/tmp/e; echo $? >/tmp/s\n`));
        for (const k of ["/tmp/o", "/tmp/e", "/tmp/s"])
            H.store.files.delete(k);
        H.submit("sh /tmp/c", now++);
        if (H.run(now++) !== -1)
            throw new Error(`the harness did not settle after: ${tool} ` + args.join(" "));
        const files = {};
        for (const [at, b] of [...H.store.files])
            if (at.startsWith(dir + "/")) {
                const name = at.slice(dir.length + 1);
                if (!(name in inputs))
                    files[name] = b;
                H.store.files.delete(at);
            }
        H.store.dirs.delete(dir);
        const s = text("/tmp/s");
        return { out: text("/tmp/o"), err: text("/tmp/e"), status: s ? Number(s) : -1, files };
    }

    return { run };
}

async function main() {
    const [wasm, ...args] = process.argv.slice(2);
    if (!wasm) {
        process.stderr.write("usage: host.mjs <as.wasm> <arguments>...\n");
        process.exit(1);
    }
    const inputs = {}, words = [];
    let output = null, options = true;
    for (let i = 0; i < args.length; i++) {
        const a = args[i];
        if (options && a === "--") {
            options = false;
            words.push(a);
        } else if (options && a === "-o" && i + 1 < args.length) {
            output = args[++i];
            words.push("-o", basename(output));
        } else if (options && a.startsWith("-o") && a.length > 2) {
            output = a.slice(2);
            words.push("-o", basename(output));
        } else if ((!options || !a.startsWith("-")) && a !== "") {
            let b;
            try {
                b = readFileSync(a);
            } catch (e) {
                process.stderr.write(`as: error: cannot open ${a}: ${e.code}\n`);
                process.exit(1);
            }
            inputs[basename(a)] = b;
            words.push(basename(a));
        } else {
            words.push(a);
        }
    }
    const as = await assembler(wasm);
    const r = as.run(words, inputs);
    process.stdout.write(r.out);
    process.stderr.write(r.err);
    for (const [name, b] of Object.entries(r.files)) {
        try {
            writeFileSync(output ?? name, b);
        } catch (e) {
            process.stderr.write(`as: error: cannot write ${output ?? name}: ${e.code}\n`);
            process.exit(1);
        }
    }
    process.exit(r.status);
}

if (resolve(process.argv[1] ?? "") === fileURLToPath(import.meta.url))
    await main();
