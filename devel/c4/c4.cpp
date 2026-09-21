// c4.c - C in four functions
//
// char, int, and pointer types
// if, while, return, and expression statements
// just enough features to allow self-compilation and a bit more
//
// Written by Robert Swierczek
//
// The four functions are upstream's. printf writes a buffer the driver drains;
// exit() is a sticky flag; OPEN/READ/CLOS leave the burst so braam.cpp can
// await them. #define int long long is upstream's: the VM word is 8 bytes.

#include "c4.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

int c4_outf(const char *fmt, ...);
int c4_format(const char *fmt, const long long *args);

char *c4_obuf;
int c4_olen;
C4Sys c4_sys;
long long c4_a;
long long c4_cycle;
long long c4_status;

static int ocap;

#define int    long long
#define printf c4_outf
#define exit(s)       \
    do {              \
        done   = 1;   \
        status = (s); \
        return;       \
    } while (0)

char *p, *lp, // current position in source code
    *data;    // data/bss pointer

int *e, *le, // current position in emitted code
    *id,     // currently parsed identifier
    *sym,    // symbol table (simple list of identifiers)
    tk,      // current token
    ival,    // current token value
    ty,      // current expression type
    loc,     // local variable offset
    line,    // current line number
    src,     // print source and assembly flag
    debug;   // print executed instructions

int *idmain;
int *pc, *sp, *bp, a, cycle;
int poolsz;
int done, status;

// tokens and classes (operators last and in precedence order)
enum {
    Num = 128,
    Fun,
    Sys,
    Glo,
    Loc,
    Id,
    Char,
    Else,
    Enum,
    If,
    Int,
    Return,
    Sizeof,
    While,
    Assign,
    Cond,
    Lor,
    Lan,
    Or,
    Xor,
    And,
    Eq,
    Ne,
    Lt,
    Gt,
    Le,
    Ge,
    Shl,
    Shr,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Inc,
    Dec,
    Brak
};

// opcodes
enum {
    LEA,
    IMM,
    JMP,
    JSR,
    BZ,
    BNZ,
    ENT,
    ADJ,
    LEV,
    LI,
    LC,
    SI,
    SC,
    PSH,
    OR,
    XOR,
    AND,
    EQ,
    NE,
    LT,
    GT,
    LE,
    GE,
    SHL,
    SHR,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    OPEN,
    READ,
    CLOS,
    PRTF,
    MALC,
    FREE,
    MSET,
    MCMP,
    EXIT
};

// types
enum { CHAR, INT, PTR };

// identifier offsets (since we can't create an ident struct)
enum { Tk, Hash, Name, Class, Type, Val, HClass, HType, HVal, Idsz };

static const char *ops =
    "LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,"
    "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
    "OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,EXIT,";

