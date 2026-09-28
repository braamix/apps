// A coroutine whose address alone is taken: clang gives its import a
// placeholder signature, () -> void.
#include "fixture.h"
#include "kernel/task.h"

Task<int> fx_taken(int a, int b);

typedef Task<int> (*Taken)(int, int);
Taken fx_taken_table[] = { fx_taken };
