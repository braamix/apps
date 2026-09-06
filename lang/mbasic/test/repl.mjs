// The editor loop and a program that exercises most of the language: typing
// lines, LIST as the exact inverse of CRUNCH, RUN, and NEW.

import { boot, session, body, golden, die, ok } from "./mblib.mjs";

await boot("repl");

const out = body(session([
    // Typed out of order and with a line replaced, so the editor's insert,
    // delete and replace all run. Typing any program line clears the
    // variables, which is why nothing here depends on one surviving.
    '30 FOR I=2 TO N',
    '10 REM SIEVE',
    '60 NEXT I',
    '20 N=30:DIM P(30)',
    '40 IF P(I)<>0 THEN 60',
    '50 PRINT I;',
    // A FOR body always runs once -- the test is at NEXT -- so I*I past N
    // would mark P(121) and raise ?BS. That is upstream's rule, not a bug.
    '55 IF I*I>N THEN 60',
    '57 FOR J=I*I TO N STEP I:P(J)=1:NEXT J',
    '70 PRINT',
    '45 REM MARK THE MULTIPLES',
    '45',              // a bare line number deletes
    'LIST',
    'RUN',
    // Strings, and the functions over them.
    'NEW',
    '10 A$="HELLO":B$=A$+", "+"WORLD"',
    '20 PRINT B$;LEN(B$)',
    '30 PRINT LEFT$(B$,5);"|";MID$(B$,8,5);"|";RIGHT$(B$,5)',
    '40 PRINT ASC(A$);CHR$(66);VAL("3.5")+1;STR$(-7)',
    '50 IF A$<B$ THEN PRINT "LESS"',
    'RUN',
    // GOSUB through an open FOR, and the DEF FN whose parameter is an
    // ordinary variable saved and restored around the call.
    'NEW',
    '10 DEF FNS(X)=X*X+1',
    '20 X=99',
    '30 FOR I=1 TO 3:GOSUB 100:NEXT I',
    '40 PRINT "X IS";X',
    '50 END',
    '100 PRINT I;FNS(I):RETURN',
    'RUN',
    'LIST 30-40',
    // The statements that keep their shape without their machine, and the
    // ones whose arguments PARCHK has already evaluated by the time the
    // function runs.
    'NEW',
    '10 DIM S$(3):S$(1)="ONE":S$(2)="TWO":PRINT S$(1);"/";S$(2)',
    '20 DATA 5,6:READ A,B:PRINT A+B:RESTORE:READ C:PRINT C',
    '30 POKE 100,65:PRINT PEEK(100);CHR$(PEEK(100));POS(0)',
    // An ON index of 0, or one past the end of the list, falls through with
    // no error; CLEAR discards every variable.
    '40 ON 1 GOSUB 100:ON 3 GOSUB 100:ON 0 GOSUB 100',
    '50 X=7:CLEAR:PRINT X',
    '60 A%=5:A=1.7:A$="S":PRINT A%;A;A$',
    '70 END',
    '100 PRINT "SUB":RETURN',
    'RUN',
]));

if (out.includes("ERROR")) die(`the session raised an error:\n${out}`);
golden("repl.log", out);
ok();