void next()
{
    char *pp;

    while ((tk = *p)) {
        ++p;
        if (tk == '\n') {
            if (src) {
                printf("%d: %.*s", line, (int)(p - lp), lp);
                lp = p;
                while (le < e) {
                    printf("%8.4s", &ops[*++le * 5]);
                    if (*le <= ADJ)
                        printf(" %d\n", *++le);
                    else
                        printf("\n");
                }
            }
            ++line;
        } else if (tk == '#') {
            while (*p != 0 && *p != '\n')
                ++p;
        } else if ((tk >= 'a' && tk <= 'z') || (tk >= 'A' && tk <= 'Z') || tk == '_') {
            pp = p - 1;
            while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                   (*p >= '0' && *p <= '9') || *p == '_')
                tk = tk * 147 + *p++;
            tk = (tk << 6) + (p - pp);
            id = sym;
            while (id[Tk]) {
                if (tk == id[Hash] && !memcmp((char *)id[Name], pp, p - pp)) {
                    tk = id[Tk];
                    return;
                }
                id = id + Idsz;
            }
            id[Name] = (int)pp;
            id[Hash] = tk;
            tk = id[Tk] = Id;
            return;
        } else if (tk >= '0' && tk <= '9') {
            if ((ival = tk - '0')) {
                while (*p >= '0' && *p <= '9')
                    ival = ival * 10 + *p++ - '0';
            } else if (*p == 'x' || *p == 'X') {
                while ((tk = *++p) && ((tk >= '0' && tk <= '9') || (tk >= 'a' && tk <= 'f') ||
                                       (tk >= 'A' && tk <= 'F')))
                    ival = ival * 16 + (tk & 15) + (tk >= 'A' ? 9 : 0);
            } else {
                while (*p >= '0' && *p <= '7')
                    ival = ival * 8 + *p++ - '0';
            }
            tk = Num;
            return;
        } else if (tk == '/') {
            if (*p == '/') {
                ++p;
                while (*p != 0 && *p != '\n')
                    ++p;
            } else {
                tk = Div;
                return;
            }
        } else if (tk == '\'' || tk == '"') {
            pp = data;
            while (*p != 0 && *p != tk) {
                if ((ival = *p++) == '\\') {
                    if ((ival = *p++) == 'n')
                        ival = '\n';
                }
                if (tk == '"')
                    *data++ = ival;
            }
            ++p;
            if (tk == '"')
                ival = (int)pp;
            else
                tk = Num;
            return;
        } else if (tk == '=') {
            if (*p == '=') {
                ++p;
                tk = Eq;
            } else
                tk = Assign;
            return;
        } else if (tk == '+') {
            if (*p == '+') {
                ++p;
                tk = Inc;
            } else
                tk = Add;
            return;
        } else if (tk == '-') {
            if (*p == '-') {
                ++p;
                tk = Dec;
            } else
                tk = Sub;
            return;
        } else if (tk == '!') {
            if (*p == '=') {
                ++p;
                tk = Ne;
            }
            return;
        } else if (tk == '<') {
            if (*p == '=') {
                ++p;
                tk = Le;
            } else if (*p == '<') {
                ++p;
                tk = Shl;
            } else
                tk = Lt;
            return;
        } else if (tk == '>') {
            if (*p == '=') {
                ++p;
                tk = Ge;
            } else if (*p == '>') {
                ++p;
                tk = Shr;
            } else
                tk = Gt;
            return;
        } else if (tk == '|') {
            if (*p == '|') {
                ++p;
                tk = Lor;
            } else
                tk = Or;
            return;
        } else if (tk == '&') {
            if (*p == '&') {
                ++p;
                tk = Lan;
            } else
                tk = And;
            return;
        } else if (tk == '^') {
            tk = Xor;
            return;
        } else if (tk == '%') {
            tk = Mod;
            return;
        } else if (tk == '*') {
            tk = Mul;
            return;
        } else if (tk == '[') {
            tk = Brak;
            return;
        } else if (tk == '?') {
            tk = Cond;
            return;
        } else if (tk == '~' || tk == ';' || tk == '{' || tk == '}' || tk == '(' || tk == ')' ||
                   tk == ']' || tk == ',' || tk == ':')
            return;
    }
}

