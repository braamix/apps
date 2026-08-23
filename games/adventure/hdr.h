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

extern int delhit;
extern int adv_status; // what proc_main will return
extern const char *magic;

#define DATSIZE (46 * 1024) // size of encrypted data

#define TAB 011
#define LF  012
#define FLUSHLF          \
    while (next() != LF) \
        ;

extern char *wd1, *wd2; // the complete words

struct hashtab { // hash table for vocabulary
    int val;     // word type &index (ktab)
    int hash;    // 32-bit hash value
};

struct text {
    unsigned short seekadr; // DATFILE must be < 2**16
    unsigned short txtlen;  // length of msg starting here
};

struct travlist {              // direcs & conditions of travel
    struct travlist *next;     // ptr to next list entry
    unsigned short conditions; // m in writeup (newloc / 1000)
    unsigned short tloc;       // n in writeup (newloc % 1000)
    unsigned short tverb;      // the verb that takes you there
};

// Game state.
extern struct game {
    short loc, newloc, oldloc, oldlc2, wzdark, gaveup, kq, k, k2;
    short verb, obj, spk;
    short blklin;
    int saved, savet, mxscor, latncy;

#define MAXSTR 20 // max length of user's words

#define HTSIZE 512              // max number of vocab words
    struct hashtab voc[HTSIZE]; // hash table for vocabulary

#define RTXSIZ 205
    struct text rtext[RTXSIZ]; // random text messages

#define MAGSIZ 35
    struct text mtext[MAGSIZ]; // magic messages

    short clsses;
#define CLSMAX 12
    struct text ctext[CLSMAX]; // classes of adventurer
    short cval[CLSMAX];

    struct text ptext[101]; // object descriptions

#define LOCSIZ 141             // number of locations
    struct text ltext[LOCSIZ]; // long loc description
    struct text stext[LOCSIZ]; // short loc descriptions

    struct travlist *travel[LOCSIZ]; // direcs & conditions of travel

    short atloc[LOCSIZ];

    short plac[101];             // initial object placement
    short fixd[101], fixed[101]; // location fixed?

    short actspk[35]; // rtext msg for verb <n>

    short cond[LOCSIZ]; // various condition bits

    short hntmax;
    short hints[20][5]; // info on hints
    short hinted[20], hintlc[20];

    short place[101], prop[101], plink[201];
    short abb[LOCSIZ];

    short maxtrs, tally, tally2; // treasure values

#define FALSE 0
#define TRUE  1

    short keys, lamp, grate, cage, rod, // mnemonics
        rod2, steps, bird, door, pillow, snake, fissur, tablet, clam, oyster, magzin, dwarf, knife,
        food, bottle, water, oil, plant, plant2, axe, mirror, dragon, chasm, troll, troll2, bear,
        messag, vend, batter, nugget, coins, chest, eggs, tridnt, vase, emrald, pyram, pearl, rug,
        chain, spices, back, look, cave, null, entrnc, dprssn, say, lock, throw_, find, invent;

    short chloc, chloc2, dseen[7], // dwarf stuff
        dloc[7], odloc[7], dflag, daltlc;

    short tk[21], stick, dtotal, attack;
    short turns, lmwarn, iwest, knfloc, // various flags & counters
        detail, abbnum, maxdie, numdie, holdng, dkill, foobar, bonus, clock1, clock2, closng, panic,
        closed, scorng;

    short limit;
} game;

extern struct travlist *tkk; // travel is closer to keys(...)

extern const short setbit[16]; // bit defn masks 1,2,4,...

// An exit from deep in the call graph, which had no way back: done() and its
// callers hand the status up instead, and a negative one says the game is over
// and already reported.
constexpr int ADV_OVER = -1;

void linkdata(void);
Task<i32> startup(void);
void rdata(void);
void mspeak(int);
void rspeak(int);
void speak(struct text *);
int toting(int);
Task<i32> yes(int, int, int);
Task<i32> yesm(int, int, int);
void drop(int, int);
int vocab(const char *, int, int);
void poof(void);
Task<Result<void>> save(Str);
Task<i32> ciao(void);
Task<Result<void>> restore(Str);
Task<i32> start(int);
int length(const char *);
[[noreturn]] void bug(int);
Task<void> getin(char **, char **);
int fdwarf(void);
Task<i32> die(int);
int bitset(int, int);
int forced(int);
int dark(int);
int pct(int);
void pspeak(int, int);
Task<void> checkhints(void);
void copystr(const char *, char *);
Task<i32> done(int);
int closing(void);
int caveclose(void);
int here(int);
int at(int);
int liqloc(int);
int weq(const char *, const char *);
int march(void);
void dstroy(int);
int score(void);
void move(int, int);
void datime(int *, int *);
int trtake(void);
int trdrop(void);
int trsay(void);
int tropen(void);
Task<i32> trkill(void);
int trtoss(void);
int trfill(void);
int trfeed(void);
int liq(int);
int ran(int);
void carry(int, int);
void juggle(int);
int put(int, int, int);
int next(void);
