// Where the package's own share directory is: what PKGDATADIR was.
#pragma once

#include "kernel/task.h"

// The package's share/le directory, empty for none.
extern char datadir[];

Task<void> epath_init();

// datadir + "/" + name, into a caller's buffer. Empty when there is no
// datadir, which is a name every caller then skips.
const char *datafile(char *buf, unsigned size, const char *name);