void expr(int lev)
{
    int t, *d;

    if (done)
        return;
    if (!tk) {
        printf("%d: unexpected eof in expression\n", line);
        exit(-1);
    } else if (tk == Num) {
        *++e = IMM;
        *++e = ival;
        next();
        ty = INT;
    } else if (tk == '"') {
        *++e = IMM;
        *++e = ival;
        next();
        while (tk == '"')
            next();
        data = (char *)((int)data + sizeof(int) & -sizeof(int));
        ty   = PTR;
    } else if (tk == Sizeof) {
        next();
        if (tk == '(')
            next();
        else {
            printf("%d: open paren expected in sizeof\n", line);
            exit(-1);
        }
        ty = INT;
        if (tk == Int)
            next();
        else if (tk == Char) {
            next();
            ty = CHAR;
        }
        while (tk == Mul) {
            next();
            ty = ty + PTR;
        }
        if (tk == ')')
            next();
        else {
            printf("%d: close paren expected in sizeof\n", line);
            exit(-1);
        }
        *++e = IMM;
        *++e = (ty == CHAR) ? sizeof(char) : sizeof(int);
        ty   = INT;
    } else if (tk == Id) {
        d = id;
        next();
        if (tk == '(') {
            next();
            t = 0;
            while (tk != ')') {
                expr(Assign);
                if (done)
                    return;
                *++e = PSH;
                ++t;
                if (tk == ',')
                    next();
            }
            next();
            if (d[Class] == Sys)
                *++e = d[Val];
            else if (d[Class] == Fun) {
                *++e = JSR;
                *++e = d[Val];
            } else {
                printf("%d: bad function call\n", line);
                exit(-1);
            }
            if (t) {
                *++e = ADJ;
                *++e = t;
            }
            ty = d[Type];
        } else if (d[Class] == Num) {
            *++e = IMM;
            *++e = d[Val];
            ty   = INT;
        } else {
            if (d[Class] == Loc) {
                *++e = LEA;
                *++e = loc - d[Val];
            } else if (d[Class] == Glo) {
                *++e = IMM;
                *++e = d[Val];
            } else {
                printf("%d: undefined variable\n", line);
                exit(-1);
            }
            *++e = ((ty = d[Type]) == CHAR) ? LC : LI;
        }
    } else if (tk == '(') {
        next();
        if (tk == Int || tk == Char) {
            t = (tk == Int) ? INT : CHAR;
            next();
            while (tk == Mul) {
                next();
                t = t + PTR;
            }
            if (tk == ')')
                next();
            else {
                printf("%d: bad cast\n", line);
                exit(-1);
            }
            expr(Inc);
            ty = t;
        } else {
            expr(Assign);
            if (tk == ')')
                next();
            else {
                printf("%d: close paren expected\n", line);
                exit(-1);
            }
        }
    } else if (tk == Mul) {
        next();
        expr(Inc);
        if (ty > INT)
            ty = ty - PTR;
        else {
            printf("%d: bad dereference\n", line);
            exit(-1);
        }
        *++e = (ty == CHAR) ? LC : LI;
    } else if (tk == And) {
        next();
        expr(Inc);
        if (*e == LC || *e == LI)
            --e;
        else {
            printf("%d: bad address-of\n", line);
            exit(-1);
        }
        ty = ty + PTR;
    } else if (tk == '!') {
        next();
        expr(Inc);
        *++e = PSH;
        *++e = IMM;
        *++e = 0;
        *++e = EQ;
        ty   = INT;
    } else if (tk == '~') {
        next();
        expr(Inc);
        *++e = PSH;
        *++e = IMM;
        *++e = -1;
        *++e = XOR;
        ty   = INT;
    } else if (tk == Add) {
        next();
        expr(Inc);
        ty = INT;
    } else if (tk == Sub) {
        next();
        *++e = IMM;
        if (tk == Num) {
            *++e = -ival;
            next();
        } else {
            *++e = -1;
            *++e = PSH;
            expr(Inc);
            *++e = MUL;
        }
        ty = INT;
    } else if (tk == Inc || tk == Dec) {
        t = tk;
        next();
        expr(Inc);
        if (*e == LC) {
            *e   = PSH;
            *++e = LC;
        } else if (*e == LI) {
            *e   = PSH;
            *++e = LI;
        } else {
            printf("%d: bad lvalue in pre-increment\n", line);
            exit(-1);
        }
        *++e = PSH;
        *++e = IMM;
        *++e = (ty > PTR) ? sizeof(int) : sizeof(char);
        *++e = (t == Inc) ? ADD : SUB;
        *++e = (ty == CHAR) ? SC : SI;
    } else {
        printf("%d: bad expression\n", line);
        exit(-1);
    }

    while (tk >= lev && !done) { // "precedence climbing" or "Top Down Operator Precedence" method
        t = ty;
        if (tk == Assign) {
            next();
            if (*e == LC || *e == LI)
                *e = PSH;
            else {
                printf("%d: bad lvalue in assignment\n", line);
                exit(-1);
            }
            expr(Assign);
            *++e = ((ty = t) == CHAR) ? SC : SI;
        } else if (tk == Cond) {
            next();
            *++e = BZ;
            d    = ++e;
            expr(Assign);
            if (tk == ':')
                next();
            else {
                printf("%d: conditional missing colon\n", line);
                exit(-1);
            }
            *d   = (int)(e + 3);
            *++e = JMP;
            d    = ++e;
            expr(Cond);
            *d = (int)(e + 1);
        } else if (tk == Lor) {
            next();
            *++e = BNZ;
            d    = ++e;
            expr(Lan);
            *d = (int)(e + 1);
            ty = INT;
        } else if (tk == Lan) {
            next();
            *++e = BZ;
            d    = ++e;
            expr(Or);
            *d = (int)(e + 1);
            ty = INT;
        } else if (tk == Or) {
            next();
            *++e = PSH;
            expr(Xor);
            *++e = OR;
            ty   = INT;
        } else if (tk == Xor) {
            next();
            *++e = PSH;
            expr(And);
            *++e = XOR;
            ty   = INT;
        } else if (tk == And) {
            next();
            *++e = PSH;
            expr(Eq);
            *++e = AND;
            ty   = INT;
        } else if (tk == Eq) {
            next();
            *++e = PSH;
            expr(Lt);
            *++e = EQ;
            ty   = INT;
        } else if (tk == Ne) {
            next();
            *++e = PSH;
            expr(Lt);
            *++e = NE;
            ty   = INT;
        } else if (tk == Lt) {
            next();
            *++e = PSH;
            expr(Shl);
            *++e = LT;
            ty   = INT;
        } else if (tk == Gt) {
            next();
            *++e = PSH;
            expr(Shl);
            *++e = GT;
            ty   = INT;
        } else if (tk == Le) {
            next();
            *++e = PSH;
            expr(Shl);
            *++e = LE;
            ty   = INT;
        } else if (tk == Ge) {
            next();
            *++e = PSH;
            expr(Shl);
            *++e = GE;
            ty   = INT;
        } else if (tk == Shl) {
            next();
            *++e = PSH;
            expr(Add);
            *++e = SHL;
            ty   = INT;
        } else if (tk == Shr) {
            next();
            *++e = PSH;
            expr(Add);
            *++e = SHR;
            ty   = INT;
        } else if (tk == Add) {
            next();
            *++e = PSH;
            expr(Mul);
            if ((ty = t) > PTR) {
                *++e = PSH;
                *++e = IMM;
                *++e = sizeof(int);
                *++e = MUL;
            }
            *++e = ADD;
        } else if (tk == Sub) {
            next();
            *++e = PSH;
            expr(Mul);
            if (t > PTR && t == ty) {
                *++e = SUB;
                *++e = PSH;
                *++e = IMM;
                *++e = sizeof(int);
                *++e = DIV;
                ty   = INT;
            } else if ((ty = t) > PTR) {
                *++e = PSH;
                *++e = IMM;
                *++e = sizeof(int);
                *++e = MUL;
                *++e = SUB;
            } else
                *++e = SUB;
        } else if (tk == Mul) {
            next();
            *++e = PSH;
            expr(Inc);
            *++e = MUL;
            ty   = INT;
        } else if (tk == Div) {
            next();
            *++e = PSH;
            expr(Inc);
            *++e = DIV;
            ty   = INT;
        } else if (tk == Mod) {
            next();
            *++e = PSH;
            expr(Inc);
            *++e = MOD;
            ty   = INT;
        } else if (tk == Inc || tk == Dec) {
            if (*e == LC) {
                *e   = PSH;
                *++e = LC;
            } else if (*e == LI) {
                *e   = PSH;
                *++e = LI;
            } else {
                printf("%d: bad lvalue in post-increment\n", line);
                exit(-1);
            }
            *++e = PSH;
            *++e = IMM;
            *++e = (ty > PTR) ? sizeof(int) : sizeof(char);
            *++e = (tk == Inc) ? ADD : SUB;
            *++e = (ty == CHAR) ? SC : SI;
            *++e = PSH;
            *++e = IMM;
            *++e = (ty > PTR) ? sizeof(int) : sizeof(char);
            *++e = (tk == Inc) ? SUB : ADD;
            next();
        } else if (tk == Brak) {
            next();
            *++e = PSH;
            expr(Assign);
            if (tk == ']')
                next();
            else {
                printf("%d: close bracket expected\n", line);
                exit(-1);
            }
            if (t > PTR) {
                *++e = PSH;
                *++e = IMM;
                *++e = sizeof(int);
                *++e = MUL;
            } else if (t < PTR) {
                printf("%d: pointer type expected\n", line);
                exit(-1);
            }
            *++e = ADD;
            *++e = ((ty = t - PTR) == CHAR) ? LC : LI;
        } else {
            printf("%d: compiler error tk=%d\n", line, tk);
            exit(-1);
        }
    }
}

