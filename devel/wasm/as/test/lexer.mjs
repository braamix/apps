// as's lexer, through `as --tokens`. First a crafted file, held to a golden
// file (BLESS=1 writes it). Then crafted errors, each with its message and
// place. test/parser.mjs runs the suite through the lexer and the parser.

import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { assembler } from "../host.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const AS = join(HERE, "../../../../build/devel/wasm/as/as.wasm");

function die(msg) {
    console.error("lexer: " + msg);
    process.exit(1);
}

if (!existsSync(AS))
    die(`no ${AS} — run make`);
const as = await assembler(AS);
const bad = [];

// ------------------------------------------------------------ the golden dump

{
    const golden = join(HERE, "lexer.golden");
    const r = as.run(["--tokens", "lexer.s"], { "lexer.s": readFileSync(join(HERE, "lexer.s")) });
    if (r.status !== 0 || r.err)
        bad.push(`--tokens lexer.s: status ${r.status}: ${r.err}`);
    else if (process.env.BLESS)
        writeFileSync(golden, r.out);
    else if (r.out !== readFileSync(golden, "utf8")) {
        const x = r.out.split("\n"), y = readFileSync(golden, "utf8").split("\n");
        const i = x.findIndex((l, k) => l !== y[k]);
        bad.push(`--tokens lexer.s: line ${i + 1} is ${JSON.stringify(x[i])}, ` +
                 `golden ${JSON.stringify(y[i])}`);
    }
}

// ------------------------------------------------------------ crafted errors

// [source, line:col: message]; the source as bytes where it is not text.
const ERRORS = [
    ["0$x", "1:1: unknown operator 0$x"],
    ['"a""b"', '1:1: unknown operator "a""b"'],
    ['(data"a")', '1:2: unknown operator data"a"'],
    ["(i32.const 1_)", "1:12: unknown operator 1_"],
    ["0x", "1:1: unknown operator 0x"],
    ["1__0", "1:1: unknown operator 1__0"],
    ["0x_1", "1:1: unknown operator 0x_1"],
    ["_1", "1:1: unknown operator _1"],
    [".5", "1:1: unknown operator .5"],
    ["1e", "1:1: unknown operator 1e"],
    ["0x1p", "1:1: unknown operator 0x1p"],
    ["+", "1:1: unknown operator +"],
    ["A", "1:1: unknown operator A"],
    ["( @a)", "1:3: unknown operator @a"],
    ["a,b", "1:2: unknown operator ,"],
    ["x ;", "1:3: unknown operator ;"],
    ["[", "1:1: unknown operator ["],
    ["{", "1:1: unknown operator {"],
    ["$", "1:1: empty identifier"],
    ["(func $)", "1:7: empty identifier"],
    ['$""', "1:1: empty identifier"],
    ['$"a\nb"', "1:1: empty identifier"],
    ["$(@a)", "1:1: empty identifier"],
    ['$"\\ef"', "1:1: malformed UTF-8 encoding"],
    ['"abc', "1:1: unclosed string literal"],
    ['x\n  "a\nb"', "2:3: unclosed string literal"],
    ['"\\q"', "1:2: illegal escape"],
    ['"ab\\', "1:4: illegal escape"],
    ['"\\u{d800}"', "1:2: illegal escape"],
    ['"\\u{dfff}"', "1:2: illegal escape"],
    ['"\\u{110000}"', "1:2: illegal escape"],
    ['"\\u{}"', "1:2: illegal escape"],
    ['"\\u{_1}"', "1:2: illegal escape"],
    ['"\\u{1234"', "1:2: illegal escape"],
    ['"\\x"', "1:2: illegal escape"],
    ['"\\1"', "1:2: illegal escape"],
    ['"a\tb"', "1:3: illegal control character in string literal"],
    ['"a\x7fb"', "1:3: illegal control character in string literal"],
    [[0x22, 0x61, 0xff, 0x22], "1:3: malformed UTF-8 encoding"],
    [[0x22, 0xc0, 0x80, 0x22], "1:2: malformed UTF-8 encoding"],
    [[0x22, 0xed, 0xa0, 0x80, 0x22], "1:2: malformed UTF-8 encoding"],
    [[0x22, 0xf4, 0x90, 0x80, 0x80, 0x22], "1:2: malformed UTF-8 encoding"],
    ["(; (; ;)", "1:1: unclosed comment"],
    ["x\r\n (;", "2:2: unclosed comment"],
    [[0x3b, 0x3b, 0x20, 0xff], "1:4: malformed UTF-8 encoding"],
    [[0x28, 0x3b, 0xe2, 0x82, 0x3b, 0x29], "1:3: malformed UTF-8 encoding"],
    ["(@", "1:1: empty annotation id"],
    ["(@)", "1:1: empty annotation id"],
    ["(@ x)", "1:1: empty annotation id"],
    ['(@"")', "1:1: empty annotation id"],
    ['(@"\n")', "1:1: empty annotation id"],
    ["(@(@a)x)", "1:1: empty annotation id"],
    ['(@"\\ef")', "1:1: malformed UTF-8 encoding"],
    ['(@a (@"") )', "1:5: empty annotation id"],
    ["(@a (b)", "1:1: unclosed annotation"],
    ["(@a ;; )", "1:1: unclosed annotation"],
    ["(@a (; ) ;)", "1:1: unclosed annotation"],
    ['(@x ")', "1:5: unclosed string literal"],
    ["(@a é)", "1:5: illegal character"],
    ["(@a \x01)", "1:5: illegal character"],
    ["(@a \x7f)", "1:5: illegal character"],
    [[0x28, 0x40, 0x61, 0x20, 0x80, 0x29], "1:5: malformed UTF-8 encoding"],
    ["(@a $)", "1:5: empty identifier"],
    ['(@a $"")', "1:5: empty identifier"],
    ["\x01", "1:1: misplaced control character"],
    ["x\x7f", "1:2: misplaced control character"],
    ["é", "1:1: misplaced unicode character"],
    [[0x20, 0xff], "1:2: malformed UTF-8 encoding"],
];
{
    const inputs = {};
    ERRORS.forEach(([src], k) => {
        inputs[`e${k}.s`] = typeof src === "string" ? src : new Uint8Array(src);
    });
    const r = as.run(["--tokens", ...Object.keys(inputs)], inputs);
    const said = new Map();
    for (const l of r.err.split("\n").filter((l) => l))
        said.set(l.slice(0, l.indexOf(":")), l.slice(l.indexOf(":") + 1));
    ERRORS.forEach(([src, want], k) => {
        const got = said.get(`e${k}.s`);
        const expect = want.replace(/^(\d+:\d+): /, "$1: error: ");
        if (got !== expect)
            bad.push(`${JSON.stringify(src)}: ${got ?? "accepted"}, expected ${expect}`);
    });
}

if (bad.length)
    die(`${bad.length} failures:\n  ` + bad.slice(0, 50).join("\n  "));
console.log(`lexer ok: the golden dump, ${ERRORS.length} errors`);
