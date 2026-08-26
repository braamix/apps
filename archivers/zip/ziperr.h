// ziperr.h — Zip 3, by Mark Adler. The table lives in globals.cpp.
#pragma once

// VMS-compatible severity values, bits 2:0.
#define ZE_S_WARNING 0x00
#define ZE_S_SUCCESS 0x01
#define ZE_S_ERROR   0x02
#define ZE_S_INFO    0x03
#define ZE_S_SEVERE  0x04
#define ZE_S_UNUSED  0x07

#define ZE_S_PERR 0x10

// 0..4 and 12..18 follow PKZIP's conventions. PKZIP assigns 4..10 all to
// "insufficient memory", so 5..10 are used here for other purposes.
#define ZE_MISS    -1 // used by procname(), zipbare()
#define ZE_OK      0  // success
#define ZE_EOF     2  // unexpected end of zip file
#define ZE_FORM    3  // zip file structure error
#define ZE_MEM     4  // out of memory
#define ZE_LOGIC   5  // internal logic error
#define ZE_BIG     6  // entry too large to split, read, or write
#define ZE_NOTE    7  // invalid comment format
#define ZE_TEST    8  // zip test (-T) failed or out of memory
#define ZE_ABORT   9  // user interrupt or termination
#define ZE_TEMP    10 // error using a temp file
#define ZE_READ    11 // read or seek error
#define ZE_NONE    12 // nothing to do
#define ZE_NAME    13 // missing or empty zip file
#define ZE_WRITE   14 // error writing to a file
#define ZE_CREAT   15 // couldn't open to write
#define ZE_PARMS   16 // bad command line
#define ZE_OPEN    18 // could not open a specified file to read
#define ZE_COMPERR 19
#define ZE_ZIP64   20

#define ZE_MAXERR 20

struct ziperror {
    const char *name;
    const char *string;
    int severity;
};

extern ZCONST struct ziperror ziperrors[ZE_MAXERR + 1];

// PERR() said whether to call perror(). There is no errno here, so the flag is
// kept for the severity it carries and nothing reads it.
#define PERR(e)      (ziperrors[e].severity & ZE_S_PERR)
#define ZIPERRORS(e) ziperrors[e].string
