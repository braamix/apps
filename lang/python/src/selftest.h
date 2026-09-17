#pragma once

#include "kernel/string.h"

// Run the internal checks, appending a line per case to `out`. False if any
// failed. Reached by `python --selftest`.
bool selftest_run(String &out);
