// The definition, with the signature of the call.
#include "kernel/task.h"

Task<int> fx_taken(int a, int b)
{
    co_return a * 10 + b;
}