void stmt()
{
    int *a, *b;

    if (done)
        return;
    if (tk == If) {
        next();
        if (tk == '(')
            next();
        else {
            printf("%d: open paren expected\n", line);
            exit(-1);
        }
        expr(Assign);
        if (tk == ')')
            next();
        else {
            printf("%d: close paren expected\n", line);
            exit(-1);
        }
        *++e = BZ;
        b    = ++e;
        stmt();
        if (tk == Else) {
            *b   = (int)(e + 3);
            *++e = JMP;
            b    = ++e;
            next();
            stmt();
        }
        *b = (int)(e + 1);
    } else if (tk == While) {
        next();
        a = e + 1;
        if (tk == '(')
            next();
        else {
            printf("%d: open paren expected\n", line);
            exit(-1);
        }
        expr(Assign);
        if (tk == ')')
            next();
        else {
            printf("%d: close paren expected\n", line);
            exit(-1);
        }
        *++e = BZ;
        b    = ++e;
        stmt();
        *++e = JMP;
        *++e = (int)a;
        *b   = (int)(e + 1);
    } else if (tk == Return) {
        next();
        if (tk != ';')
            expr(Assign);
        *++e = LEV;
        if (tk == ';')
            next();
        else {
            printf("%d: semicolon expected\n", line);
            exit(-1);
        }
    } else if (tk == '{') {
        next();
        while (tk != '}' && !done)
            stmt();
        next();
    } else if (tk == ';') {
        next();
    } else {
        expr(Assign);
        if (done)
            return;
        if (tk == ';')
            next();
        else {
            printf("%d: semicolon expected\n", line);
            exit(-1);
        }
    }
}

