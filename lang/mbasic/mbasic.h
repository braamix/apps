// Microsoft BASIC 1.1 for the 6502, ported to Braam.
//
// Upstream is one MACRO-10 file, m6502.asm, and the design documents recovered
// from it are in internals/. This header is what upstream kept in page
// zero -- "THESE ARE THE MOST COMMONLY USED LOCATIONS. THEY HOLD BOOKKEEPING
// INFO AND ALL OTHER FREQUENTLY USED INFORMATION" -- turned into fields of one
// object, plus the shapes that replace the ones that were byte layouts.
//
// The interpreter is plain C++ and never blocks. braam.cpp is the driver and
// the only file in the program with a co_await in it: a co_await is a call and
// not a tail call here, so a statement loop that awaited would grow the native
// stack until the process trapped. step() runs a burst and says what it wants.
#pragma once

#include "err.h"
#include "kernel/str.h"
#include "kernel/string.h"
#include "kernel/types.h"
#include "kernel/vec.h"

// ============================================================== tokens
//
// A token's value is 128 + its ordinal in RESLST, and RESLST, STMDSP, FUNDSP
// and OPTAB are four parallel tables guarded by the same conditionals: adding
// a word under one renumbers everything after it in all four. This build is
// the Apple base (INTPRC, ADDPRC, GETCMD) plus EXTIO, DISKO and LNGERR, with
// no VERIFY, DDT, NULL or TIME, plus RENUM from the later releases. See
// tables.cpp.

enum : u8 {
    ENDTK  = 0x80, // the first statement token
    FORTK  = 0x81,
    DATATK = 0x83,
    GOTOTK = 0x89,
    RUNTK  = 0x8A,
    GOSUTK = 0x8D,
    REMTK  = 0x8F,
    PRINTK = 0x98,
    SCRATK = 0xA1, // NEW
    RENUTK = 0xA2, // RENUM, the last statement token -- not upstream's
    TABTK  = 0xA3,
    TOTK   = 0xA4,
    FNTK   = 0xA5,
    SPCTK  = 0xA6,
    THENTK = 0xA7,
    NOTTK  = 0xA8,
    STEPTK = 0xA9,
    PLUSTK = 0xAA, // the seven binary operators, contiguous for OPTAB
    MINUTK = 0xAB,
    MULTK  = 0xAC,
    DIVTK  = 0xAD,
    PWRTK  = 0xAE,
    ANDTK  = 0xAF,
    ORTK   = 0xB0,
    GREATK = 0xB1, // the relationals, adjacent and in this order
    EQULTK = 0xB2,
    LESSTK = 0xB3,
    ONEFUN = 0xB4, // SGN, the first function
    LASNUM = 0xC7, // CHR$, the last taking one argument
    MIDTK  = 0xCA,
    GOTK   = 0xCB, // GO: a token with no STMDSP entry
};

constexpr u8 STMT_COUNT = RENUTK - ENDTK + 1;
constexpr u8 FUNC_COUNT = MIDTK - ONEFUN + 1;

// RESLST, one entry per token, in order from ENDTK.
extern const Str RESLST[];
constexpr u8 RESLST_COUNT = GOTK - ENDTK + 1;

// ============================================================== positions
//
// TXTPTR. Upstream's was the address field of a live LDA instruction, which
// saved a register on the hottest path; a plain position is equivalent
// (13-porting-notes.md §1). Program lines are a sorted Vec here rather than a
// list of absolute links, so crossing a line is `line++` and LNKPRG is gone.

constexpr i32 DIRECT      = -1;     // the line typed at OK, held in `dirbuf`
constexpr u16 DIRECT_LINE = 0xFFFF; // CURLIN+1 == 255

// Upstream pointed TXTPTR at TXTTAB-1 and relied on reading the zero byte
// planted there, so that GOTO and STXTPT could both reuse the ordinary "end of
// line, follow the link" path rather than duplicate it. AT_LINE_START is that
// zero byte: the position before a line's text, which chrget() steps off by
// wrapping to zero and peek_at() reads as a terminator.
constexpr u32 AT_LINE_START = ~u32(0);

struct TextPos {
    i32 line = DIRECT;
    u32 off  = AT_LINE_START;

    bool direct() const { return line == DIRECT; }
};

