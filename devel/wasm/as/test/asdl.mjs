// wat.asdl, checked by validate_asdl.py: it must parse, every field's type
// must be defined, no constructor twice, every type reachable from Module.
// Then broken copies of it, each of which the validator must refuse, so a
// check that stops working fails here too. Needs python3 and pyasdl.
//
// Then ast.h against it, by the mapping ast.h states: every type and
// constructor, each field with its C++ type, and nothing else; and every
// constructor named by ast.cpp's printer. Broken copies of ast.h too.

import { spawnSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const DIR = join(dirname(fileURLToPath(import.meta.url)), "..");
const ASDL = join(DIR, "wat.asdl");
const PRIMITIVES = "name,u8,u32,u64,opcode,location";
const tmp = mkdtempSync(join(tmpdir(), "asdl-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));

function validate(file) {
    return spawnSync("python3", [join(DIR, "validate_asdl.py"), "-p", PRIMITIVES, file],
                     { encoding: "utf8" });
}

let bad = 0;
function fail(msg) {
    console.error("asdl: " + msg);
    bad++;
}

const good = validate(ASDL);
if (good.error)
    fail(`cannot run python3: ${good.error.message}`);
else if (good.status !== 0)
    fail(`wat.asdl is refused:\n${good.stdout}${good.stderr}`);
else
    process.stdout.write(good.stdout);

// Each case changes one thing, and the validator must name it.
const text = readFileSync(ASDL, "utf8");
const cases = [
    ["undefined type", "Param* params, ValType* results", "Parm* params, ValType* results",
     "undefined type 'Parm'"],
    ["undefined primitive", "u32 count", "u31 count", "undefined type 'u31'"],
    ["syntax error", "Limits = (u64 min,", "Limits = (u64 min,,", "syntax error"],
    ["duplicate constructor", "| Likely ", "| Unlikely ", "constructor 'Unlikely'"],
    ["duplicate type", "    Expr = (Instr* instrs)",
     "    Expr = (Instr* instrs)\n    Expr = (Instr* more)", "type 'Expr' defined twice"],
    ["unreachable type", "BranchHint? hint)\n", "BranchHint? hint)\n    Lost = Lost\n",
     "type 'Lost' is not reachable"],
];
for (const [what, from, to, expect] of cases) {
    if (!text.includes(from)) {
        fail(`${what}: wat.asdl has no '${from.trim()}' to change`);
        continue;
    }
    const file = join(tmp, what.replace(/ /g, "-") + ".asdl");
    writeFileSync(file, text.replace(from, to));
    const r = validate(file);
    if (r.status === 0)
        fail(`${what}: accepted`);
    else if (!r.stderr.includes(expect))
        fail(`${what}: says\n${r.stderr}not '${expect}'`);
}

// ------------------------------------------------------------ ast.h

// Types ast.h has beyond wat.asdl.
const SUPPORT = new Set(["Loc", "Opt", "List", "Arena"]);
const PRIM = { identifier: "Str", name: "Str", bytes: "Str", opcode: "Opcode",
               location: "Loc", bool: "bool", u8: "u8", u32: "u32", u64: "u64",
               Kind: "Kind" }; // Kind: the tag

// Top-level declarations of ast.h's namespace: {enums, structs, nested},
// structs by name with {members: Map name -> type, kind: [ctor…] or null,
// declared: [nested struct…]}; nested by "X::C".
function parseHeader(text) {
    text = text.replace(/\/\*[^]*?\*\//g, "").replace(/\/\/.*$/gm, "");
    const ns = text.indexOf("namespace wat {");
    if (ns < 0)
        return null;
    text = text.slice(ns + "namespace wat {".length);
    const out = { enums: new Map(), structs: new Map(), nested: new Map(), other: [] };
    const words = str => str.split(",").map(w => w.trim()).filter(w => w);
    let i = 0;
    while (i < text.length) {
        const rest = text.slice(i);
        const m = /^\s*(template\s*<[^>]*>\s*)?(enum class|struct)\s+([\w:]+)(\s*:\s*[\w:]+)?\s*([{;])/
            .exec(rest);
        if (!m) {
            const semi = rest.indexOf(";");
            if (semi < 0)
                break;
            const stmt = rest.slice(0, semi).trim();
            if (stmt && !/^(using |void |})/.test(stmt))
                out.other.push(stmt);
            i += semi + 1;
            continue;
        }
        i += m[0].length;
        if (m[5] === ";")
            continue; // a declaration
        let depth = 1, j = i;
        while (depth && j < text.length)
            depth += text[j] === "{" ? 1 : text[j] === "}" ? -1 : 0, j++;
        const body = text.slice(i, j - 1);
        i = j;
        if (m[2] === "enum class") {
            out.enums.set(m[3], words(body));
            continue;
        }
        const s = { members: new Map(), kind: null, declared: [], base: (m[4] || "").replace(/[:\s]/g, "") };
        const kind = /enum class Kind : u8 \{([^}]*)\};/.exec(body);
        if (kind)
            s.kind = words(kind[1]);
        let flat = body.replace(/enum class Kind : u8 \{[^}]*\};/, "");
        while (/\{[^{}]*\}/.test(flat))
            flat = flat.replace(/\{[^{}]*\}/g, ";");
        for (let stmt of flat.split(";")) {
            stmt = stmt.replace(/^\s*(public|private):/, "").trim();
            if (!stmt || stmt.includes("(") || /^(static|template|using)\b/.test(stmt))
                continue;
            const nest = /^struct (\w+)$/.exec(stmt);
            if (nest) {
                s.declared.push(nest[1]);
                continue;
            }
            const f = /^(.*?)\s*\b(\w+)\s*(=.*)?$/.exec(stmt);
            s.members.set(f[2], f[1].replace(/\s+/g, ""));
        }
        (m[3].includes("::") ? out.nested : out.structs).set(m[3], s);
    }
    return out;
}

// What wat.asdl says ast.h must be, against what it is: a list of errors.
function checkHeader(schema, text, cpp) {
    const errs = [];
    const h = parseHeader(text);
    if (!h)
        return ["ast.h has no namespace wat"];
    const T = schema.types;
    const enumOnly = t => T[t].constructors && !T[t].attributes.length &&
        Object.values(T[t].constructors).every(f => !f.length);
    const product = t => T[t].fields ||
        (Object.keys(T[t].constructors).length === 1 && T[t].constructors[t] && T[t].constructors[t]);
    const isNode = t => T[t].constructors && h.structs.get(t)?.declared.length > 0;
    const cxx = f => {
        let b = PRIM[f.type] ?? f.type;
        if (T[f.type] && isNode(f.type))
            b += "*";
        return f.qualifier === "*" ? `List<${b}>` : f.qualifier === "?" ? `Opt<${b}>` : b;
    };
    // `members` must be exactly `want`, a list of fields.
    const exactly = (where, members, want) => {
        const seen = new Set();
        for (const f of want) {
            const name = [f.name, f.name + "_"].find(n => members.has(n));
            seen.add(name);
            if (!name)
                errs.push(`${where} has no field '${f.name}'`);
            else if (members.get(name) !== cxx(f))
                errs.push(`${where}.${name} is ${members.get(name)}, not ${cxx(f)}`);
        }
        for (const name of members.keys())
            if (!seen.has(name))
                errs.push(`${where} has '${name}', which wat.asdl has not`);
    };
    const kindIs = (where, got, want) => {
        if (JSON.stringify(got) !== JSON.stringify(want))
            errs.push(`${where}'s constructors are ${got?.join(",") ?? "none"}, not ${want.join(",")}`);
    };

    for (const name of Object.keys(T)) {
        const t = T[name];
        if (enumOnly(name) && !product(name)) {
            if (!h.enums.has(name))
                errs.push(`ast.h has no enum class ${name}`);
            else
                kindIs(name, h.enums.get(name), Object.keys(t.constructors));
            continue;
        }
        const s = h.structs.get(name);
        if (!s) {
            errs.push(`ast.h has no struct ${name}`);
            continue;
        }
        const fields = product(name);
        if (fields) {
            if (s.kind)
                errs.push(`${name} is a product, and has a Kind`);
            exactly(name, s.members, [...fields, ...(t.attributes ?? [])]);
            continue;
        }
        const ctors = Object.keys(t.constructors);
        kindIs(name, s.kind, ctors);
        const tag = { name: "kind", type: "Kind", qualifier: "" };
        if (isNode(name)) {
            // A base with the tag and the attributes, and a struct per constructor.
            exactly(name, s.members, [tag, ...t.attributes]);
            kindIs(`${name}'s nested structs`, s.declared, ctors);
            for (const c of ctors) {
                const n = h.nested.get(`${name}::${c}`);
                if (!n)
                    errs.push(`ast.h has no struct ${name}::${c}`);
                else if (n.base !== name)
                    errs.push(`${name}::${c} is not derived from ${name}`);
                else
                    exactly(`${name}::${c}`, n.members, t.constructors[c]);
            }
            continue;
        }
        // Every constructor's fields side by side: one type to a name.
        const all = new Map();
        for (const c of ctors)
            for (const f of t.constructors[c]) {
                if (f.name === "kind")
                    errs.push(`${name}.${c} has a field named kind`);
                const had = all.get(f.name);
                if (had && cxx(had) !== cxx(f))
                    errs.push(`${name}.${f.name} has two types, ${cxx(had)} and ${cxx(f)}`);
                all.set(f.name, f);
            }
        exactly(name, s.members, [tag, ...all.values(), ...t.attributes]);
    }
    for (const name of [...h.enums.keys(), ...h.structs.keys()])
        if (!T[name] && !SUPPORT.has(name))
            errs.push(`ast.h has ${name}, which wat.asdl has not`);
    for (const [name] of h.nested) {
        const [outer, c] = name.split("::");
        if (!T[outer]?.constructors?.[c])
            errs.push(`ast.h has ${name}, which wat.asdl has not`);
    }
    for (const stmt of h.other)
        errs.push(`ast.h has '${stmt}', which is not a type`);

    // The printer names every constructor, but Idx's: a number or an $id.
    for (const name of Object.keys(T))
        if (T[name].constructors && name !== "Idx")
            for (const c of Object.keys(T[name].constructors))
                if (!cpp.includes(`"${c}"`))
                    errs.push(`ast.cpp does not print ${name}.${c}`);
    return errs;
}

const dump = spawnSync("python3", [join(DIR, "validate_asdl.py"), "-j", "-p", PRIMITIVES, ASDL],
                       { encoding: "utf8" });
if (dump.status !== 0) {
    fail(`cannot dump wat.asdl:\n${dump.stdout}${dump.stderr}`);
    process.exit(1);
}
const schema = JSON.parse(dump.stdout);
const header = readFileSync(join(DIR, "ast.h"), "utf8");
const printer = readFileSync(join(DIR, "ast.cpp"), "utf8");
for (const e of checkHeader(schema, header, printer))
    fail(e);

// Each breaks ast.h or ast.cpp one way, and the check must say so.
const broken = [
    ["missing constructor", "        RefNull,\n", "", "Instr's constructors"],
    ["missing type", "struct Local {", "struct Locale {", "no struct Local"],
    ["extra type", "struct Local {", "struct Extra {\n};\n\nstruct Local {", "Extra, which"],
    ["extra field", "    Idx label_out;", "    Idx label_out;\n    u32 spare;", "'spare'"],
    ["wrong type", "    List<Idx> supers;", "    List<u32> supers;", "SubType.supers"],
    ["renamed field", "    bool has_else = false;", "    bool else_ = false;", "no field 'has_else'"],
    ["value sum field", "    TypeUse type;\n    TableType table;", "    TypeUse type;", "no field 'table'"],
    ["enum", "Addr32, Addr64", "Addr64, Addr32", "AddrType's constructors"],
    ["not derived", "struct Instr::Lane : Instr", "struct Instr::Lane : Decl", "not derived"],
    ["missing node", "struct Decl::Start : Decl {", "struct Decl::Begin : Decl {", "Decl::Start"],
];
for (const [what, from, to, expect] of broken) {
    if (!header.includes(from)) {
        fail(`${what}: ast.h has no '${from.trim()}' to change`);
        continue;
    }
    const errs = checkHeader(schema, header.replace(from, to), printer);
    if (!errs.some(e => e.includes(expect)))
        fail(`${what}: says ${JSON.stringify(errs)}, not '${expect}'`);
}
if (!checkHeader(schema, header, printer.replace('"LegacyCatchAll"', '"Legacy"'))
         .some(e => e.includes("does not print LegacyCatch.LegacyCatchAll")))
    fail("printer: a constructor left out is not seen");

if (bad)
    process.exit(1);
console.log(`asdl: ok, and ${cases.length} broken copies refused; ` +
            `ast.h agrees, and ${broken.length + 1} broken copies refused`);
