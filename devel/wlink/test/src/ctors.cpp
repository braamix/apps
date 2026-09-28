// A C++ global object: its constructor is an init function of this file.
#include "fixture.h"

namespace {

struct Greeter {
    Greeter(const char *s) { fx_puts(s); }
};

Greeter greeter("cpp ");

} // namespace
