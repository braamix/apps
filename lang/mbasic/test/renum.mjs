// RENUM: every reference it rewrites, everything it must leave alone, and the
// three ways it refuses.

import { boot, session, body, golden, ok } from "./mblib.mjs";

await boot("renum");

const out = body(session([
    // The whole reference set in one program: GOTO, GO TO, GOSUB, THEN n,
    // IF..THEN GOTO n, ON x GOTO a,b,c and RUN n.
    '10 X=1',
    '20 IF X=1 THEN 40',
    '30 GO TO 20',
    '40 ON X GOSUB 60,70',
    '50 IF X=1 THEN GOTO 80',
    '60 PRINT "SIXTY":RETURN',
    '70 PRINT "SEVENTY":RETURN',
    '80 GOTO 90',
    '90 END',
    'RENUM',
    'LIST',
    'RUN',
    // Each argument position, including the omitted middle.
    'RENUM 100',
    'LIST 100-130',
    // From line 130 on only: 100..120 keep their numbers.
    'RENUM 200,130,5',
    'LIST',
    'RENUM 1000,,25',
    'LIST',
    'RUN',
    // Data that looks like a reference and is not, and two literals ahead of
    // one that is -- a string arm that stopped at an opening quote instead of
    // a closing one swallowed the THEN and everything after it.
    'NEW',
    '10 DATA 100,"a:b":READ A,B$:PRINT A;B$',
    '20 PRINT "GOTO 10"',
    '30 REM GOTO 10 : GOSUB 20',
    '40 IF "A"<"B" AND "C"<"D" THEN 60',
    '50 PRINT "SKIPPED"',
    '60 END',
    'RENUM 1,,1',
    'LIST',
    'RUN',
    // A dangling reference: reported, left as typed, the rest renumbered.
    'NEW',
    '10 GOTO 999',
    '20 ON 1 GOTO 10,888',
    '30 END',
    'RENUM 500,,10',
    'LIST',
    // The three refusals.
    'NEW',
    '10 PRINT 1',
    '20 PRINT 2',
    '30 PRINT 3',
    'RENUM 15,30',      // would put 15 after 20
    'RENUM 1,1,0',      // a zero increment
    'RENUM 63990,,100', // past 63999
    'LIST',
    // CONT after a RENUM: the program moved under it.
    'NEW',
    '10 STOP',
    '20 PRINT "ON"',
    'RUN',
    'RENUM 100',
    'CONT',
]));

golden("renum.log", out);
ok();
