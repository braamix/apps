// The word rules stated once, the way case.mjs states the case rules: a
// reserved word is a word, so it does not begin after a letter and does not
// precede one. Upstream's matched anywhere, and `total` was TO + "tal".
//
// What that costs is here too -- a keyword must be free-standing now -- and so
// is what it does not cost, which is every boundary that is not a letter.

import { boot, session, body, golden, die, ok } from "./mblib.mjs";

await boot("words");

const out = body(session([
    // The fourteen names the Manual used to list as unusable, and SYSTEM,
    // which was SYS + TEM. MONEY and MONTH are left in separate groups: they
    // are one variable, MO, which is the two-character rule and not this one.
    'TOTAL=1:STORE=2:MONEY=3:WRONG=4:SORT=5',
    'PRINT TOTAL;STORE;MONEY;WRONG;SORT',
    'WORD=6:LAND=7:RANDOM=8:LETTERS=9:MONTH=10',
    'PRINT WORD;LAND;RANDOM;LETTERS;MONTH',
    'ALREADY=11:SINE=12:USING=13:POSITIVE=14:SYSTEM=15',
    'PRINT ALREADY;SINE;USING;POSITIVE;SYSTEM',
    // A name is still two characters, so these are one variable -- the rule
    // did not change, only where a keyword may be found.
    'TOTAL=99:PRINT TOTIENT',
    // Boundaries that are not letters still crunch, which is the whole reason
    // the guard tests isletc and not "not alphanumeric".
    '10 FOR I=1TO3:PRINT1;I;:NEXT I',
    '20 IF 5AND3 THEN GOTO40',
    '30 PRINT "NOT REACHED"',
    '40 PRINT "AT 40"',
    'RUN',
    'LIST',
    // The entries that are not all letters: punctuation at one end or both.
    'NEW',
    'PRINT STR$(12);TAB(4);"x";SPC(2);"y"',
    'PRINT LEFT$("abcd",2);MID$("abcd",2,2);CHR$(65)',
    'PRINT 3>2;NOT 0;5 OR 2',
    // FN is exempt from the trailing guard: a name always follows it.
    '10 DEF FNS(Q)=Q*Q:PRINT FNS(4);',
    '20 DEF FN T(Q)=Q+Q:PRINT FN T(4)',
    'RUN',
    'LIST',
    // LIST must stay the exact inverse: a name holding a reserved word, and
    // an FN call, both come back as typed.
    'NEW',
    '10 TOTAL=1:POSITIVE=2:PRINT TOTAL+POSITIVE',
    '20 DEF FNA(X)=X+1:PRINT FNA(1)',
    'LIST',
    'RUN',
    // What the rule costs: a keyword abutting a letter is now a name, so the
    // first two are errors and the third reads the variable AA, not A AND B.
    'NEW',
    'FORI=1TO5',
    'PRINTA',
    'A=1:B=1:PRINT AANDB',
    // GO TO still needs its space, and GOTO still wins over GO.
    '10 GO TO 30',
    '20 PRINT "SKIPPED"',
    '30 PRINT "AT 30"',
    'LIST',
    'RUN',
]));

golden("words.log", out);
ok();
