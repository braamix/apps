// ADVENTURE -- Jim Gillogly, Jul 1977
//
// This program is a re-write of ADVENT, written in FORTRAN mostly by
// Don Woods of SAIL.  In most places it is as nearly identical to the
// original as possible given the language and word-size differences.
// A few places, such as the message arrays and travel arrays were changed
// to reflect the smaller core size and word size.  The labels of the
// original are reflected in this version, so that the comments of the
// fortran are still applicable here.
//
// The data file distributed with the fortran source is assumed to be called
// "glorkz" in the directory where the program is first run.
//
// Data save/restore rewritten in portable way by Serge Vakulenko.
//
// Ported to Braam: glorkz is inside the binary and parsed at startup, and
// everything that reached the C library or the OS is in braam.cpp.

// hdr.h: included by c advent files.  It pulls the SDK's headers in itself, so
// a unit here includes hdr.h and nothing else.
#pragma once

#include "braam.h"
#include "kernel/alloc.h"
#include "proc/io.h"

constexpr int TAB = 011;
constexpr int LF  = 012;

#define FLUSHLF          \
    while (next() != LF) \
        ;

constexpr int FALSE = 0;
constexpr int TRUE  = 1;

constexpr usize DATSIZE = 46 * 1024; // size of encrypted data
constexpr usize MAXSTR  = 20;        // max length of user's words
constexpr usize HTSIZE  = 512;       // max number of vocab words
constexpr usize RTXSIZ  = 205;
constexpr usize MAGSIZ  = 35;
constexpr usize CLSMAX  = 12;
constexpr usize LOCSIZ  = 141; // number of locations

struct HashTab { // hash table for vocabulary
    i32 val;     // word type &index (ktab)
    i32 hash;    // 32-bit hash value
};

struct Text {
    u16 seekadr; // DATFILE must be < 2**16
    u16 txtlen;  // length of msg starting here
};

struct Travel {     // direcs & conditions of travel
    Travel *next;   // ptr to next list entry
    u16 conditions; // m in writeup (newloc / 1000)
    u16 tloc;       // n in writeup (newloc % 1000)
    u16 tverb;      // the verb that takes you there
};

// An exit from deep in the call graph, which had no way back: done() and its
// callers hand the status up instead, and a negative one says the game is over
// and already reported.
constexpr int ADV_OVER = -1;

// The game.  Upstream's globals are the state, and its routines are the
// methods; a field carries a trailing underscore so neither hides the other.
class Game {
public:
    Task<i32> play(Args args); // upstream's main()

private:
    // ---- the command loop -- main.cpp ---------------------------------
    int closing(), caveclose();

    // ---- setup -- init.cpp --------------------------------------------
    void linkdata();
    Task<i32> startup();

    // ---- the glorkz parser -- io.cpp ----------------------------------
    void rdata();
    int next(), rnum();
    void putdat(char), rtrav(), rdesc(int), rvoc(), rlocs(), rdflt(), rliq(), rhints();

    // ---- messages and input -- io.cpp ---------------------------------
    void speak(Text *), pspeak(int, int), rspeak(int), mspeak(int);
    Task<void> getin(); // fills wd1_ and wd2_
    Task<i32> yes(int, int, int), yesm(int, int, int);

    // ---- the objects -- vocab.cpp -------------------------------------
    void dstroy(int), juggle(int), move(int, int), carry(int, int), drop(int, int);
    int put(int, int, int), vocab(const char *, int, int);

    // ---- statement functions -- subr.cpp ------------------------------
    int toting(int), here(int), at(int), liq2(int), liq(int), liqloc(int);
    int bitset(int, int), forced(int), dark(int), pct(int);

    // ---- the dwarves and the travel table -- move.cpp -----------------
    int fdwarf(), mback(), badmove(), trbridge(), specials(), march();

    // ---- the hints and the verb handlers -- verbs.cpp -----------------
    Task<void> checkhints();
    int trsay(), trtake(), dropper(), trdrop(), tropen(), trtoss(), trfeed(), trfill();
    Task<i32> trkill();

    // ---- privileged operations -- wizard.cpp --------------------------
    void poof();
    Task<i32> wizard(), start(int), ciao();

    // ---- scoring and the end -- done.cpp ------------------------------
    int score();
    Task<i32> done(int), die(int);

    // ---- suspend and resume -- save.cpp -------------------------------
    Task<Result<void>> save(Str), restore(Str);

    // ================= state ===========================================
    i16 loc_, newloc_, oldloc_, oldlc2_, wzdark_, gaveup_, kq_, k_, k2_;
    i16 verb_, obj_, spk_;
    i16 blklin_;
    i32 saved_, savet_, mxscor_, latncy_;

    HashTab voc_[HTSIZE]; // hash table for vocabulary

    Text rtext_[RTXSIZ]; // random text messages

    Text mtext_[MAGSIZ]; // magic messages

    i16 clsses_;
    Text ctext_[CLSMAX]; // classes of adventurer
    i16 cval_[CLSMAX];

    Text ptext_[101]; // object descriptions

    Text ltext_[LOCSIZ]; // long loc description
    Text stext_[LOCSIZ]; // short loc descriptions

    Travel *travel_[LOCSIZ]; // direcs & conditions of travel

    i16 atloc_[LOCSIZ];

    i16 plac_[101];              // initial object placement
    i16 fixd_[101], fixed_[101]; // location fixed?

    i16 actspk_[35]; // rtext msg for verb <n>

    i16 cond_[LOCSIZ]; // various condition bits

    i16 hntmax_;
    i16 hints_[20][5]; // info on hints
    i16 hinted_[20], hintlc_[20];

    i16 place_[101], prop_[101], plink_[201];
    i16 abb_[LOCSIZ];

    i16 maxtrs_, tally_, tally2_; // treasure values

    i16 keys_, lamp_, grate_, cage_, rod_, // mnemonics
        rod2_, steps_, bird_, door_, pillow_, snake_, fissur_, tablet_, clam_, oyster_, magzin_,
        dwarf_, knife_, food_, bottle_, water_, oil_, plant_, plant2_, axe_, mirror_, dragon_,
        chasm_, troll_, troll2_, bear_, messag_, vend_, batter_, nugget_, coins_, chest_, eggs_,
        tridnt_, vase_, emrald_, pyram_, pearl_, rug_, chain_, spices_, back_, look_, cave_, null_,
        entrnc_, dprssn_, say_, lock_, throw_, find_, invent_;

    i16 chloc_, chloc2_, dseen_[7], // dwarf stuff
        dloc_[7], odloc_[7], dflag_, daltlc_;

    i16 tk_[21], stick_, dtotal_, attack_;
    i16 turns_, lmwarn_, iwest_, knfloc_, // various flags & counters
        detail_, abbnum_, maxdie_, numdie_, holdng_, dkill_, foobar_, bonus_, clock1_, clock2_,
        closng_, panic_, closed_, scorng_;

    i16 limit_;

    // ---- what upstream kept beside the state struct --------------------
    Travel *tkk_;      // travel is closer to keys(...)
    char wd1_[MAXSTR]; // the complete words
    char wd2_[MAXSTR];
    int delhit_; // user typed a DEL
    const char *magic_;
    i32 status_; // what proc_main will return

    static const i16 SETBIT[16]; // bit defn masks 1,2,4,...
};

// Free: pure, or defined in braam.cpp, which must not include this header.
[[noreturn]] void bug(int);
int ran(int);
void datime(int *, int *);
int length(const char *);
int weq(const char *, const char *);
void copystr(const char *, char *);
