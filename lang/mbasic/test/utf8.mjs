// UTF-8: a line is bytes, a character is a codepoint, and a column is one of
// those. A token and a UTF-8 byte are both >= 0x80, so LIST used to expand
// "café" into "caflenstep" and SAVE wrote that to disk -- this is the case
// that says the two are separated now.
//
// The harness needs nothing special: session() encodes its lines as UTF-8 and
// decodes the transcript back, and the grid holds one codepoint per cell.

import { boot, session, body, golden, get, die, ok, H } from "./mblib.mjs";

await boot("utf8");

const out = body(session([
    // The three regions CRUNCH copies verbatim, which are the three LIST used
    // to mangle. Listed, run, saved and loaded back.
    '10 A$ = "café"',
    '20 REM naïve — dash : gosub 99',
    '30 DATA café, 2 : READ D$, D : PRINT D$; D',
    '40 PRINT A$',
    'LIST',
    'RUN',
    'SAVE "/tmp/u.bas"',
    'NEW',
    'LOAD "/tmp/u.bas"',
    'LIST',
    'RUN',
    // Characters, not bytes. café is four; 日本語 is three and nine bytes.
    'NEW',
    '10 A$="café" : B$="日本語"',
    '20 PRINT LEN(A$); LEN(B$)',
    '30 PRINT LEFT$(A$,3);"|";MID$(A$,4,1);"|";RIGHT$(A$,2)',
    '40 PRINT LEFT$(B$,1);"|";MID$(B$,2,1);"|";RIGHT$(B$,1)',
    '50 PRINT ASC(A$); ASC(B$); ASC("é")',
    '60 PRINT CHR$(233); CHR$(26085); CHR$(65)',
    '70 PRINT LEN(CHR$(26085))',
    'RUN',
    // CHR$ reaches the whole range now; past it is ?Illegal quantity.
    'PRINT CHR$(1114112)',
    'PRINT CHR$(-1)',
    // Columns are characters, so these two put the X in the same place.
    'PRINT "héllo";TAB(10);"X"',
    'PRINT "hello";TAB(10);"X"',
    'PRINT "日本";TAB(10);"X"',
    'PRINT "ab";SPC(3);"X"',
    'PRINT "日本",  "z"',
    // A keyword is still a keyword; non-ASCII outside a literal is not a name.
    '80 café = 1',
    'PRINT POS(0)',
]));

if (out.includes("café") === false) die(`the round trip lost the text:\n${out}`);
golden("utf8.log", out);

// The saved file is what LIST printed, so it must hold the bytes too.
const saved = get("/tmp/u.bas");
if (!saved.includes('"café"') || !saved.includes("naïve — dash") || !saved.includes("data café"))
    die(`SAVE mangled the text:\n${JSON.stringify(saved)}`);

// Malformed input cannot arrive through put(), which encodes -- plant the
// bytes raw, the way editors/vi/test/viutf8.mjs does. utf8_decode answers
// U+FFFD for each, so LEN counts them and nothing is swallowed.
H.store.files.set("/tmp/bad.bas", new Uint8Array([
    ...new TextEncoder().encode('10 A$="a'), 0xff, 0x62, // a stray 0xFF
    ...new TextEncoder().encode('"\n20 PRINT LEN(A$)\n'),
]));
const bad = body(session(['LOAD "/tmp/bad.bas"', "RUN", "LIST"]));
if (!bad.includes(" 3 ")) die(`a malformed byte did not count as one character:\n${bad}`);
if (!bad.includes("�")) die(`a malformed byte did not render as U+FFFD:\n${bad}`);

ok();
