// The error channel: a sticky flag plus a message, checked rather than thrown.
// That is ground rule 3, and it is what every operation here reports through.
//
// From phase 7 the channel can also hold the exception *object* a `raise`
// named. The kind and the message stay, because a few hundred call sites say
// `err_set("TypeError", ...)` and building an object for an error that is
// about to be printed and forgotten would be waste; the VM materialises one
// only where an `except` might want it.
#pragma once

#include "kernel/str.h"
#include "kernel/string.h"
#include "value.h"

// NotImpl is not a failure: this type does not do that, and the caller may
// try the other operand or raise its own.
enum class R : u8 { Ok, Err, NotImpl };

// Set the pending error. Always returns R::Err, to be returned straight on.
R err_set(Str kind, Str message);

// With a place in the source, for the lexer and the parser.
R err_set_at(Str kind, Str message, u32 line, u32 col);

// With one detail appended after ": " -- a type name, a key, an operator.
R err_set2(Str kind, Str message, Str detail);

// The pending error is this exception object. Always returns R::Err. `kind`,
// when given, is what err_kind() answers for it.
R err_set_value(Value v, Str kind = Str());

// The object a raise named, or Nil when the error came from a kind and a
// message and nobody has needed an object for it.
Value err_value();

// A root: the pending exception outlives the operation that set it.
void err_mark();

bool err_pending();
u32 err_line();
u32 err_col();
Str err_kind();
Str err_message();
void err_clear();

// "TypeError: unsupported operand" -- kind, colon, message.
void err_format(String &out);
