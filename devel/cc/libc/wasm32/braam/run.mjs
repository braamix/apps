// Run a Braam process vcc built, without Braam: node run.mjs prog [args...]
//
// A fake kernel (docs/Braam.md §7.4).  It checks the module as braam-core's
// test/system/abi.mjs does (the imports, the five exports, one braam section), gives it
// env.memory of the section's page counts, writes argv through _alloc and calls _start.
// It serves kernel.sys (Exit, GetPid, Now, Random) and kernel.sys_async for Write (fd 1
// and 2 to stdout and stderr), Read (fd 0 from stdin, in 512-byte chunks), Open, Close,
// Read, Write, Seek and FStat on files, Stat, Remove, MkDir, Chdir and Rename on paths
// (relative to the current directory), Sleep, SigAct and Poll.  Each reply goes
// through _resume from a later macrotask, never from inside sys_async, so a step that
// forgets to return is caught.  The end reports "[exit N]" on stderr and exits with N;
// a trap reports itself and exits with 255, a process that waits on nothing with 254.
//
// Signals: a byte 0x03 on standard input is a ^C, as a terminal makes one (the read
// that reaches it parks, and SIG_INT finds it parked), and node's own SIGINT and
// SIGTERM are passed on.  A signal the process asked for (SigAct) goes to _sig, and
// every call parked on a Read of stdin, a Sleep or a Poll answers Err(Intr); any other
// one ends the process with status 130, as Braam's default action does.
import { closeSync, fstatSync, lstatSync, mkdirSync, openSync, readSync, renameSync,
         rmdirSync, rmSync, statSync, unlinkSync, writeSync } from 'node:fs';
import { readFileSync } from 'node:fs';
import { basename } from 'node:path';
import { isatty } from 'node:tty';

const PROC_MAGIC = 0x6d617262;
const PROC_ABI = 21;
const PROC_MAX_PAGES = 1600;
const SYS = { Exit: 1, GetPid: 2, Now: 3, Random: 5, Write: 16, Read: 17, Open: 18,
              Close: 19, Stat: 20, MkDir: 22, Remove: 23, Chdir: 25, Rename: 29, Seek: 30,
              Sleep: 32, FStat: 33, SigAct: 85, Poll: 86 };
const SIG = { INT: 2, TERM: 15, CONT: 18, WINCH: 28 };
const CATCHABLE = (1 << SIG.INT) | (1 << SIG.TERM) | (1 << SIG.WINCH);
const POLL = { IN: 1, OUT: 2, MAX: 64, FOREVER: 0xffffffff };
const ERR = { Intr: 15, Invalid: 1, NotFound: 3, Exists: 4, NotDir: 5, IsDir: 6, Perm: 7, Io: 8,
              Unsupported: 11, NotEmpty: 13 };
const O = { Read: 1, Write: 2, Create: 4, Trunc: 8, Append: 16, Excl: 32 };
const CHUNK = 512;
const READ_MAX = 65536 - 4;

function fail(why) {
    process.stderr.write(`run.mjs: ${why}\n`);
    process.exit(253);
}

if (process.argv.length < 3)
    fail('usage: node run.mjs prog [args...]');
const path = process.argv[2];
const module = new WebAssembly.Module(readFileSync(path));

// The process ABI, as abi.mjs asserts it.
const names = (list) => list.map((e) => `${e.module ? e.module + '.' : ''}${e.name}`).sort();
const imports = names(WebAssembly.Module.imports(module));
const exports = names(WebAssembly.Module.exports(module));
for (const name of imports)
    if (!['env.memory', 'kernel.sys', 'kernel.sys_async'].includes(name))
        fail(`imports ${name}, which is not the process ABI`);
if (!imports.includes('env.memory'))
    fail('does not import env.memory');
if (exports.join() !== '_alloc,_free,_resume,_sig,_start')
    fail(`exports [${exports}], expected [_alloc,_free,_resume,_sig,_start]`);
const meta = WebAssembly.Module.customSections(module, 'braam');
if (meta.length !== 1)
    fail(`carries ${meta.length} braam sections, expected 1`);
const m = new Uint32Array(meta[0].slice(0, 20));
if (m[0] !== PROC_MAGIC || m[1] !== PROC_ABI)
    fail(`metadata is ${m[0].toString(16)}/${m[1]}`);
