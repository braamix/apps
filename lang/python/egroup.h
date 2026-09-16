// Exception groups (PEP 654): BaseExceptionGroup, ExceptionGroup, and what
// `except*` asks of them at run time.
#pragma once

#include "exc.h"

// The instance is a BaseExceptionGroup, or a class under it.
bool is_egroup(Value v);

// The class is BaseExceptionGroup, or under it.
bool is_egroup_type(Value cls);

// `cls(msg, excs)`, with CPython's checks and its choice of ExceptionGroup
// where every member is an Exception. `args` is the call's tuple.
Value egroup_new(Value cls, Value args);

// derive, split, subgroup and __new__ into the two types' namespaces, and
// add_note into BaseException's.
bool egroup_install();

// CheckEgMatch: a tuple (rest, match) for `except* type` over `exc`, or a
// ContObj answering one -- a group is split by its own split method.
R egroup_match(Value exc, Value type, Value &out);

// PrepReraiseStar: what an except* statement raises once its clauses are
// done, from the exception it caught and what they raised. None for nothing.
Value egroup_reraise(Value orig, Value raised);

// The box CPython draws round an uncaught group, below its own line.
bool egroup_report(Value e, String &out);
