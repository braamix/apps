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

#include <stdlib.h>
#include "msg.h"
#include "proc/io.h"

constexpr int TAB = 011;
constexpr int LF  = 012;

inline constexpr char IOTAPE[] = "Ax3F'tt$8hqer*hnGKrX:!l"; // encryption tape

constexpr int FALSE = 0;
constexpr int TRUE  = 1;

constexpr i32 DATSIZE  = 46 * 1024; // size of encrypted data
constexpr i32 MAXSTR   = 20;        // max length of user's words
constexpr int WORD_SIG = 5;         // glorkz's significant letters: hash and weq agree on it
constexpr i32 HTSIZE   = 512;       // max number of vocab words
constexpr i32 RTXSIZ   = 205;
constexpr i32 MAGSIZ   = 35;
constexpr i32 CLSMAX   = 12;
constexpr i32 LOCSIZ   = 141; // number of locations
constexpr i32 OBJSIZ   = 101;
constexpr i32 HINTSIZ  = 20;

struct HashTab { // hash table for vocabulary
    i32 val;     // word type &index (ktab)
    i32 hash;    // 32-bit hash value
};

struct Text {
    u16 seekadr; // DATFILE must be < 2**16
    u16 txtlen;  // length of msg starting here
};

// clang-format off
// A vocabulary word worth 2000 + n in glorkz is verb n.  linkdata() checks the
// naming against the data file.
enum class Verb : i16 {
    None = 0,       // signed, because rdflt() leaves -1 here
    Take = 1, Drop, Say, Open, Nothing, Lock, On, Off, Wave, Calm, Walk, Kill, Pour, Eat, Drink,
    Rub, Throw, Quit, Find, Invent, Feed, Fill, Blast, Score, Foo, Brief, Read, Break, Wake,
    Suspend, Hours,
};
// clang-format on

// clang-format off
// A vocabulary word under 1000 is motion n.  Travel::tverb holds one, and so
// does k_ once lookup() has classified a word as one.
enum class Motion : i16 {
    Forced = 1,     // no word: this exit fires whatever was typed
    Road = 2, Enter, Upstream, Downstream, Forest, Forward, Back, Valley, Stairs, Out, Building,
    Gully, Stream, Rock, Bed, Crawl, Cobbles, In, Surface, Null, Dark, Passage, Low, Canyon,
    Awkward, Giant, View, Up, Down, Pit, Outdoors, Crack, Steps, Dome, Left, Right, Hall, Jump,
    Barren, Over, Across, East, West, North, South, NE, SE, SW, NW, Debris, Hole, Wall, Broken,
    Y2, Climb, Look, Floor, Room, Slit, Slab, Xyzzy, Depression, Entrance, Plugh, Secret, Cave,
    // 68 has no word.
    Cross = 69, Bedquilt, Plover, Oriental, Cavern, Shell, Reservoir, Main, Fork,
};
// clang-format on

struct Travel {     // direcs & conditions of travel
    u16 conditions; // m in writeup (newloc / 1000)
    u16 tloc;       // n in writeup (newloc % 1000)
    Motion tverb;   // the verb that takes you there
};

// An exit from deep in the call graph, which had no way back: done() and its
// callers hand the status up instead, and a negative one says the game is over
// and already reported.
constexpr int ADV_OVER = -1;

// What the routines below Phase return, named for the statement label it was.
enum class Move : u8 {
    Turn,     // 2     back to the top of the turn
    Found,    // 9     mback(): the exit is under the cursor
    Next,     // 12    specials(): try the next travel entry
    Died,     // 99    to die()
    Describe, // 2000  the room again
    Over,     // die(): the game ended and said so
};

// The object space.  An object is 1..MAXOBJ; obj + FIXED addresses the same
// object's immovable half, which is what fixed_ holds and place_ does not.
constexpr int MAXOBJ   = OBJSIZ - 1;
constexpr int FIXED    = 100;
constexpr int TREASURE = 50; // the first treasure; maxtrs_ is the last
constexpr int CARRIED  = -1; // place_[obj] while it is in your hands
constexpr int PINNED   = -1; // fixed_[obj] once it may not be picked up again