if (m[4] !== PROC_MAX_PAGES)
    fail(`asks for ${m[4]} pages, expected ${PROC_MAX_PAGES}`);

const memory = new WebAssembly.Memory({ initial: m[3], maximum: m[4] });
const bytes = () => new Uint8Array(memory.buffer);
const start = Date.now();
let status = null;      // what Sys::Exit said
let finished = false;
let outstanding = 0;    // calls not answered yet
const waiting = new Map(); // token -> cancel(): the parked calls a signal abandons
let caught = 0;         // the signals the process asked for, as bits
let input = new Uint8Array(0); // standard input read and not yet served
const files = new Map(); // descriptor -> { fd: node's, pos, append }
let next_fd = 3;
let x;                  // the instance's exports

// A trap is a crash, as the kernel reports it; exit() makes one after Sys::Exit.
function trap(e) {
    const said = status === null ? '' : ` (Sys::Exit said ${status})`;
    process.stderr.write(`trap: ${e.message}${said}\n`);
    process.exit(255);
}

// After a step: 0 is an exited process.
function stepped(r) {
    if (r === 0) {
        finished = true;
        process.stderr.write(`[exit ${status}]\n`);
        process.exit(status & 255);
    }
}

// The reply to `token`: an i32 status, then `data`, in a block of the process's own.
function reply(token, st, data = new Uint8Array(0)) {
    outstanding++;
    setImmediate(() => {
        outstanding--;
        try {
            const n = 4 + data.length;
            const at = x._alloc(n);
            new DataView(memory.buffer).setInt32(at, st, true);
            bytes().set(data, at + 4);
            stepped(x._resume(token, at, n));
        } catch (e) {
            trap(e);
        }
    });
}

function nodeFlags(flags) {
    const rw = (flags & O.Read) && (flags & O.Write) ? 'r+' : 'r';
    if (!(flags & O.Write))
        return 'r';
    let f = flags & O.Append ? 'a' : flags & O.Trunc || flags & O.Create ? 'w' : rw;
    if (flags & O.Read && f !== 'r+')
        f += '+';
    if (flags & O.Excl)
        f += 'x';
    return f;
}

function errorOf(e) {
    switch (e.code) {
    case 'ENOENT': return -ERR.NotFound;
    case 'EEXIST': return -ERR.Exists;
    case 'ENOTDIR': return -ERR.NotDir;
    case 'EISDIR': return -ERR.IsDir;
    case 'ENOTEMPTY': return -ERR.NotEmpty;
    case 'EACCES': case 'EPERM': return -ERR.Perm;
    default: return -ERR.Io;
    }
}

// Stat's data: u32 kind, u64 size, u64 mtime (milliseconds).
function statData(st) {
    const out = new DataView(new ArrayBuffer(20));
    out.setUint32(0, st.isDirectory() ? 1 : st.isSymbolicLink() ? 2 : 0, true);
    out.setBigUint64(4, BigInt(st.size), true);
    out.setBigUint64(12, BigInt(Math.floor(st.mtimeMs)), true);
    return new Uint8Array(out.buffer);
}

// A call on a path that answers with a status alone.
function onPath(token, fn) {
    try {
        fn();
        return reply(token, 0);
    } catch (e) {
        return reply(token, errorOf(e));
    }
}

function park(token, cancel) {
    outstanding++;
    waiting.set(token, cancel);
}

function unpark(token) {
    if (waiting.delete(token))
        outstanding--;
}

const word = (v) => {
    const out = new DataView(new ArrayBuffer(4));
    out.setUint32(0, v >>> 0, true);
    return new Uint8Array(out.buffer);
};

// A signal: told to the process if it asked, else the default action.
function raise(sig) {
    if (finished)
        return;
    if (caught & (1 << sig)) {
        try {
            x._sig(sig);
        } catch (e) {
            trap(e);
        }
        for (const [token, cancel] of [...waiting]) {
            cancel();
            unpark(token);
            reply(token, -ERR.Intr);
        }
        return;
    }
    if (sig === SIG.WINCH || sig === SIG.CONT)
        return;
    finished = true;
    process.stderr.write(`[exit 130] (signal ${sig})\n`);
    process.exit(130);
}
process.on('SIGINT', () => raise(SIG.INT));
process.on('SIGTERM', () => raise(SIG.TERM));

