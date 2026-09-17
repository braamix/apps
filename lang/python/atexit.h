// `atexit`, and what the VM asks of it when the program is done.
#pragma once

#include "obj.h"

// Are there calls still to make?
bool atexit_pending();

// A ContObj that makes them, newest first. Nil with the error pending.
Value atexit_runner();

// Where a callback's exception is reported; the VM writes it to stderr.
void atexit_report(Str text);

bool atexit_install(DictObj *into);