// glorkz gives a word the number WORD_CLASS * class + n, which is what the two
// enums above number within.  vocab()'s two other type arguments are not
// classes at all.
enum class WordClass : int { Motion = 0, Object = 1, Verb = 2, Message = 3 };

constexpr int WORD_CLASS = 1000;
constexpr int VOC_STORE  = -2; // fill in an entry rather than look one up
constexpr int VOC_USER   = -1; // look up whatever the player typed, any class

constexpr int word_of(WordClass c, int n)
{
    return WORD_CLASS * int(c) + n;
}

// The rooms the code names.  glorkz decides the numbers; check_locs() ties the
// ones it can to the data file.  Scoped but not enum class: a location is
// arithmetic here as much as it is an identity -- Alcove + PloverRoom - loc_ is
// how the tunnel between those two swaps ends.
namespace Loc {
enum : i16 {
    Road            = 1, // end of road before the small brick building
    Building        = 3, // inside the well house
    Valley          = 4, // the valley beside the stream
    Slit            = 7, // where the stream vanishes into the slit
    OutsideGrate    = 8, // the 20-foot depression, and the last room above ground
    BelowGrate      = 9, // the chamber beneath the grate
    BirdChamber     = 13,
    HallOfMists     = 15, // and from here down the dwarves walk
    NoteRoom        = 18, // the low room with the crude note: daltlc_
    HallOfMtKing    = 19,
    OilPit          = 24, // the bottom of the eastern pit
    WestOfFissure   = 27,
    Y2              = 33, // where a hollow voice says "plugh"
    MazeAllAlike    = 44,
    ComplexJunction = 64,
    Alcove          = 99,  // the plover tunnel's near end
    PloverRoom      = 100, // and its far end
    CulDeSac        = 105, // where the pearl rolls
    WittsEnd        = 108, // and the magazine's one point
    ChestDeadEnd    = 114, // the pirate's chest: chloc_
    RepositoryNE    = 115, // the endgame
    RepositorySW    = 116,
    MessageDeadEnd  = 140, // where the pirate leaves his note: chloc2_
};
} // namespace Loc

// pspeak()'s skip: print the object's first line, whatever its prop.
constexpr int PS_FIRST_LINE = -1;

// How many exits fdwarf() will weigh for one dwarf; tk_ is one longer.
constexpr int DWARF_EXITS = 20;

// dloc_, odloc_ and dseen_ hold the five dwarves and, at PIRATE, the pirate.
constexpr int NDWARVES = 5;
constexpr int PIRATE   = NDWARVES + 1;

// dflag_, which counts up as the dwarves take an interest.  Scoped but not
// enum class: fdwarf() does arithmetic on it and trfeed() increments it.
namespace Dwarves {
enum : i16 {
    Asleep  = 0,  // not in the cave yet
    Woken   = 1,  // the player has reached the hall of mists
    Active  = 2,  // one has thrown an axe, and they can be met
    Angry   = 3,  // and now they throw knives
    Furious = 20, // fed, or the wizard let a suspended game resume
};
} // namespace Dwarves

// A travel entry's conditions m, in bands of a hundred:
//      0       always
//      1..99   that percent of the time
//      100     never -- carrying object 0
//      100+n   only if carrying object n
//      200+n   only if object n is here
//      300+n   and up, only if prop[n] == m / COND_BAND - 3
constexpr int COND_BAND  = 100; // and m % COND_BAND is the object
constexpr int COND_NEVER = 100;
constexpr int COND_CARRY = 1 * COND_BAND;
constexpr int COND_AT    = 2 * COND_BAND;
constexpr int COND_PROP  = 3 * COND_BAND;

// And its destination n: a location, else a Special, else Msg(n - TRAV_SPEC).
constexpr int TRAV_LOC  = 300;
constexpr int TRAV_SPEC = 500;

enum class Special : u8 { PloverTunnel = 1, DropEmerald = 2, TrollBridge = 3 };

// die()'s entry: Pit says how, Elsewhere means the caller has said already.
enum class Death : u8 { Pit = 90, Elsewhere = 99 };