// A read of standard input: what is buffered up to the next ^C, else a chunk more.
function readInput(token, max) {
    if (input.length === 0) {
        const buf = new Uint8Array(CHUNK);
        let n = 0;
        try {
            n = readSync(0, buf, 0, CHUNK, null);
        } catch (e) {
            if (e.code === 'EAGAIN') { // a terminal or pipe with nothing yet
                const t = setTimeout(() => {
                    unpark(token);
                    readInput(token, max);
                }, 10);
                return park(token, () => clearTimeout(t));
            }
            if (e.code !== 'EOF')
                return reply(token, errorOf(e));
        }
        if (n === 0)
            return reply(token, 0);
        input = buf.subarray(0, n);
    }
    if (input[0] === 3) {
        input = input.subarray(1);
        park(token, () => {});
        return setImmediate(() => {
            raise(SIG.INT);
            if (waiting.has(token)) {
                unpark(token);
                readInput(token, max);
            }
        });
    }
    let n = input.indexOf(3);
    n = Math.min(n < 0 ? input.length : n, max);
    const data = input.slice(0, n);
    input = input.subarray(n);
    return reply(token, 0, data);
}

// Poll's revents for one descriptor, or null for one that is not open.
function ready(fd, events) {
    if (fd === 0)
        return input.length || !isatty(0) ? events & POLL.IN : 0;
    if (fd === 1 || fd === 2)
        return events & POLL.OUT;
    return files.has(fd) ? events & (POLL.IN | POLL.OUT) : null;
}

