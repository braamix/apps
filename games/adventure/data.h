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

class DataReader {
public:
    explicit DataReader(Game &g) : g_(g) {}

    DataReader(const DataReader &)            = delete;
    DataReader &operator=(const DataReader &) = delete;

    void read(); // upstream rdata()

private:
    int next(), rnum();
    void putdat(char), rtrav(), rdesc(int), rvoc(), rlocs(), rdflt(), rliq(), rhints();

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
