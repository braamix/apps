// The error channel: a sticky flag plus a message, checked rather than thrown.
// Phase 7 replaces the message with an exception object; the mechanism is the
// same, and is ground rule 3.
#pragma once

#include "kernel/str.h"
#include "kernel/string.h"

// NotImpl is not a failure: this type does not do that, and the caller may
// try the other operand or raise its own.
enum class R : u8 { Ok, Err, NotImpl };

// Set the pending error. Always returns R::Err, to be returned straight on.
R err_set(Str kind, Str message);

// With one detail appended after ": " -- a type name, a key, an operator.
R err_set2(Str kind, Str message, Str detail);

bool err_pending();
Str err_kind();
Str err_message();
void err_clear();

// "TypeError: unsupported operand" -- kind, colon, message.
void err_format(String &out);