// done()'s entry, and the label it was.  Cave has no caller: upstream's
// 13000 is unreachable, and its Magic::GreenSmoke with it.
enum class Done : u8 { Cave = 1, Score = 2, Dwarves = 3 }; // 13000, 20000, 19000

// The low four bits of cond_, set by glorkz section 9.
enum class CondBit : i16 {
    Lit     = 0, // lit without the lamp
    Oil     = 1, // the liquid here is oil, not water
    Fluid   = 2, // a liquid is here
    NoDwarf = 3, // no dwarf and no pirate comes here
};

// cond_ with none of those bits: a forced move.  linkdata() writes it,
// forced() reads it.
constexpr i16 COND_FORCED = 2;

// A hint's number is also its bit in cond_: glorkz section 9 sets bit 4 on the
// grate room, 5 on the bird chamber and so on, which is what bitset(loc_, hint)
// tests.  Bits 0 to 3 are CondBit above, so a location-triggered hint cannot
// number below 4.
enum class Hint : i16 {
    Oyster       = 2, // the clue inside it, taken by hand in verb_object()
    Instructions = 3, // "Would you like instructions?", taken in startup()
    Grate        = 4, // outside the grate
    Bird         = 5, // the bird chamber
    Snake        = 6, // the hall of the mountain king
    Maze         = 7, // the all-alike maze
    Plover       = 8, // beyond the plover room
    WittsEnd     = 9, // and how to get out of it
};

// One row of glorkz section 11.  Upstream's first column held the hint's own
// number; the row index is that here, and the slot went with it.
struct HintRule {
    i16 turns    = 0;         // turns in the region before it is offered
    i16 cost     = 0;         // points it costs to take
    Msg question = Msg::None; // "Are you trying to catch the bird?"
    Msg answer   = Msg::None; // what it tells you
};

// One step of a turn, named for the FORTRAN statement label it was.  Upstream
// jumped between these with goto and the tr* handlers returned the number the
// caller switched back into one; here a step returns the next step and play()
// dispatches.
enum class Phase : u8 {
    Dwarves,    // 2      the dwarves move, then the room
    Describe,   // 2000   the room and what is in it
    EndTurn,    // 2012
    Hints,      // 2600   hints, then read a command
    Timers,     // 2608   the clocks and the lamp
    Lamp,       // 19999  liqloc, and the word rewrites
    West,       // 2610
    Lookup,     // 2630   which kind of word it was
    Motion,     // 8      march
    Shift,      // 2800   wd1 = wd2
    VerbOnly,   // 4000
    VerbObject, // 4090
    ObjectOnly, // 5000
    Named,      // 5010
    Drop,       // 9020   the four a tr* handler re-enters with another verb
    Kill,       // 9120
    Feed,       // 9210
    Fill,       // 9220
    Done3,      // done(Done::Dwarves), from trdrop() which cannot await
    Over,       // finished, and it has said so
};

// The game.  Upstream's globals are the state, and its routines are the
// methods; a field carries a trailing underscore so neither hides the other.
class Game {
    friend class DataReader; // fills the tables from glorkz

public:
    // Sizes every table.  The one place that can run out of memory: after it
    // nothing grows but the travel lists, so every index below is unchecked,
    // exactly as the C arrays were.
    bool alloc();

    Task<i32> play(Args args); // upstream's main()

private:
    // ---- the command loop, one method per label -- main.cpp -----------
    Phase closing(), caveclose();
    Phase end_turn(), lamp(), west(), lookup(), shift();
    Phase object_only(), named(), no_see();
    Task<Phase> dwarves(), describe(), hints_then_read(), timers(), motion();
    Task<Phase> verb_only(), verb_object();

    // 2009 -> 2010 -> 2011 -> 2012 is a fallthrough chain with no branching,
    // so it stays one as a chain of calls.  8000 and 8142 are the same shape.
    Phase nothing() { return k_ = i16(Msg::Ok), speak_k(); }                    // 2009
    Phase speak_k() { return spk_ = Msg(k_), report(); }                        // 2010
    Phase report() { return rspeak(spk_), Phase::EndTurn; }                     // 2011
    Phase what();                                                               // 8000
    Phase eat_food() { return dstroy(food_), spk_ = Msg::Delicious, report(); } // 8142

