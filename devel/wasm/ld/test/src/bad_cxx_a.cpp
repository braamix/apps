// C++ names in refusals: a duplicate definition and an undefined reference,
// each reported demangled.
#include "fixture.h"

namespace ns {
int twice(int x)
{
    return x + x;
}
int missing(const char *);
} // namespace ns

extern "C" void fx_main(void)
{
    fx_putn(ns::twice(ns::missing("x")));
}