static int alloc(int n)
{
    poolsz = n;
    if (!(sym = (int *)malloc(poolsz))) {
        printf("could not malloc(%d) symbol area\n", poolsz);
        return -1;
    }
    if (!(le = e = (int *)malloc(poolsz))) {
        printf("could not malloc(%d) text area\n", poolsz);
        return -1;
    }
    if (!(data = (char *)malloc(poolsz))) {
        printf("could not malloc(%d) data area\n", poolsz);
        return -1;
    }
    if (!(sp = (int *)malloc(poolsz))) {
        printf("could not malloc(%d) stack area\n", poolsz);
        return -1;
    }
    memset(sym, 0, poolsz);
    memset(e, 0, poolsz);
    memset(data, 0, poolsz);
    return 0;
}

static void kwinit()
{
    int i;

    p = "char else enum if int return sizeof while "
        "open read close printf malloc free memset memcmp exit void main";
    i = Char;
    while (i <= While) {
        next();
        id[Tk] = i++;
    } // add keywords to symbol table
    i = OPEN;
    while (i <= EXIT) {
        next();
        id[Class] = Sys;
        id[Type]  = INT;
        id[Val]   = i++;
    } // add library to symbol table
    next();
    id[Tk] = Char; // handle void type
    next();
    idmain = id; // keep track of main
}

static void set_source(char *text)
{
    lp = p = text;
}

