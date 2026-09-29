// A reader of .wast scripts, as bytes, just enough to find their modules:
// each command's S-expression, and in it each module as the text `as`
// assembles, the text a `quote` spells, or the bytes of a `binary`.

// A node: { list, start, end, items } or { atom, start, end } or
// { string: bytes, start, end }. `start` and `end` are byte offsets.
export function read(src) {
    let i = 0;
    const top = { list: true, start: 0, end: src.length, items: [] };
    const stack = [top];
    const fail = (why) => {
        throw new Error(`${why} at line ${line(src, i)}`);
    };
    const space = (c) => c === 0x20 || c === 0x09 || c === 0x0a || c === 0x0d;
    const stops = new Set([...'()";'].map((c) => c.charCodeAt(0)));
    while (i < src.length) {
        const c = src[i];
        if (space(c)) {
            i++;
        } else if (c === 0x3b && src[i + 1] === 0x3b) { // ;;
            while (i < src.length && src[i] !== 0x0a && src[i] !== 0x0d)
                i++;
        } else if (c === 0x28 && src[i + 1] === 0x3b) { // (;
            let depth = 0;
            do {
                if (src[i] === 0x28 && src[i + 1] === 0x3b)
                    depth++, i += 2;
                else if (src[i] === 0x3b && src[i + 1] === 0x29)
                    depth--, i += 2;
                else if (i >= src.length)
                    fail("unclosed comment");
                else
                    i++;
            } while (depth);
        } else if (c === 0x28) {
            const n = { list: true, start: i++, end: 0, items: [] };
            stack.at(-1).items.push(n);
            stack.push(n);
        } else if (c === 0x29) {
            if (stack.length === 1)
                fail("unbalanced )");
            stack.pop().end = ++i;
        } else if (c === 0x22) {
            const start = i;
            const bytes = string(src, i, fail);
            i = bytes.end;
            stack.at(-1).items.push({ string: bytes.value, start, end: i });
        } else {
            const start = i;
            while (i < src.length && !space(src[i]) && !stops.has(src[i]))
                i++;
            if (i === start) // a lone ;
                i++;
            if (src[i] === 0x22) // an id such as $"x"
                i = string(src, i, fail).end;
            const atom = new TextDecoder().decode(src.subarray(start, i));
            stack.at(-1).items.push({ atom, start, end: i });
        }
    }
    if (stack.length !== 1)
        fail("unclosed (");
    return top.items;
}

// A string literal at `i`: its bytes, and where it ends.
function string(src, i, fail) {
    const out = [];
    const hex = (c) => /[0-9a-fA-F]/.test(String.fromCharCode(c));
    for (i++;; i++) {
        if (i >= src.length)
            fail("unclosed string");
        const c = src[i];
        if (c === 0x22)
            return { value: new Uint8Array(out), end: i + 1 };
        if (c !== 0x5c) {
            out.push(c);
            continue;
        }
        const e = String.fromCharCode(src[++i]);
        if (e === "t")
            out.push(9);
        else if (e === "n")
            out.push(10);
        else if (e === "r")
            out.push(13);
        else if (e === '"' || e === "'" || e === "\\")
            out.push(e.charCodeAt(0));
        else if (e === "u") {
            let j = i + 2, v = "";
            for (; src[j] !== 0x7d; j++)
                v += String.fromCharCode(src[j]);
            out.push(...new TextEncoder().encode(String.fromCodePoint(parseInt(v.replace(/_/g, ""), 16))));
            i = j;
        } else if (hex(src[i]) && hex(src[i + 1])) {
            out.push(parseInt(e + String.fromCharCode(src[++i]), 16));
        } else {
            fail("illegal escape");
        }
    }
}

export function line(src, at) {
    let n = 1;
    for (let k = 0; k < at && k < src.length; k++)
        if (src[k] === 0x0a)
            n++;
    return n;
}

function concat(parts) {
    const out = new Uint8Array(parts.reduce((n, p) => n + p.length, 0));
    let k = 0;
    for (const p of parts) {
        out.set(p, k);
        k += p.length;
    }
    return out;
}

// A module form: { kind: "text" | "quote" | "binary", bytes }, or null for
// what is not one, such as `(module instance …)`.
export function module(src, n) {
    if (!n.list || n.items[0]?.atom !== "module")
        return null;
    let k = 1;
    const def = n.items[k]?.atom === "definition" ? n.items[k] : null;
    if (def)
        k++;
    if (n.items[k]?.atom === "instance")
        return null;
    if (n.items[k]?.atom?.startsWith("$"))
        k++;
    const kind = n.items[k]?.atom;
    if (kind === "quote" || kind === "binary")
        return { kind, bytes: concat(n.items.slice(k + 1).map((s) => s.string)) };
    const bytes = def ? concat([src.subarray(n.start, def.start), src.subarray(def.end, n.end)])
                      : src.slice(n.start, n.end);
    return { kind: "text", bytes };
}

// Every module of a script: { command, kind, bytes, message, line }, where
// `command` is `module` or the assertion it stands in, and `message` the
// assertion's expected text.
export function modules(src) {
    const out = [];
    for (const cmd of read(src)) {
        if (!cmd.list)
            continue;
        const head = cmd.items[0]?.atom;
        const m = module(src, cmd);
        if (m) {
            out.push({ command: "module", ...m, message: null, line: line(src, cmd.start) });
            continue;
        }
        if (!head?.startsWith("assert_"))
            continue;
        const inner = cmd.items[1] && module(src, cmd.items[1]);
        if (!inner)
            continue;
        const last = cmd.items.at(-1);
        const message = last.string ? new TextDecoder().decode(last.string) : null;
        out.push({ command: head, ...inner, message, line: line(src, cmd.start) });
    }
    return out;
}
