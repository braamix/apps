// FOUT's whole format table, and PRINT's column machinery.
//
// The arithmetic is IEEE double, but every rule about what comes out is
// upstream's: 9 significant digits, a leading space or '-', fixed notation for
// 0.01 <= |x| < 1e9 and E notation outside it, ".5" and never "0.5", trailing
// zeros stripped, an always-present exponent sign and exactly two digits.

import { boot, session, body, golden, die, ok } from "./mblib.mjs";

await boot("numbers");

const out = body(session([
    'PRINT 0;1;-1;.5;-.5',
    'PRINT 1/3;2/3;1/7',
    // The two boundaries of fixed notation, from either side.
    'PRINT .01;.0099999;999999999;1000000000',
    'PRINT 1E10;1.23456789E-5;-1E-10;2^30',
    // Nine significant digits, and the rounding at the ninth.
    'PRINT 123456789;1234567891;.123456789',
    'PRINT 100/3;1000000/7',
    'PRINT INT(-2.5);INT(2.5);-2^2;0^0',
    'PRINT SQR(2);EXP(1);LOG(10);ATN(1)*4',
    'PRINT SIN(0);COS(0);TAN(0);SGN(-4);ABS(-4)',
    // The comma zones are CLMWID = 14 wide, and NCMWID is where they stop.
    'PRINT 1,2,3,4',
    'PRINT "A";TAB(10);"B";SPC(3);"C"',
    'PRINT TAB(3);"X"',
    // TAB to a column already passed does nothing rather than wrapping.
    'PRINT "LONGER";TAB(2);"Y"',
    // Integer variables go through AYINT, whose range is the whole of i16 --
    // upstream rejected exactly -32768 (13-porting-notes.md §3.2).
    'A%=-32768:PRINT A%;NOT 32767;-32768 AND -1',
    'PRINT NOT 0;5 AND 3;5 OR 2;(1<2);(1>2);(1<=1)',
    // RND is deterministic from a fixed seed, as upstream's was.
    'PRINT RND(1);RND(1);RND(0)',
]));

if (out.includes("ERROR")) die(`the session raised an error:\n${out}`);
golden("numbers.log", out);
ok();