static int compile()
{
    int bt, ty, i;

    done   = 0;
    status = 0;
    line   = 1;
    next();
    while (tk && !done) {
        bt = INT; // basetype
        if (tk == Int)
            next();
        else if (tk == Char) {
            next();
            bt = CHAR;
        } else if (tk == Enum) {
            next();
            if (tk != '{')
                next();
            if (tk == '{') {
                next();
                i = 0;
                while (tk != '}') {
                    if (tk != Id) {
                        printf("%d: bad enum identifier %d\n", line, tk);
                        return -1;
                    }
                    next();
                    if (tk == Assign) {
                        next();
                        if (tk != Num) {
                            printf("%d: bad enum initializer\n", line);
                            return -1;
                        }
                        i = ival;
                        next();
                    }
                    id[Class] = Num;
                    id[Type]  = INT;
                    id[Val]   = i++;
                    if (tk == ',')
                        next();
                }
                next();
            }
        }
        while (tk != ';' && tk != '}' && !done) {
            ty = bt;
            while (tk == Mul) {
                next();
                ty = ty + PTR;
            }
            if (tk != Id) {
                printf("%d: bad global declaration\n", line);
                return -1;
            }
            if (id[Class]) {
                printf("%d: duplicate global definition\n", line);
                return -1;
            }
            next();
            id[Type] = ty;
            if (tk == '(') { // function
                id[Class] = Fun;
                id[Val]   = (int)(e + 1);
                next();
                i = 0;
                while (tk != ')') {
                    ty = INT;
                    if (tk == Int)
                        next();
                    else if (tk == Char) {
                        next();
                        ty = CHAR;
                    }
                    while (tk == Mul) {
                        next();
                        ty = ty + PTR;
                    }
                    if (tk != Id) {
                        printf("%d: bad parameter declaration\n", line);
                        return -1;
                    }
                    if (id[Class] == Loc) {
                        printf("%d: duplicate parameter definition\n", line);
                        return -1;
                    }
                    id[HClass] = id[Class];
                    id[Class]  = Loc;
                    id[HType]  = id[Type];
                    id[Type]   = ty;
                    id[HVal]   = id[Val];
                    id[Val]    = i++;
                    next();
                    if (tk == ',')
                        next();
                }
                next();
                if (tk != '{') {
                    printf("%d: bad function definition\n", line);
                    return -1;
                }
                loc = ++i;
                next();
                while (tk == Int || tk == Char) {
                    bt = (tk == Int) ? INT : CHAR;
                    next();
                    while (tk != ';') {
                        ty = bt;
                        while (tk == Mul) {
                            next();
                            ty = ty + PTR;
                        }
                        if (tk != Id) {
                            printf("%d: bad local declaration\n", line);
                            return -1;
                        }
                        if (id[Class] == Loc) {
                            printf("%d: duplicate local definition\n", line);
                            return -1;
                        }
                        id[HClass] = id[Class];
                        id[Class]  = Loc;
                        id[HType]  = id[Type];
                        id[Type]   = ty;
                        id[HVal]   = id[Val];
                        id[Val]    = ++i;
                        next();
                        if (tk == ',')
                            next();
                    }
                    next();
                }
                *++e = ENT;
                *++e = i - loc;
                while (tk != '}' && !done)
                    stmt();
                *++e = LEV;
                id   = sym; // unwind symbol table locals
                while (id[Tk]) {
                    if (id[Class] == Loc) {
                        id[Class] = id[HClass];
                        id[Type]  = id[HType];
                        id[Val]   = id[HVal];
                    }
                    id = id + Idsz;
                }
            } else {
                id[Class] = Glo;
                id[Val]   = (int)data;
                data      = data + sizeof(int);
            }
            if (tk == ',')
                next();
        }
        next();
    }
    if (done)
        return -1;
    if (!(pc = (int *)idmain[Val])) {
        printf("main() not defined\n");
        return -1;
    }
    return 0;
}

static int vmsetup(int argc, char **argv)
{
    int *t;

    bp = sp = (int *)((int)sp + poolsz);
    *--sp   = EXIT; // call exit if main returns
    *--sp   = PSH;
    t       = sp;
    *--sp   = argc;
    *--sp   = (int)argv;
    *--sp   = (int)t;
    cycle   = 0;
    a       = 0;
    return 0;
}

