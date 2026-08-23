// data.h: the glorkz parser.
//
// Alive only during startup.  What it produces outlives it: Game's tables, and
// the decrypted message text in Game::dat_ that speak() indexes at play time.
//
// oldloc_, obj_ and verb_ are Game's, not this object's, because upstream's
// parser used those globals as its cursors and what it leaves in them is read
// afterwards -- the first march() copies oldloc_ into oldlc2_.
#pragma once

#include "hdr.h"

// glorkz's sections, in the order the file numbers them.
enum class Section : int {
    End      = 0,  // no more data
    Long     = 1,  // long location descriptions
    Short    = 2,  // short ones
    Travel   = 3,  // the travel table
    Vocab    = 4,  // the vocabulary
    Objects  = 5,  // object descriptions, one line per prop value
    Messages = 6,  // what rspeak() says
    Places   = 7,  // initial object locations
    Defaults = 8,  // the default message per verb
    Liquid   = 9,  // the cond_ bits
    Classes  = 10, // the scoring classes
    Hints    = 11, // the hint table
    Magic    = 12, // what mspeak() says
};

class DataReader {
public:
    explicit DataReader(Game &g) : g_(g) {}

    DataReader(const DataReader &)            = delete;
    DataReader &operator=(const DataReader &) = delete;

    void read(); // upstream rdata()

private:
    int next(), rnum();
    void putdat(char), rtrav(), rdesc(Section), rvoc(), rlocs(), rdflt(), rliq(), rhints();

    Game &g_;
    const char *inbuf = nullptr; // the next byte of glorkz
    const char *inend = nullptr;
    const char *tape  = IOTAPE; // pointer to encryption tape
    u32 outadr        = 0;      // where the next byte goes
    int adrptr        = 0;      // current seek adr ptr
    int seekhere      = 1;      // initial seek for output file
    int outsw         = 0;      // putting stuff to data file?
    char breakch      = 0;      // tell which char ended rnum
    char nbf[12]      = {};
};
