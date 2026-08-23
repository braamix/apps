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
#include "kernel/vec.h"
#include "proc/io.h"

constexpr int TAB = 011;
constexpr int LF  = 012;

inline constexpr char IOTAPE[] = "Ax3F'tt$8hqer*hnGKrX:!l"; // encryption tape

constexpr int FALSE = 0;
constexpr int TRUE  = 1;

constexpr i32 DATSIZE = 46 * 1024; // size of encrypted data
constexpr i32 MAXSTR  = 20;        // max length of user's words
constexpr i32 HTSIZE  = 512;       // max number of vocab words
constexpr i32 RTXSIZ  = 205;
constexpr i32 MAGSIZ  = 35;
constexpr i32 CLSMAX  = 12;
constexpr i32 LOCSIZ  = 141; // number of locations
constexpr i32 OBJSIZ  = 101;
constexpr i32 HINTSIZ = 20;

struct HashTab { // hash table for vocabulary
    i32 val;     // word type &index (ktab)
    i32 hash;    // 32-bit hash value
};

struct Text {
    u16 seekadr; // DATFILE must be < 2**16
    u16 txtlen;  // length of msg starting here
};

struct Travel {     // direcs & conditions of travel
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
    friend class DataReader; // fills the tables from glorkz

    // Sizes every table.  The one place that can run out of memory: after it
    // nothing grows but the travel lists, so every index below is unchecked,
    // exactly as the C arrays were.
    bool alloc();

    Task<i32> play(Args args); // upstream's main()

private:
    // ---- the command loop -- main.cpp ---------------------------------
    int closing(), caveclose();

    // ---- setup -- init.cpp --------------------------------------------
    void linkdata();
    Task<i32> startup();

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
    void tk_to(int loc) { tkk_loc_ = i16(loc), tkk_ = 0; }
    bool tk_end() const { return usize(tkk_) >= travel_[tkk_loc_].size(); }
    const Travel &tk() const { return travel_[tkk_loc_][usize(tkk_)]; }

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

    // ---- suspend and resume -- save.cpp, serial.h ---------------------
    Task<Result<void>> save(Str), restore(Str);
    template <class V>
    void visit(V &v); // the one field list, in serial.h

    // ================= state ===========================================
    i16 loc_ = 0, newloc_ = 0, oldloc_ = 0, oldlc2_ = 0, wzdark_ = 0, gaveup_ = 0, kq_ = 0, k_ = 0,
        k2_   = 0;
    i16 verb_ = 0, obj_ = 0, spk_ = 0;
    i16 blklin_ = 0;
    i32 saved_ = 0, savet_ = 0, mxscor_ = 0, latncy_ = 0;

    Vec<HashTab> voc_; // hash table for vocabulary

    Vec<Text> rtext_; // random text messages, RTXSIZ + 1: rdesc() lets 205 through

    Vec<Text> mtext_; // magic messages, MAGSIZ + 1 for the same reason

    i16 clsses_ = 0;
    Vec<Text> ctext_; // classes of adventurer
    Vec<i16> cval_;

    Vec<Text> ptext_; // object descriptions

    // The location tables are LOCSIZ + 1: linkdata() runs to i <= LOCSIZ, and
    // reads ltext_ there before the && short-circuits on travel_.
    Vec<Text> ltext_; // long loc description
    Vec<Text> stext_; // short loc descriptions

    Vec<Vec<Travel>> travel_; // direcs & conditions of travel, by location

    Vec<i16> atloc_;

    Vec<i16> plac_;         // initial object placement
    Vec<i16> fixd_, fixed_; // location fixed?

    Vec<i16> actspk_; // rtext msg for verb <n>

    Vec<i16> cond_; // various condition bits

    i16 hntmax_ = 0;
    Vec<Vec<i16>> hints_; // info on hints, HINTSIZ x 5
    Vec<i16> hinted_, hintlc_;

    Vec<i16> place_, prop_, plink_;
    Vec<i16> abb_;

    i16 maxtrs_ = 0, tally_ = 0, tally2_ = 0; // treasure values

    i16 keys_ = 0, lamp_ = 0, grate_ = 0, cage_ = 0, rod_ = 0, // mnemonics
        rod2_ = 0, steps_ = 0, bird_ = 0, door_ = 0, pillow_ = 0, snake_ = 0, fissur_ = 0,
        tablet_ = 0, clam_ = 0, oyster_ = 0, magzin_ = 0, dwarf_ = 0, knife_ = 0, food_ = 0,
        bottle_ = 0, water_ = 0, oil_ = 0, plant_ = 0, plant2_ = 0, axe_ = 0, mirror_ = 0,
        dragon_ = 0, chasm_ = 0, troll_ = 0, troll2_ = 0, bear_ = 0, messag_ = 0, vend_ = 0,
        batter_ = 0, nugget_ = 0, coins_ = 0, chest_ = 0, eggs_ = 0, tridnt_ = 0, vase_ = 0,
        emrald_ = 0, pyram_ = 0, pearl_ = 0, rug_ = 0, chain_ = 0, spices_ = 0, back_ = 0,
        look_ = 0, cave_ = 0, null_ = 0, entrnc_ = 0, dprssn_ = 0, say_ = 0, lock_ = 0, throw_ = 0,
        find_ = 0, invent_ = 0;

    i16 chloc_ = 0, chloc2_ = 0, dflag_ = 0, daltlc_ = 0; // dwarf stuff
    Vec<i16> dseen_, dloc_, odloc_;

    Vec<i16> tk_; // scratch for fdwarf()
    i16 stick_ = 0, dtotal_ = 0, attack_ = 0;
    i16 turns_ = 0, lmwarn_ = 0, iwest_ = 0, knfloc_ = 0, // various flags & counters
        detail_ = 0, abbnum_ = 0, maxdie_ = 0, numdie_ = 0, holdng_ = 0, dkill_ = 0, foobar_ = 0,
        bonus_ = 0, clock1_ = 0, clock2_ = 0, closng_ = 0, panic_ = 0, closed_ = 0, scorng_ = 0;

    i16 limit_ = 0;

    Vec<char> dat_; // the decrypted text, indexed by speak() and pspeak()

    // ---- what upstream kept beside the state struct --------------------
    // travel is closer to keys(...).  Upstream walked a linked list; here the
    // cursor is a list and an index into it, and index == size() is its null.
    i16 tkk_loc_       = 0;
    i32 tkk_           = 0;
    char wd1_[MAXSTR]  = {}; // the complete words
    char wd2_[MAXSTR]  = {};
    int delhit_        = 0; // user typed a DEL
    const char *magic_ = nullptr;
    i32 status_        = 0; // what proc_main will return

    static const i16 SETBIT[16]; // bit defn masks 1,2,4,...
};

// Free: pure, or defined in braam.cpp, which must not include this header.
[[noreturn]] void bug(int);
int ran(int);
void datime(int *, int *);
int length(const char *);
int atoi(const char *);
int weq(const char *, const char *);
void copystr(const char *, char *);