static int burst()
{
    int i, *t;
    int budget = 8192;
    long long args[6];
    int k;

    while (budget--) {
        i = *pc++;
        ++cycle;
        if (debug) {
            printf("%d> %.4s", cycle, &ops[i * 5]);
            if (i <= ADJ)
                printf(" %d\n", *pc);
            else
                printf("\n");
        }
        if (i == LEA)
            a = (int)(bp + *pc++); // load local address
        else if (i == IMM)
            a = *pc++; // load global address or immediate
        else if (i == JMP)
            pc = (int *)*pc; // jump
        else if (i == JSR) {
            *--sp = (int)(pc + 1);
            pc    = (int *)*pc;
        } // jump to subroutine
        else if (i == BZ)
            pc = a ? pc + 1 : (int *)*pc; // branch if zero
        else if (i == BNZ)
            pc = a ? (int *)*pc : pc + 1; // branch if not zero
        else if (i == ENT) {
            *--sp = (int)bp;
            bp    = sp;
            sp    = sp - *pc++;
        } // enter subroutine
        else if (i == ADJ)
            sp = sp + *pc++; // stack adjust
        else if (i == LEV) {
            sp = bp;
            bp = (int *)*sp++;
            pc = (int *)*sp++;
        } // leave subroutine
        else if (i == LI)
            a = *(int *)a; // load int
        else if (i == LC)
            a = *(char *)a; // load char
        else if (i == SI)
            *(int *)*sp++ = a; // store int
        else if (i == SC)
            a = *(char *)*sp++ = a; // store char
        else if (i == PSH)
            *--sp = a; // push

        else if (i == OR)
            a = *sp++ | a;
        else if (i == XOR)
            a = *sp++ ^ a;
        else if (i == AND)
            a = *sp++ & a;
        else if (i == EQ)
            a = *sp++ == a;
        else if (i == NE)
            a = *sp++ != a;
        else if (i == LT)
            a = *sp++ < a;
        else if (i == GT)
            a = *sp++ > a;
        else if (i == LE)
            a = *sp++ <= a;
        else if (i == GE)
            a = *sp++ >= a;
        else if (i == SHL)
            a = *sp++ << a;
        else if (i == SHR)
            a = *sp++ >> a;
        else if (i == ADD)
            a = *sp++ + a;
        else if (i == SUB)
            a = *sp++ - a;
        else if (i == MUL)
            a = *sp++ * a;
        else if (i == DIV)
            a = *sp++ / a;
        else if (i == MOD)
            a = *sp++ % a;

        else if (i == OPEN) {
            c4_sys.path  = (char *)sp[1];
            c4_sys.flags = *sp;
            return C4_OPEN;
        } else if (i == READ) {
            c4_sys.fd  = sp[2];
            c4_sys.buf = (char *)sp[1];
            c4_sys.n   = *sp;
            return C4_READ;
        } else if (i == CLOS) {
            c4_sys.fd = *sp;
            return C4_CLOSE;
        } else if (i == PRTF) {
            t = sp + pc[1];
            for (k = 0; k < 6; k++)
                args[k] = t[-2 - k];
            a = c4_format((char *)t[-1], args);
        } else if (i == MALC)
            a = (int)malloc(*sp);
        else if (i == FREE)
            free((void *)*sp);
        else if (i == MSET)
            a = (int)memset((char *)sp[2], sp[1], *sp);
        else if (i == MCMP)
            a = memcmp((char *)sp[2], (char *)sp[1], *sp);
        else if (i == EXIT) {
            printf("exit(%d) cycle = %d\n", *sp, cycle);
            status = *sp;
            return C4_EXIT;
        } else {
            printf("unknown instruction = %d! cycle = %d\n", i, cycle);
            status = -1;
            return C4_STOP;
        }

        if (c4_olen > 4096)
            return C4_TICK;
    }
    return C4_TICK;
}

