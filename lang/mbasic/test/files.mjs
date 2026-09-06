// SAVE and LOAD (DISKO), and the EXTIO channels.
//
// Neither switch was on in the Apple build this port is based on, and both
// renumber every token after the point they insert at -- which is why they had
// to be decided before a line of the tokenizer was written.
//
// A program is LIST-format text: the same bytes a user would type. Upstream's
// KIM cassette had to relink on load because the line links were absolute
// addresses; there is nothing to relink here, only to re-CRUNCH.

import { boot, session, body, golden, get, die, ok } from "./mblib.mjs";

await boot("files");

const out = body(session([
    '10 REM SAVED',
    '20 FOR I=1 TO 3:PRINT I*I;:NEXT I',
    '30 PRINT',
    'SAVE "/tmp/p.bas"',
    'NEW',
    'LIST',
    'LOAD "/tmp/p.bas"',
    'LIST',
    'RUN',
    // The channels. OPEN <n>,<name> reads; a third argument of "W" writes.
    'NEW',
    '10 OPEN 1,"/tmp/d.txt","W"',
    '20 PRINT#1,"ALPHA"',
    '30 PRINT#1,"BETA"',
    '40 CLOSE 1',
    '50 OPEN 2,"/tmp/d.txt"',
    '60 INPUT#2,A$:INPUT#2,B$',
    '70 PRINT "READ BACK: ";A$;" ";B$',
    '80 CLOSE 2',
    'RUN',
    // A file number that was never opened is ?FILE DATA, not ?SYNTAX.
    'PRINT#7,"NO"',
    'CLOSE 7',
]));

const saved = get("/tmp/p.bas");
if (saved !== "10 REM SAVED\n20 FOR I=1 TO 3:PRINT I*I;:NEXT I\n30 PRINT\n")
    die(`SAVE did not write LIST-format text:\n${JSON.stringify(saved)}`);
const data = get("/tmp/d.txt");
if (data !== "ALPHA\r\nBETA\r\n")
    die(`PRINT# did not write what it printed:\n${JSON.stringify(data)}`);

golden("files.log", out);
ok();
