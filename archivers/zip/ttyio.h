// ttyio.h — the password prompt, by Info-ZIP.
#pragma once

#include "zip.h"

// A password, read without echoing it. Answers p, or NULL if there is no
// terminal to read one from.
Task<char *> getp OF((ZCONST char *m, char *p, int n));
