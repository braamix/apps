// `sys.monitoring`: the numbers PEP 669 fixes, and what the rest of the
// interpreter needs of the registry.
//
// The registry itself is monitormod.cpp. A code object carries the per-tool
// local event sets in a lazily allocated array of MON_LOCAL_EVENTS octets,
// one tool bitmask each, which is CPython's layout.
#pragma once

#include "value.h"

struct CodeObj;

// The event ids, which fix each event's bit in an event set. The order is
// CPython's Include/cpython/monitoring.h and may not be rearranged: a program
// that spells out a number expects these.
enum : u32 {
    MON_PY_START = 0,
    MON_PY_RESUME,
    MON_PY_RETURN,
    MON_PY_YIELD,
    MON_CALL,
    MON_LINE,
    MON_INSTRUCTION,
    MON_JUMP,
    MON_BRANCH_LEFT,
    MON_BRANCH_RIGHT,
    MON_STOP_ITERATION,
    MON_RAISE,
    MON_EXCEPTION_HANDLED,
    MON_PY_UNWIND,
    MON_PY_THROW,
    MON_RERAISE,
    MON_C_RETURN,
    MON_C_RAISE,
    MON_BRANCH,
};

enum : u32 {
    MON_EVENTS       = 19, // every event
    MON_LOCAL_EVENTS = 16, // those a code object can ask for: the first sixteen
    MON_TOOLS        = 8,  // tool ids, of which the last two are not on offer
};

// The four ids PEP 669 hands out, and the two the interpreter keeps for
// sys.setprofile and sys.settrace.
enum : u32 {
    MON_DEBUGGER_ID    = 0,
    MON_COVERAGE_ID    = 1,
    MON_PROFILER_ID    = 2,
    MON_OPTIMIZER_ID   = 5,
    MON_SYS_PROFILE_ID = 6,
    MON_SYS_TRACE_ID   = 7,
};

// The module object `sys.monitoring` names. Nil with the error pending.
Value monitoring_new();

// The global event set a tool has asked for, and the callback it registered.
u32 mon_tool_events(u32 tool);
Value mon_callback(u32 tool, u32 event);

// Whether anything is watching at all: the one test the instruction loop
// makes before it looks any closer.
bool mon_armed();

// What `tool` wants of this code object: what it asked for globally, and
// what it asked for on this one.
u32 mon_events_for(const CodeObj *c, u32 tool);

// The DISABLE marker, which a callback returns to ask not to be called again.
Value mon_disable();

// The per-code half, defined in code.cpp beside the array it reads.
u32 mon_local_events(const CodeObj *c, u32 tool);
bool mon_set_local_events(CodeObj *c, u32 tool, u32 events);

// What a restart undoes. Nothing yet: no event fires, so none is disabled.
void mon_restart();