// A name's first character. CRUNCH folds what it stores, so crunched text
// holds only the lowercase half; both halves stay under 0x80, where
// Var::name keeps its type tag.
inline bool isletc(u8 c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

inline bool isdigit(u8 c)
{
    return c >= '0' && c <= '9';
}

// A stored line: the line number and its crunched text, NUL-terminated the way
// upstream's was, so every scan still ends on a zero byte.
struct Line {
    u16 num = 0;
    Vec<u8> text;
};

// ============================================================== values
//
// VALTYP is 0 for numeric and 255 for string -- the comment at m6502.asm:766
// says "0=NUMERIC 1=STRING" and is wrong. INTFLG is 128 for an integer.
constexpr u8 VNUM = 0;
constexpr u8 VSTR = 255;

enum class VType : u8 { Num, Str, Int, Fn };

// What upstream held in the FAC, where a string was a pointer to a 3-byte
// descriptor rather than a value (13-porting-notes.md §2.3). Here the two
// shapes are one struct and `s` is live only when type is Str.
struct Val {
    f64 n = 0;
    String s;
    u8 valtyp = VNUM;

    bool is_str() const { return valtyp == VSTR; }
};

// A variable, named the way upstream named it: two bytes with the type in
// their high bits, so that A, A$ and A% never match each other in one flat
// search and no type tag is needed. Only the first two characters of a name
// are significant -- EATEM discards the rest.
//
//   float A, AB   name[0] bit 7 = 0   name[1] bit 7 = 0
//   string A$     name[0] bit 7 = 0   name[1] bit 7 = 1
//   integer A%    name[0] bit 7 = 1   name[1] bit 7 = 1
//   DEF FN A      name[0] bit 7 = 1   name[1] bit 7 = 0
struct Var {
    u8 name[2] = { 0, 0 };
    VType type = VType::Num;

    f64 num = 0;
    String str;
    i16 ival = 0;

    // DEF FN: the formula's text position and the argument variable's index.
    // Upstream tested the pointer's high byte for zero to mean "referenced but
    // never defined", because PTRGT2 auto-created the entry with five zeroes.
    TextPos fnbody;
    u32 fnarg  = NO_VAR;
    bool fndef = false;

    static constexpr u32 NO_VAR = ~u32(0);
};

// An array. Upstream stored the extents big-endian and in reverse subscript
// order, because ISARY collected subscripts onto the 6502 stack and the
// creation loop popped them; the combined effect is that for A(i,j) the linear
// index is j*extent_i + i -- the FIRST subscript varies fastest, i.e. column
// major. The extents are in written order here and the indexing does the same
// arithmetic. An undimensioned array gets an extent of 11 per dimension.
struct Array {
    u8 name[2] = { 0, 0 };
    VType type = VType::Num;
    Vec<u16> extent;
    Vec<f64> num;
    Vec<String> str;
    Vec<i16> ints;
};

// Where a value lives. An index and never a pointer: upstream's FORPNT was a
// raw pointer into VARTAB and was invalidated by every BLTU, which is why the
// original re-derives it in so many places -- and a suspension has to hold one
// across a return to the driver.
struct VarRef {
    enum Kind : u8 { None, Simple, Element, Scratch } kind = None;
    u32 index                                              = 0; // into vars, or into arrays
    u32 elem                                               = 0; // Element: the linear element index
    u8 valtyp                                              = VNUM;
    bool intflg                                            = false;

    bool null() const { return kind == None; }

    static VarRef scratch() { return VarRef{ Scratch, 0, 0, VNUM, false }; }

    friend bool operator==(const VarRef &a, const VarRef &b)
    {
        return a.kind == b.kind && a.index == b.index && a.elem == b.elem;
    }
};

// ============================================================== frames
//
// Upstream's semi-permanent stack entries (02-data-structures.md §4). The
// 6502 hardware stack was a typed structure scanned by tag byte, and it cannot
// be replaced by the host call stack: FOR and RETURN unwind by CONTENT, not by
// depth. The fourth record kind, the deferred-expression frame, had no tag and
// was never scanned for -- it is a frmevl() local here and went with it.
//
// The step's separate sign byte and the limit's packed form existed only to
// serve FCOMPN and PSHWD; they are two doubles. What they encoded does not go
// with them -- see next() in stmt.cpp.
struct Frame {
    enum Kind : u8 { For = FORTK, Gosub = GOSUTK } kind = For;
    VarRef forpnt;
    f64 step   = 1;
    f64 limit  = 0;
    u16 curlin = 0;
    TextPos txtptr; // FOR: the end of the FOR statement, i.e. TXTPTR + DATAN.
}; // GOSUB: before the target line number, so RETURN's fall
   // into DATAN still skips ON X GOSUB's whole list.

// ============================================================== channels
//
// EXTIO. Upstream routed OUTDO and INLIN through CHANNL and left the rest to
// the machine's ROM. Output is buffered and drained by the driver, so PRINT#
// never suspends; INPUT# does, through the same NeedLine as INPUT.
struct Chan {
    u8 num     = 0; // the file number OPEN gave it
    i32 fd     = -1;
    bool input = false;
    String obuf;
    String ibuf;
    usize ipos = 0;
    bool eof   = false;
};

// ============================================================== the protocol
//
// What step() stopped for. Upstream's blocking calls, turned inside out: the
// interpreter says what it wants and the driver performs it. This is
// simbesm's cpu_burst() contract (emulators/simbesm/machine.h).
enum class Reason : u8 {
    Done,     // finished; `status` is the exit code
    Yield,    // the burst is up: drain, take ^C, come straight back
    NeedLine, // INLIN: fill in_line and in_end
    NeedChar, // CZGETL: fill in_char
    NeedFile, // perform req.file and fill its reply fields
};

enum class FileOp : u8 { Open, Close, Load, Save };

// How a read ended. Upstream had no way to say any of this: INLIN could not
// fail. Err(Intr) with sig_take(SIG_INT) is ^C; Err(Closed) is end of input.
enum class InEnd : u8 { Line, Eof, Interrupt, Error };

struct FileReq {
    FileOp op  = FileOp::Open;
    u8 num     = 0;  // the BASIC file number
    i32 fd     = -1; // in for Close, out for Open
    bool input = true;
    String name; // Open, Load, Save
    String data; // in for Save, out for Load
    bool ok = true;
};

struct Request {
    u8 chan   = 0;     // 0 is the console; otherwise the channel to read
    bool main = false; // a MAIN line: in script mode the file supplies it
    Str prompt;        // "", "? " or "?? " -- already written to `out` as well
    FileReq file;
};

// Where step() picks up. Upstream had a program counter for this; a read that
// unwinds the native stack has to make it data.
enum class Resume : u8 { Newstt, Main, Input, Get, File };

// The INPUT/READ/GET loop's state. Upstream held all of it in page zero and
// never unwound out of it; naming every field is what a suspension costs.
struct InputState {
    u8 inpflg = 0; // 0 INPUT, 64 GET, 152 READ -- upstream's own values
    u8 chan   = 0;
    TextPos varlist; // VARTXT: the program text at the top of INLOOP
    VarRef forpnt;
    String buf; // BUF: the line being consumed
    usize inpptr = 0;
    String dbuf; // READ: the DATA statement's text
    usize dpos = 0;
};

constexpr u8 INPFLG_INPUT = 0;
constexpr u8 INPFLG_GET   = 64;
constexpr u8 INPFLG_READ  = 152;

// ============================================================== limits
//
// GETSTK's one budget covered three unlike things -- expression nesting, the
// frame count, and FBUFFR's safety below the stack. They separate here.
constexpr u32 NUMLEV     = 23;  // guaranteed nesting; raised from 19 on 78-02-25
constexpr u32 STKLIM     = 46;  // FOR/GOSUB frames, as the 256-byte page held
constexpr u32 NUMTMP     = 3;   // string temporaries; exceeding them is ?ST
constexpr u32 STMT_BURST = 512; // statements between parks -- see newstt()
constexpr u32 BUFLEN     = 240; // the input line
constexpr u32 CLMWID     = 14;  // PRINT's comma field width, an assembly constant
constexpr u32 LINLEN     = 80;  // assumed width where there is no terminal
constexpr u16 MAXLIN     = 63999;

struct Interp;
using StmtFn = void (Interp::*)();
using FuncFn = void (Interp::*)();

extern const StmtFn STMDSP[STMT_COUNT];
extern const FuncFn FUNDSP[FUNC_COUNT];

// OPTAB: a precedence and a handler, indexed as 3*(token - PLUSTK) upstream.
// The precedences are arbitrary except in their ordering, which is where NOT
// binding tighter than AND but looser than a comparison is written down.
struct OpEntry {
    u8 prec;
    void (Interp::*fn)(Val &arg);
};

enum : u8 { OP_NEG = 7, OP_NOT = 8, OP_REL = 9, OPTAB_COUNT = 10 };
extern const OpEntry OPTAB[OPTAB_COUNT];

// ============================================================== the machine

struct Interp {
    // ---- the driver's half
    Reason step();
    Request req;
    String out; // OUTDO's sink; the driver drains it at every stop
    Vec<Chan> chans;

    String in_line; // NeedLine
    InEnd in_end = InEnd::Line;
    u8 in_char   = 0;     // NeedChar
    bool brkflg  = false; // ^C arrived; ISCNTC reads it
    i32 status   = 0;

    // Script mode: a named file, whose lines the driver feeds as Main reads.
    // No banner and no Ok, an implicit RUN when the lines run out, and exit
    // when that run ends.
    bool script  = false;
    bool running = false; // the implicit RUN is under way
    bool ran_    = false; // a RUN statement has executed

    // INIT. The two questions are asked only at a console: down a pipe there
    // is no one to answer them, and a run that reads its program from a file
    // should not eat the first two lines of it.
    Reason start(u32 cols, bool interactive);
    void banner();

    // ---- page zero
    TextPos txtptr;
    Vec<u8> dirbuf; // BUF: the crunched direct line
    u16 curlin  = DIRECT_LINE;
    u16 linnum  = 0; // LINGET's answer
    u8 valtyp   = VNUM;
    bool intflg = false;
    u8 dimflg   = 0;
    u8 subflg   = 0;
    u8 opmask   = 0;
    u8 domask   = 0;
    u8 cntwfl   = 0; // ^O: output suppression
    u8 chanl    = 0; // EXTIO: the channel PRINT/INPUT is routed to
    u32 trmpos  = 0; // the print head's column
    u32 linwid  = LINLEN;
    u32 ncmwid  = LINLEN - LINLEN % CLMWID;
    u32 numlev  = 0; // live frmevl() levels
    u32 ntemp   = 0; // live string temporaries

    Val fac;
    u8 varnam[2] = { 0, 0 };
    VarRef forpnt; // LET leaves the assigned variable here, as upstream did,
                   // which is how FOR gets its loop variable

    // What upstream juggled on the 6502 stack while GETBYT read the numeric
    // argument of LEFT$, RIGHT$ and MID$; PREAM was the prologue that unpicked
    // it. ISFUN parses them and leaves them here.
    String fnstr;
    u8 fnn1 = 0, fnn2 = 0;
    bool fnhas2 = false;

    // ---- storage
    Vec<Line> prog;
    Vec<Var> vars;
    Vec<Array> arrays;
    Vec<Frame> frames;
    InputState inp;

    TextPos datptr; // DATPTR: the READ scan
    u16 datlin = 0;

    // CONT. One bool is the whole of "you can CONT after STOP or ^C, but not
    // after an error or after touching the program".
    struct ContPoint {
        TextPos txt;
        u16 lin    = 0;
        bool valid = false; // upstream's OLDTXT+1 != 0
    } oldtxt;

    // POKE/PEEK/WAIT lost their machine; they keep a scratch space of their
    // own so the statements still work and GETADR's 0..65535 check is unchanged.
    u8 *poke_space = nullptr;

    u32 memsiz = 0; // the byte budget FRE reports against
    f64 rndx   = 0; // the last random number

    // ---- error and suspension (err.h)
    bool pending() const { return halt_ != Halt::None; }
    void error(ErrCode code);
    bool reason(bool ok) // REASON: upstream's "is there heap left"
    {
        if (!ok)
            error(ERROM);
        return ok;
    }

    // ---- newstt.cpp
    void newstt();
    void gone();
    void gone2(u8 c);
    void gone3();
    void iscntc();
    void ready();
    void error_print();
    void break_print();
    void stpend(bool print_break);
    void stkini();
    void clearc();
    void runc();
    bool getstk(u8 n);
    void main_line(); // MAIN: the line typed at OK
    void main_resume();

    // ---- crunch.cpp
    u8 chrget();
    u8 chrgot();
    u8 peek_at(TextPos p) const;
    void synchr(u8 want);
    bool terminator(u8 c) const { return c == 0 || c == ':'; }
    void crunch(Str src, Vec<u8> &dst);
    void linget();
    usize fndlin(u16 n, bool &found) const;
    void scrtch();
    void stxtpt();

    // ---- list.cpp
    void list();

    // ---- renum.cpp
    void renum();

    // ---- print.cpp
    void outdo(u8 c);
    void outstr(Str s);
    void crdo();
    void crfin();
    void print();
    void printn();
    void cmd();
    void strprt(Str s);
    void linprt(u32 n);

    // ---- stmt.cpp and the rest are declared beside their tables (tables.cpp).
    void stmt_end();
    void stmt_for();
    void stmt_next();
    void stmt_data();
    void stmt_inputn();
    void stmt_input();
    void stmt_dim();
    void stmt_read();
    void stmt_let();
    void stmt_goto();
    void stmt_run();
    void stmt_if();
    void stmt_restore();
    void stmt_gosub();
    void stmt_return();
    void stmt_rem();
    void stmt_stop();
    void stmt_ongoto();
    void stmt_wait();
    void stmt_load();
    void stmt_save();
    void stmt_def();
    void stmt_poke();
    void stmt_printn();
    void stmt_print();
    void stmt_cont();
    void stmt_list();
    void stmt_clear();
    void stmt_cmd();
    void stmt_sys();
    void stmt_open();
    void stmt_close();
    void stmt_get();
    void stmt_new();
    void stmt_renum();

    usize fndfor(VarRef v) const;
    void scan_to(bool stop_colon);
    void datan();
    void remn();

    // ---- OPTAB's handlers. ARG is the left operand, the FAC the right, which
    // is upstream's order: FSUB is ARG-FAC and FDIV is ARG/FAC, memory minus
    // and divided by the accumulator, not the other way round.
    void ovcheck();
    void op_add(Val &arg);
    void op_sub(Val &arg);
    void op_mul(Val &arg);
    void op_div(Val &arg);
    void op_pwr(Val &arg);
    void op_and(Val &arg);
    void op_or(Val &arg);
    void op_neg(Val &arg);
    void op_not(Val &arg);
    void op_rel(Val &arg);

    // ---- FUNDSP. The three past LASNUM take more than one argument and are
    // dispatched differently; PREAM is their shared prologue.
    void fn_sgn();
    void fn_int();
    void fn_abs();
    void fn_usr();
    void fn_fre();
    void fn_pos();
    void fn_sqr();
    void fn_rnd();
    void fn_log();
    void fn_exp();
    void fn_cos();
    void fn_sin();
    void fn_tan();
    void fn_atn();
    void fn_peek();
    void fn_len();
    void fn_str();
    void fn_val();
    void fn_asc();
    void fn_chr();
    void fn_left();
    void fn_right();
    void fn_mid();

    // ---- frmevl.cpp
    void frmevl();
    void frmevl_prec(u8 minprec);
    void frmnum();
    void eval();
    void strtxt();
    void cat();
    void chknum();
    void chkstr();
    void chkval(bool want_str);
    void parchk();
    void isvar();
    void isfun();
    u8 getbyt();
    u16 getadr();
    u16 adr_of();
    i16 ayint();
    i16 posint();

    // ---- ptrget.cpp
    //
    // `create` is upstream's NOTFNS, which looked at its own caller's return
    // address to see whether it was ISVAR: a variable being READ in an
    // expression is not created, and answers the ZERO block instead.
    VarRef ptrget(bool create = true);
    VarRef isary(bool create);
    VarRef element_of(VarRef r, const Vec<u16> &subs);
    VarRef getfnm();
    void fndoer();
    f64 var_get_num(VarRef v);
    void var_put_num(VarRef v, f64 x);
    Str var_get_str(VarRef v);
    bool var_put_str(VarRef v, Str s);

    // ---- fout.cpp / math.cpp
    Str fout(f64 v, char *buf, usize cap);
    f64 fin();

    // ---- file.cpp
    Chan *chan_find(u8 n);
    void file_resume();
    bool filename(String &name_out);
    bool load_line(Str line);

    // ---- input.cpp
    void inloop();
    void input_resume();
    void get_resume();
    void datlop();
    void trmnok();
    void varend();
    Str in_src() const;
    usize &in_pos();
    f64 fin_str(Str s, usize &used);

private:
    friend struct InterpAccess;
    Halt halt_       = Halt::None;
    Reason want_     = Reason::Yield;
    Resume resume_   = Resume::Newstt;
    ErrCode errcode_ = ERRSN;
    u32 burst_       = STMT_BURST;

    void suspend_line(Str prompt, u8 chan, Resume back);
    void suspend_char(Resume back);
    void suspend_file(Resume back);
};

// Held by frmevl() for one nesting level. The decrement has to happen on every
// return path, and a sticky-flag unwind has many.
struct Depth {
    Interp &b;
    explicit Depth(Interp &b_) : b(b_) { b.numlev++; }
    ~Depth() { b.numlev--; }
    Depth(const Depth &)            = delete;
    Depth &operator=(const Depth &) = delete;
};