const kernel = {
    sys(op, a0, a1, a2) {
        switch (op & 0xff) {
        case SYS.Exit: status = a0 | 0; return 0;
        case SYS.GetPid: return 1;
        case SYS.Now: return (Date.now() - start) | 0;
        case SYS.Random: return (Math.random() * 0x100000000) | 0;
        default: return -ERR.Unsupported;
        }
    },
    sys_async(op, token, ptr, len) {
        const code = op & 0xff, arg = op >>> 8;
        const payload = bytes().slice(ptr, ptr + len);
        const u32 = (at) => new DataView(payload.buffer).getUint32(at, true);
        const text = (a, b) => new TextDecoder().decode(payload.subarray(a, b));
        switch (code) {
        case SYS.Write: {
            const f = arg === 1 || arg === 2 ? { fd: arg, pos: null } : files.get(arg);
            if (f === undefined)
                return reply(token, -ERR.Invalid);
            try {
                const n = writeSync(f.fd, payload, 0, payload.length, f.append ? null : f.pos);
                if (f.pos !== null)
                    f.pos = f.append ? fstatSync(f.fd).size : f.pos + n;
                return reply(token, n);
            } catch (e) {
                return reply(token, errorOf(e));
            }
        }
        case SYS.Read: {
            if (arg === 0)
                return readInput(token, Math.min(len >= 4 ? u32(0) : CHUNK, CHUNK));
            const f = files.get(arg);
            if (f === undefined)
                return reply(token, -ERR.Invalid);
            const max = Math.min(len >= 4 ? u32(0) : CHUNK, READ_MAX);
            const buf = new Uint8Array(max);
            try {
                const n = readSync(f.fd, buf, 0, max, f.pos);
                if (f.pos !== null)
                    f.pos += n;
                return reply(token, 0, buf.subarray(0, n));
            } catch (e) {
                return reply(token, e.code === 'EOF' ? 0 : errorOf(e));
            }
        }
        case SYS.Open: {
            try {
                const fd = openSync(text(), nodeFlags(arg));
                files.set(next_fd, { fd, pos: 0, append: Boolean(arg & O.Append) });
                return reply(token, next_fd++);
            } catch (e) {
                return reply(token, errorOf(e));
            }
        }
        case SYS.Close: {
            const f = files.get(arg);
            if (f === undefined)
                return reply(token, arg <= 2 ? 0 : -ERR.Invalid);
            files.delete(arg);
            closeSync(f.fd);
            return reply(token, 0);
        }
        case SYS.Seek: {
            const f = files.get(arg);
            if (f === undefined)
                return reply(token, arg <= 2 ? -ERR.Unsupported : -ERR.Invalid);
            const view = new DataView(payload.buffer);
            const whence = view.getUint32(0, true), offset = Number(view.getBigInt64(4, true));
            const base = whence === 0 ? 0 : whence === 1 ? f.pos : fstatSync(f.fd).size;
            if (whence > 2 || base + offset < 0)
                return reply(token, -ERR.Invalid);
            f.pos = base + offset;
            const out = new DataView(new ArrayBuffer(8));
            out.setBigUint64(0, BigInt(f.pos), true);
            return reply(token, 0, new Uint8Array(out.buffer));
        }
        case SYS.FStat: {
            const f = files.get(arg);
            if (f === undefined)
                return reply(token, arg <= 2 ? -ERR.Unsupported : -ERR.Invalid);
            return reply(token, 0, statData(fstatSync(f.fd)));
        }
        case SYS.Stat:
            try {
                const st = arg & 1 ? lstatSync(text()) : statSync(text());
                return reply(token, 0, statData(st));
            } catch (e) {
                return reply(token, errorOf(e));
            }
        case SYS.MkDir:
            return onPath(token, () => mkdirSync(text()));
        case SYS.Remove:
            return onPath(token, () => {
                const path = text();
                if (!lstatSync(path).isDirectory())
                    unlinkSync(path);
                else if (arg & 1)
                    rmSync(path, { recursive: true });
                else
                    rmdirSync(path);
            });
        case SYS.Rename: {
            const n = u32(0);
            return onPath(token, () => renameSync(text(4, 4 + n), text(4 + n)));
        }
        case SYS.Chdir:
            try {
                if (arg & 1)
                    process.chdir(text());
                return reply(token, 0, new TextEncoder().encode(process.cwd()));
            } catch (e) {
                return reply(token, errorOf(e));
            }
        case SYS.Sleep: {
            const t = setTimeout(() => {
                unpark(token);
                reply(token, 0);
            }, len >= 4 ? u32(0) : 0);
            return park(token, () => clearTimeout(t));
        }
        case SYS.SigAct: {
            const before = caught;
            if (len >= 4) {
                if (u32(0) & ~CATCHABLE)
                    return reply(token, -ERR.Invalid);
                caught = u32(0);
            }
            return reply(token, 0, word(before));
        }
        case SYS.Poll: {
            const n = (len - 4) >>> 3;
            if (len < 4 || n > POLL.MAX)
                return reply(token, -ERR.Invalid);
            const timeout = u32(0), start = Date.now();
            const answer = () => {
                const out = new Uint8Array(4 * n);
                let count = 0;
                for (let i = 0; i < n; i++) {
                    const r = ready(u32(4 + 8 * i), u32(8 + 8 * i));
                    if (r === null)
                        return -ERR.Invalid;
                    out.set(word(r), 4 * i);
                    count += r !== 0;
                }
                if (count === 0 && timeout !== POLL.FOREVER && Date.now() - start < timeout)
                    return null;
                if (count === 0 && timeout === POLL.FOREVER)
                    return null;
                return [count, out];
            };
            const first = answer();
            if (typeof first === 'number')
                return reply(token, first);
            if (first)
                return reply(token, first[0], first[1]);
            const t = setInterval(() => {
                const a = answer();
                if (a === null)
                    return;
                clearInterval(t);
                unpark(token);
                if (typeof a === 'number')
                    reply(token, a);
                else
                    reply(token, a[0], a[1]);
            }, 10);
            return park(token, () => clearInterval(t));
        }
        default:
            return reply(token, -ERR.Unsupported);
        }
    },
};

// A process that is waiting while nothing is coming will never finish.
process.on('beforeExit', () => {
    if (!finished) {
        process.stderr.write(`run.mjs: the process waits on nothing (${outstanding} calls)\n`);
        process.exit(254);
    }
});

try {
    x = new WebAssembly.Instance(module, { env: { memory }, kernel }).exports;
    // argv: u32 argc, then u32 length and the bytes of each word.
    const words = [basename(path), ...process.argv.slice(3)].map((w) => new TextEncoder().encode(w));
    const size = 4 + words.reduce((n, w) => n + 4 + w.length, 0);
    const at = x._alloc(size);
    const view = new DataView(memory.buffer);
    view.setUint32(at, words.length, true);
    let p = at + 4;
    for (const w of words) {
        view.setUint32(p, w.length, true);
        bytes().set(w, p + 4);
        p += 4 + w.length;
    }
    stepped(x._start(at, size));
} catch (e) {
    trap(e);
}
