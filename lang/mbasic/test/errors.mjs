// Every message in ERRTAB, the " IN <line>" that only program mode adds, the
// two INPUT complaints, and the single byte that decides whether CONT works.
//
// LNGERR=1 here, so the messages are spelled out. Their numeric codes are not
// asserted and must never be: 04-interpreter-loop.md §4.3 records that every
// one of them differs between the two ERRTAB shapes, because they are byte
// offsets into unrelated tables.

import { boot, session, body, golden, die, ok } from "./mblib.mjs";

await boot("errors");

const out = body(session([
    'NEXT',
    'RETURN',
    'READ A',
    'PRINT CHR$(-1)',
    'PRINT 1E30*1E30',
    'PRINT ' + "(".repeat(30) + "1" + ")".repeat(30),
    'GOTO 999',
    'DIM Q(2):PRINT Q(9)',
    'DIM Q(2)',
    'PRINT 1/0',
    'INPUT A',
    'PRINT "A"+1',
    'A$="0123456789":A$=A$+A$:A$=A$+A$:A$=A$+A$:A$=A$+A$',
    'PRINT A$+A$',
    'CLOSE 9',
    'A$="A":PRINT A$+(A$+(A$+(A$+(A$+A$))))',
    'CONT',
    'PRINT FNZ(1)',
    'PRINT ((1)',
    // The same errors from a program, where " IN <line>" is added. The test
    // is CURLIN+1 == 255, which MAIN sets the moment it sees a non-empty line.
    '10 PRINT "BEFORE"',
    '20 PRINT 1/0',
    'RUN',
    // STKINI zeroes the byte CONT needs, so an error is not continuable.
    'CONT',
    // ...but a STOP is, and it resumes AFTER the STOP.
    'NEW',
    '10 PRINT "ONE":STOP:PRINT "TWO"',
    '20 PRINT "THREE"',
    'RUN',
    'CONT',
    // And touching the program clears it again.
    'RUN',
    '5 REM',
    'CONT',
]));

golden("errors.log", out);
ok();