#undef exit
#undef printf
#undef int

static void grow(int need)
{
    int n;
    char *q;

    if (c4_olen + need <= ocap)
        return;
    n = ocap ? ocap : 256;
    while (n < c4_olen + need)
        n *= 2;
    q = (char *)realloc(c4_obuf, n);
    if (!q)
        return;
    c4_obuf = q;
    ocap    = n;
}

static void outc(char c)
{
    grow(1);
    if (c4_olen < ocap)
        c4_obuf[c4_olen++] = c;
}

static void outs(const char *s, int n)
{
    int i;

    if (!s) {
        s = "(null)";
        n = 6;
    }
    grow(n);
    i = 0;
    while (i < n && c4_olen < ocap)
        c4_obuf[c4_olen++] = s[i++];
}

static void outd(long long v)
{
    char b[32];
    int i = 0;
    unsigned long long u;

    if (v < 0) {
        outc('-');
        if (v == (-9223372036854775807LL - 1))
            u = 9223372036854775808ULL;
        else
            u = (unsigned long long)(-v);
    } else {
        u = (unsigned long long)v;
    }
    if (u == 0)
        b[i++] = '0';
    while (u) {
        b[i++] = (char)('0' + (u % 10));
        u /= 10;
    }
    while (i)
        outc(b[--i]);
}

int c4_format(const char *fmt, const long long *args)
{
    int start = c4_olen;
    int ai    = 0;
    const char *q;

    if (!fmt)
        return 0;
    for (q = fmt; *q; q++) {
        if (*q != '%') {
            outc(*q);
            continue;
        }
        q++;
        if (*q == '%') {
            outc('%');
            continue;
        }
        int width = 0, prec = -1;
        while (*q >= '0' && *q <= '9')
            width = width * 10 + *q++ - '0';
        if (*q == '.') {
            q++;
            if (*q == '*') {
                prec = (int)args[ai++];
                q++;
            } else {
                prec = 0;
                while (*q >= '0' && *q <= '9')
                    prec = prec * 10 + *q++ - '0';
            }
        }
        if (*q == 'd' || *q == 'i') {
            outd(args[ai++]);
        } else if (*q == 's') {
            char *s = (char *)(long)args[ai++];
            int n   = s ? (int)strlen(s) : 0;
            int pad;
            if (prec >= 0 && n > prec)
                n = prec;
            pad = width > n ? width - n : 0;
            while (pad--)
                outc(' ');
            outs(s, n);
        }
    }
    return c4_olen - start;
}

int c4_outf(const char *fmt, ...)
{
    long long args[8];
    int n = 0;
    va_list ap;
    const char *q;

    va_start(ap, fmt);
    for (q = fmt; *q && n < 8; q++) {
        if (*q != '%')
            continue;
        q++;
        if (*q == '%')
            continue;
        while (*q >= '0' && *q <= '9')
            q++;
        if (*q == '.') {
            q++;
            if (*q == '*') {
                args[n++] = va_arg(ap, long long);
                q++;
            } else {
                while (*q >= '0' && *q <= '9')
                    q++;
            }
        }
        if (*q == 'd' || *q == 'i')
            args[n++] = va_arg(ap, long long);
        else if (*q == 's')
            args[n++] = (long long)(long)va_arg(ap, char *);
    }
    va_end(ap);
    return c4_format(fmt, args);
}

void c4_clear_out()
{
    c4_olen = 0;
}

int c4_alloc(int n)
{
    return (int)alloc(n);
}

void c4_kwinit()
{
    kwinit();
}

void c4_set_source(char *text)
{
    set_source(text);
}

int c4_compile()
{
    int r     = (int)compile();
    c4_status = status;
    return r;
}

int c4_vmsetup(int argc, char **argv)
{
    return (int)vmsetup(argc, argv);
}

int c4_burst()
{
    int r     = (int)burst();
    c4_a      = a;
    c4_cycle  = cycle;
    c4_status = status;
    return r;
}

void c4_set_a(long long v)
{
    a    = v;
    c4_a = v;
}
