// The fixture devel/cc's tests share: a boot with the cc package installed as
// a user installs it. A signed index offers the cc and wasm packages, and `pkg
// install cc` pulls in both. The anchor is the fixture's own, over keys made
// for the run, so no publisher's key is needed.

import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, readdirSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { HARNESS, KERNEL, ROOTFS, SDK } from "../../../test/sdk.mjs";

export const APPS = resolve(dirname(fileURLToPath(import.meta.url)), "../../..");
export const TREE = join(APPS, "devel/cc");
export const BUILD = join(APPS, "build/devel/cc");
export const HOST_VCC = join(APPS, "build/host-cc/stage/bin/vcc");
const TOOLS = join(SDK, "libexec/braam");
const URL = "https://braamix.github.io";

export function fixture(name) {
    const die = (msg) => {
        console.error(`${name}: ${msg}`);
        process.exit(1);
    };
    return { die, boot: () => boot(die) };
}

// The two packages, which `make test-packages` packs, and a repository
// vouching for them.
function repository(die) {
    const zipIn = (dir, name) => {
        const z = readdirSync(dir).filter((f) => f.startsWith(name + "-") && f.endsWith(".zip"));
        if (z.length !== 1)
            die(`${dir}: ${z.length} ${name} packages; \`make test-packages\` packs them`);
        return join(dir, z[0]);
    };
    const zips = [zipIn(BUILD, "cc"), zipIn(join(APPS, "build/devel/wasm"), "wasm")];
    const keys = mkdtempSync(join(tmpdir(), "vcc-repo-"));
    const py = (script, args) =>
        execFileSync("python3", [join(TOOLS, script), ...args], { cwd: keys });
    execFileSync("python3", ["-c", "import ed25519; ed25519.generate('root.key'); " +
                             "ed25519.generate('index.key')"],
                 { cwd: keys, env: { ...process.env, PYTHONPATH: TOOLS } });
    py("mkanchor.py", ["--out", "anchor", "--version", "3", "--expiry", "4102444800000",
                       "--threshold", "root=1", "--threshold", "index=1", "--key",
                       "root=root.key", "--key", "index=index.key", "--sign", "root.key"]);
    py("mkindex.py", ["--out", "index", "--url", URL, "--version", "1", "--expiry",
                      "4102444800000", "--sign", "index.key", ...zips]);
    const anchor = readFileSync(join(keys, "anchor"));
    const index = readFileSync(join(keys, "index"));
    rmSync(keys, { recursive: true });
    return { zips, anchor, index };
}

// Boots, installs cc, and leaves the shell in /tmp. What it returns:
//   put(path, bytes|string)  plants a file, and its directories
//   text(path)               a file's text, "" when there is none
//   submit(line, timers)     types a line and runs the kernel until it idles
//   run(cmd)                 {out, err, status} of a command, run from /tmp
//   sh(script)               the same of a script, which has no length limit
//   store                    the cc package's store directory
async function boot(die) {
    const { zips, anchor, index } = repository(die);
    const H = await import(HARNESS);
    await H.init(KERNEL, ROOTFS);
    H.kernel().init(0);
    if (H.run(0) !== -1)
        die("the shell did not park on the keyboard");
    H.regrid(120, 40, "resize returned no screen descriptor");

    const put = (path, bytes) => {
        H.store.files.set(path,
                          typeof bytes === "string" ? new TextEncoder().encode(bytes) : bytes);
        for (let d = dirname(path); d !== "/"; d = dirname(d))
            H.store.dirs.add(d);
    };
    const text = (path) =>
        new TextDecoder().decode(H.store.files.get(path) ?? new Uint8Array());

    // The kernel runs until it idles; a timer it is waiting for is the clock
    // moved on to when it falls due.
    let now = 1;
    const settle = (what) => {
        for (let k = 0, d = H.run(now++); d !== -1; k++, d = H.run(now)) {
            if (k === 10000)
                die("the kernel did not settle after: " + what);
            now += d;
        }
    };
    // timers = false runs the kernel once and leaves the clock where it is,
    // for a program that sleeps for ever.
    const submit = (line, timers = true) => {
        if (line.length > 60)
            die(`the command line is ${line.length} keys, and the ring holds 64`);
        H.submit(line, now++);
        if (timers)
            settle(line);
        else
            H.run(now++);
    };

    // A command's stdout, stderr and status, by way of three files: short
    // names, as a command line is at most sixty keys.
    let cwd = "/home";
    const result = () => ({ out: text(cwd + "/o"), err: text(cwd + "/e"),
                            status: Number(text(cwd + "/s").trim()) });
    const run = (cmd) => {
        submit(`${cmd} >o 2>e; echo $? >s`);
        return result();
    };
    const sh = (script) => {
        put("/tmp/c.sh", `{\n${script}\n} >o 2>e\necho $? >s\n`);
        submit("sh /tmp/c.sh");
        return result();
    };

    put("/etc/anchor", new Uint8Array(anchor));
    put("/etc/repositories", URL + "\n");
    H.net.routes.set(URL + "/index", { status: 200, headers: "content-type: text/plain\n",
                                       body: new Uint8Array(index) });
    for (const z of zips)
        H.net.routes.set(`${URL}/${z.slice(z.lastIndexOf("/") + 1)}`,
                         { status: 200, headers: "content-type: application/zip\n",
                           body: new Uint8Array(readFileSync(z)) });
    let r = run("pkg update");
    if (r.status !== 0)
        die(`pkg update: status ${r.status}\n${r.out}${r.err}`);
    r = run("pkg install cc");
    if (r.status !== 0 || !/Installing wasm /.test(r.out) || !/Installing cc /.test(r.out))
        die(`pkg install cc: status ${r.status}\n${r.out}${r.err}`);
    for (const c of ["cc", "as", "ld"])
        if (!H.store.files.has("/pkg/gen/1/bin/" + c))
            die(`no ${c} in /pkg/bin`);
    submit("cd /tmp");
    cwd = "/tmp";

    const store = "/pkg/store/" + zips[0].slice(zips[0].lastIndexOf("/") + 1, -4);
    return { H, put, text, submit, run, sh, store };
}
