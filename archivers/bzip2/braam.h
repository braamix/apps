// Braam port layer for bzip2 1.0.8.
#pragma once

#define BZ_UNIX     1
#define BZ_LCCWIN32 0

#include <bzlib.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "compat/cio.h"
#include "fs/path.h"
#include "kernel/types.h"
#include "proc/io.h"

#define PATH_SEP   '/'
#define MY_LSTAT   b_lstat
#define MY_STAT    b_stat
#define MY_S_ISREG S_ISREG
#define MY_S_ISDIR S_ISDIR

typedef char Char;
typedef unsigned char Bool;
typedef unsigned char UChar;
typedef int Int32;
typedef unsigned int UInt32;
typedef short Int16;
typedef unsigned short UInt16;
typedef int IntNative;

#define True  ((Bool)1)
#define False ((Bool)0)

#define NORETURN

extern Int32 verbosity;
extern Bool keepInputFiles, smallMode, deleteOutputOnInterrupt;
extern Bool forceOverwrite, testFailsExist, unzFailsExist, noisy;
extern Int32 numFileNames, numFilesProcessed, blockSize100k;
extern Int32 exitValue;
extern Int32 opMode;
extern Int32 srcMode;
extern Int32 longestFileName;
extern Char inName[];
extern Char outName[];
extern Char tmpName[];
extern Char *progName;
extern Char progNameReally[];
extern FILE *outputHandleJustInCase;
extern Int32 workFactor;

extern int b2_fatal;
extern const char *b2_remove_out;

Task<i32> bzip2_main(Args args);

Task<void> b2_flush_diag();
void b2_set_exit(Int32 v);
Task<void> b2_on_int();

void b2_maybe_err(const char *fmt, ...);
void b2_maybe_errx(const char *fmt, ...);
