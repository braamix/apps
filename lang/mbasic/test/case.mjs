// The case rules, stated once.
//
// CRUNCH folds what it stores (crunch.cpp), so a keyword and a name may be
// typed in any case and the stored line is canonical lowercase -- which is
// what LIST and SAVE then print. The three things CRUNCH never reaches keep
// the case they were typed in: a string literal, a DATA item, and a REM tail.
// String *data* is not folded either, so a program comparing against "Y" still
// wants a "Y".

import { boot, session, body, golden, put, get, die, ok } from "./mblib.mjs";

await boot("case");

const out = body(session([
    // One keyword whatever the case, and one variable: X and x are the same,
    // and only the first two characters count, so aB and ABcd are too.
    '10 rem Sort the array -- this comment keeps its case',
    '20 print "MiXeD"; : PrInT " kept"',
    '30 x = 3 : X = X + 1 : print X',
    '40 aB = 7 : print ABcd',
    // DATA is verbatim to the next colon, so Alpha keeps its A.
    '50 data Alpha,Beta',
    '60 read a$,B$ : print a$; "/"; b$',
    // FIN takes either exponent letter; FOUT still prints E.
    '70 print 1E5; 1e5; VAL("2e3")',
    // A DEF FN name folds like any other.
    '80 def FNs(q) = q * q : print fns(4); FNS(5)',
    // Mixed case in a string is compared byte for byte -- data is not folded.
    '90 if "Y" = "y" then print "FOLDED" ',
    '95 if "Y" <> "y" then print "String data is not folded"',
    'LIST',
    'RUN',
]));

for (const [what, want] of [
    ["the comment keeps its case", "rem Sort the array"],
    ["LIST lowercases the keywords", "20 print \"MiXeD\"; : print \" kept\""],
    ["the string literal keeps its case", "MiXeD kept"],
    ["X and x are one variable", " 4 "],
    ["only two characters are significant", " 7 "],
    ["DATA keeps its case", "Alpha/Beta"],
    ["either exponent letter parses", " 100000  100000  2000 "],
    ["a DEF FN name folds", " 16  25 "],
    ["string data is not folded", "String data is not folded"],
])
    if (!out.includes(want))
        die(`${what}: no ${JSON.stringify(want)} in\n${out}`);
if (out.includes("error"))
    die(`the session raised an error:\n${out}`);

golden("case.log", out);

// A filename is a string literal, so its case survives the tokenizer. Folding
// it would make this a miss on any case-sensitive filesystem.
put("/tmp/MiXeD.bas", '10 print "Loaded by name"\n');
const named = body(session(['LOAD "/tmp/MiXeD.bas"', 'RUN']));
if (!named.includes("Loaded by name"))
    die(`LOAD folded the filename:\n${named}`);

// SAVE writes what LIST prints, so a program typed in upper case comes back
// canonical -- with its REM tail as typed.
const saved = body(session([
    '10 REM Upper Case Entry',
    '20 FOR I=1 TO 3:PRINT I;:NEXT I',
    'SAVE "/tmp/c.bas"',
]));
if (saved.includes("error"))
    die(`the upper-case spelling did not run:\n${saved}`);
const text = get("/tmp/c.bas");
if (text !== "10 rem Upper Case Entry\n20 for i=1 to 3:print i;:next i\n")
    die(`SAVE did not write canonical text:\n${JSON.stringify(text)}`);

ok();