    // ---- setup -- init.cpp --------------------------------------------
    void linkdata(), check_vocab(), check_msgs(), check_hints(), check_locs();
    Task<i32> startup();

    // ---- messages and input -- io.cpp ---------------------------------
    void speak(Text *), pspeak(int, int), rspeak(Msg), mspeak(Magic);
    Task<void> getin(); // fills wd1_ and wd2_
    Task<i32> yes(Msg, Msg, Msg), yesm(Magic, Magic, Magic);

    // ---- the objects -- vocab.cpp -------------------------------------
    void dstroy(int), juggle(int), move(int, int), carry(int, int), drop(int, int);
    int put(int, int, int), vocab(const char *, int, int);

    // The canned lookup: the word must be in this class, and its n comes back.
    int vocab(const char *word, WordClass c) { return vocab(word, int(c), 0); }

    // ---- statement functions -- subr.cpp ------------------------------
    int toting(int), here(int), at(int), liq2(int), liq(int), liqloc(int);
    int bitset(int, int), forced(int), dark(int), pct(int);

    // ---- the dwarves and the travel table -- move.cpp -----------------
    Move fdwarf(), mback(), badmove(), trbridge(), specials(), march();
    void tk_to(int loc) { tkk_loc_ = i16(loc), tkk_ = 0; }
    bool tk_end() const { return usize(tkk_) >= travel_[tkk_loc_].size(); }
    const Travel &tk() const { return travel_[tkk_loc_][usize(tkk_)]; }

    // ---- the hints and the verb handlers -- verbs.cpp -----------------
    Task<void> checkhints();
    Phase trsay(), trtake(), trdrop(), tropen(), trtoss(), trfeed(), trfill();
    Phase dropper();
    Task<Phase> trkill();

    // ---- privileged operations -- wizard.cpp --------------------------
    void poof();
    Task<i32> wizard(), start(int), ciao();

    // ---- scoring and the end -- done.cpp ------------------------------
    int score();
    Task<void> done(Done);
    Task<Move> die(Death);

    // ---- suspend and resume -- save.cpp, serial.h ---------------------
    Task<Result<void>> save(Str), restore(Str);
    template <class V>
    void visit(V &v); // the one field list, in serial.h

    // ================= state ===========================================
    i16 loc_ = 0, newloc_ = 0, oldloc_ = 0, oldlc2_ = 0, wzdark_ = 0, gaveup_ = 0, kq_ = 0, k_ = 0;
    i16 obj_    = 0;
    Msg spk_    = Msg::None;
    Verb verb_  = Verb::None;
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

    Vec<Msg> actspk_; // rtext msg for verb <n>

    Vec<i16> cond_; // various condition bits

    i16 hntmax_ = 0;
    Vec<HintRule> hints_; // info on hints, by Hint
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
        emrald_ = 0, pyram_ = 0, pearl_ = 0, rug_ = 0, chain_ = 0, back_ = 0, look_ = 0, cave_ = 0,
        null_ = 0, entrnc_ = 0, dprssn_ = 0;

    // Typed, so verb_ == say_ needs no cast.
    Verb say_ = Verb::None, lock_ = Verb::None, throw_ = Verb::None, find_ = Verb::None,
         invent_ = Verb::None;

    i16 chloc_ = 0, chloc2_ = 0, dflag_ = 0, daltlc_ = 0; // dwarf stuff
    Vec<i16> dseen_, dloc_, odloc_;

    Vec<i16> tk_; // scratch for fdwarf()
    i16 stick_ = 0, dtotal_ = 0, attack_ = 0;
    i16 turns_ = 0, lmwarn_ = 0, iwest_ = 0, knfloc_ = 0, // various flags & counters
        detail_ = 0, abbnum_ = 0, maxdie_ = 0, numdie_ = 0, holdng_ = 0, dkill_ = 0, foobar_ = 0,
        clock1_ = 0, clock2_ = 0, closng_ = 0, panic_ = 0, closed_ = 0, scorng_ = 0;

    Msg bonus_ = Msg::None; // the explosion ending, and the points for it
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
int weq(const char *, const char *);
void copystr(const char *, char *);
