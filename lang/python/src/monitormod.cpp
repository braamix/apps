// `sys.monitoring`: PEP 669's tool registry.
//
// A module object hanging off `sys`, as CPython's is, and for now it is
// bookkeeping only -- a tool takes an id, registers callbacks and names the
// events it wants, and nothing fires. `bdb` reads `sys.monitoring.events` at
// import and that is what this is here for; task 32 of TODO.md makes the VM
// call what is registered here.
//
// The numbers are CPython's: eight tool ids of which six may be asked for,
// nineteen events of which the first sixteen can be asked for per code
// object, and each event is a power of two in an event set.
#include "builtin.h"
#include "code.h"
#include "func.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "monitor.h"
#include "obj.h"
#include "ops.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// The events, in the order that fixes each one's bit. The last three are
// derived from the others and cannot be asked for per code object.
constexpr Str EVENT_NAMES[MON_EVENTS] = {
    "PY_START",  "PY_RESUME", "PY_RETURN", "PY_YIELD",  "CALL",
    "LINE",      "INSTRUCTION", "JUMP",    "BRANCH_LEFT", "BRANCH_RIGHT",
    "STOP_ITERATION", "RAISE", "EXCEPTION_HANDLED", "PY_UNWIND", "PY_THROW",
    "RERAISE",   "C_RETURN",  "C_RAISE",   "BRANCH",
};

enum : u32 {
    C_RETURN_EVENTS = (1u << MON_C_RETURN) | (1u << MON_C_RAISE),
    C_CALL_EVENTS   = C_RETURN_EVENTS | (1u << MON_CALL),
};

// The tools, their callbacks and what each has asked for. One of these, made
// on first use and rooted for the life of the process.
struct Home {
    Value names[MON_TOOLS];                 // Nil where the id is free
    Value callbacks[MON_TOOLS][MON_EVENTS]; // Nil where none is registered
    u32 events[MON_TOOLS];                  // the global event set
    Value disable;                          // the DISABLE sentinel
    Value missing;                          // the MISSING sentinel
    bool anylocal;                          // a code object has been asked about
};

Home *home;

void home_mark()
{
    if (!home)
        return;
    for (u32 t = 0; t < MON_TOOLS; t++) {
        gc_mark(home->names[t]);
        for (u32 e = 0; e < MON_EVENTS; e++)
            gc_mark(home->callbacks[t][e]);
    }
    gc_mark(home->disable);
    gc_mark(home->missing);
}

// An id a program may ask for: the last two are sys.settrace's and
// sys.setprofile's and are not on offer.
bool valid_tool(i64 id)
{
    if (id >= 0 && id < MON_SYS_PROFILE_ID)
        return true;
    char t[24];
    Buf<64> b;
    b.put("invalid tool ").put(int_text(t, sizeof t, id)).put(" (must be between 0 and 5)");
    err_set("ValueError", b.str());
    return false;
}

// One that is also in use, which is what everything but use_tool_id wants.
bool tool_in_use(i64 id)
{
    if (!valid_tool(id))
        return false;
    if (!home->names[id].is_nil())
        return true;
    char t[24];
    Buf<64> b;
    b.put("tool ").put(int_text(t, sizeof t, id)).put(" is not in use");
    err_set("ValueError", b.str());
    return false;
}

// The first argument of every call here.
bool tool_arg(const CallArgs &a, i64 &id)
{
    if (!as_int_arg(a.args[0], id))
        return err_set("TypeError", "an integer is required") == R::Ok;
    return true;
}

R m_use_tool_id(const CallArgs &a, Value &out)
{
    i64 id = 0;
    if (!args_only(a, "use_tool_id", 2, 2) || !tool_arg(a, id) || !valid_tool(id))
        return R::Err;
    if (!is_str(a.args[1]))
        return err_set("ValueError", "tool name must be a str");
    if (!home->names[id].is_nil()) {
        char t[24];
        Buf<48> b;
        b.put("tool ").put(int_text(t, sizeof t, id)).put(" is already in use");
        return err_set("ValueError", b.str());
    }
    home->names[id] = a.args[1];
    out             = value_none();
    return R::Ok;
}

// The callbacks and the global events, dropped; the name stays, and so do
// the local event sets, which is what CPython's ClearToolId leaves behind.
void clear_tool(i64 id)
{
    for (u32 e = 0; e < MON_EVENTS; e++)
        home->callbacks[id][e] = Value();
    home->events[id] = 0;
}

R m_clear_tool_id(const CallArgs &a, Value &out)
{
    i64 id = 0;
    if (!args_only(a, "clear_tool_id", 1, 1) || !tool_arg(a, id) || !valid_tool(id))
        return R::Err;
    if (!home->names[id].is_nil())
        clear_tool(id);
    out = value_none();
    return R::Ok;
}

R m_free_tool_id(const CallArgs &a, Value &out)
{
    i64 id = 0;
    if (!args_only(a, "free_tool_id", 1, 1) || !tool_arg(a, id) || !valid_tool(id))
        return R::Err;
    if (!home->names[id].is_nil())
        clear_tool(id);
    home->names[id] = Value();
    out             = value_none();
    return R::Ok;
}

R m_get_tool(const CallArgs &a, Value &out)
{
    i64 id = 0;
    if (!args_only(a, "get_tool", 1, 1) || !tool_arg(a, id) || !valid_tool(id))
        return R::Err;
    out = home->names[id].is_nil() ? value_none() : home->names[id];
    return R::Ok;
}

// The one bit an event set holds, or -1 for none or more than one.
i32 one_event(u32 set)
{
    i32 at = -1;
    for (u32 e = 0; e < 32; e++)
        if (set & (1u << e)) {
            if (at >= 0)
                return -1;
            at = i32(e);
        }
    return at;
}

R m_register_callback(const CallArgs &a, Value &out)
{
    i64 id = 0, ev = 0;
    if (!args_only(a, "register_callback", 3, 3) || !tool_arg(a, id) || !valid_tool(id))
        return R::Err;
    if (!as_int_arg(a.args[1], ev))
        return err_set("TypeError", "an integer is required");
    i32 e = ev > 0 && ev <= 0xffffffff ? one_event(u32(ev)) : -1;
    if (e < 0)
        return err_set("ValueError", "The callback can only be set for one event at a time");
    if (e >= i32(MON_EVENTS)) {
        char t[24];
        Buf<48> b;
        b.put("invalid event ").put(int_text(t, sizeof t, ev));
        return err_set("ValueError", b.str());
    }
    Value was                = home->callbacks[id][e];
    home->callbacks[id][e]   = is_none(a.args[2]) ? Value() : a.args[2];
    out                      = was.is_nil() ? value_none() : was;
    return R::Ok;
}

R m_get_events(const CallArgs &a, Value &out)
{
    i64 id = 0;
    // Only that the id is one a program may ask for: reading what an unused
    // tool wants is not an error, and the answer is nothing.
    if (!args_only(a, "get_events", 1, 1) || !tool_arg(a, id) || !valid_tool(id))
        return R::Err;
    out = Value::of_int(i32(home->events[id]));
    return R::Ok;
}

// "invalid event set 0x1234", which CPython writes in hexadecimal.
bool bad_set(Str what, i64 n)
{
    Buf<64> b;
    b.put("invalid ").put(what).put(" 0x");
    char t[16];
    usize at = sizeof t;
    u64 v    = u64(n);
    do {
        t[--at] = "0123456789abcdef"[v & 15];
        v >>= 4;
    } while (v);
    b.put(Str(t + at, sizeof t - at));
    return err_set("ValueError", b.str()) == R::Ok;
}

// The event set a call names, checked and normalised the way CPython does:
// C_RETURN and C_RAISE come only with CALL, and BRANCH is the two halves.
bool event_set_arg(Value v, u32 bits, Str what, u32 &set)
{
    i64 n = 0;
    if (!as_int_arg(v, n))
        return err_set("TypeError", "an integer is required") == R::Ok;
    if (n < 0 || n >= (i64(1) << MON_EVENTS))
        return bad_set(what, n);
    u32 s = u32(n);
    if ((s & C_RETURN_EVENTS) && (s & C_CALL_EVENTS) != C_CALL_EVENTS)
        return err_set("ValueError", "cannot set C_RETURN or C_RAISE events independently") ==
               R::Ok;
    s &= ~u32(C_RETURN_EVENTS);
    if (s & (1u << MON_BRANCH))
        s = (s & ~(1u << MON_BRANCH)) | (1u << MON_BRANCH_LEFT) | (1u << MON_BRANCH_RIGHT);
    if (s >= (1u << bits))
        return bad_set(what, i64(s));
    set = s;
    return true;
}

R m_set_events(const CallArgs &a, Value &out)
{
    i64 id  = 0;
    u32 set = 0;
    if (!args_only(a, "set_events", 2, 2) || !tool_arg(a, id) || !tool_in_use(id) ||
        !event_set_arg(a.args[1], MON_EVENTS, "event set", set))
        return R::Err;
    home->events[id] = set;
    out              = value_none();
    return R::Ok;
}

R m_get_local_events(const CallArgs &a, Value &out)
{
    i64 id = 0;
    if (!args_only(a, "get_local_events", 2, 2) || !tool_arg(a, id))
        return R::Err;
    if (!valid_tool(id))
        return R::Err;
    if (!is_code(a.args[1]))
        return err_set("TypeError", "code must be a code object");
    out = Value::of_int(i32(mon_local_events(code_of(a.args[1]), u32(id))));
    return R::Ok;
}

R m_set_local_events(const CallArgs &a, Value &out)
{
    i64 id  = 0;
    u32 set = 0;
    if (!args_only(a, "set_local_events", 3, 3) || !tool_arg(a, id))
        return R::Err;
    if (!is_code(a.args[1]))
        return err_set("TypeError", "code must be a code object");
    if (!tool_in_use(id) || !event_set_arg(a.args[2], MON_LOCAL_EVENTS, "local event set", set))
        return R::Err;
    if (!mon_set_local_events(code_of(a.args[1]), u32(id), set))
        return oom();
    if (set)
        home->anylocal = true;
    out = value_none();
    return R::Ok;
}

// Nothing is instrumented yet, so there is nothing to put back: what a
// restart undoes is a DISABLE, and no event has fired to be disabled.
R m_restart_events(const CallArgs &a, Value &out)
{
    if (!args_only(a, "restart_events", 0, 0))
        return R::Err;
    mon_restart();
    out = value_none();
    return R::Ok;
}

// Which tools are watching each event, by name: what CPython's _all_events
// answers, and what its own tests read the registry through.
R m_all_events(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_all_events", 0, 0))
        return R::Err;
    DictObj *d = dict_new();
    if (!d)
        return oom();
    Root rd{ obj_value(d) };
    for (u32 e = 0; e < MON_LOCAL_EVENTS; e++) {
        u32 tools = 0;
        for (u32 t = 0; t < MON_TOOLS; t++)
            if (home->events[t] & (1u << e))
                tools |= 1u << t;
        if (!tools)
            continue;
        Root k{ str_new(EVENT_NAMES[e]) };
        if (k.v.is_nil() ||
            dict_set(static_cast<DictObj *>(rd.v.obj()), k.v, Value::of_int(i32(tools))) != R::Ok)
            return R::Err;
    }
    out = rd.v;
    return R::Ok;
}

constexpr ModDef DEFS[] = {
    { "use_tool_id", m_use_tool_id },
    { "clear_tool_id", m_clear_tool_id },
    { "free_tool_id", m_free_tool_id },
    { "get_tool", m_get_tool },
    { "register_callback", m_register_callback },
    { "get_events", m_get_events },
    { "set_events", m_set_events },
    { "get_local_events", m_get_local_events },
    { "set_local_events", m_set_local_events },
    { "restart_events", m_restart_events },
    { "_all_events", m_all_events },
};

// The `events` namespace: one power of two per event, and NO_EVENTS.
Value events_namespace()
{
    DictObj *d = dict_new();
    if (!d)
        return oom(), Value();
    Root rd{ obj_value(d) };
    for (u32 e = 0; e < MON_EVENTS; e++)
        if (!mod_int(static_cast<DictObj *>(rd.v.obj()), EVENT_NAMES[e], i64(1) << e))
            return Value();
    if (!mod_int(static_cast<DictObj *>(rd.v.obj()), "NO_EVENTS", 0))
        return Value();
    return namespace_new(rd.v);
}

// DISABLE and MISSING are bare objects, as CPython's are: what matters is
// that a callback can return the one and nothing else is it.
Value marker()
{
    return inst_new(type_object());
}

} // namespace

u32 mon_tool_events(u32 tool)
{
    return home && tool < MON_TOOLS ? home->events[tool] : 0;
}

Value mon_callback(u32 tool, u32 event)
{
    return home && tool < MON_TOOLS && event < MON_EVENTS ? home->callbacks[tool][event] : Value();
}

bool mon_armed()
{
    if (!home)
        return false;
    if (home->anylocal)
        return true;
    for (u32 t = 0; t < MON_TOOLS; t++)
        if (home->events[t])
            return true;
    return false;
}

u32 mon_events_for(const CodeObj *c, u32 tool)
{
    if (!home || tool >= MON_TOOLS)
        return 0;
    return home->events[tool] | mon_local_events(c, tool);
}

Value mon_disable()
{
    return home ? home->disable : Value();
}

Value monitoring_new()
{
    if (!home) {
        home = heap_new<Home>();
        if (!home)
            return oom(), Value();
        gc_root_hook(home_mark);
    }
    Root mod{ module_new("sys.monitoring") };
    if (mod.v.is_nil())
        return Value();
    DictObj *d = module_dict(mod.v);
    if (home->disable.is_nil()) {
        Root dis{ marker() };
        Root mis{ marker() };
        if (dis.v.is_nil() || mis.v.is_nil())
            return Value();
        home->disable = dis.v;
        home->missing = mis.v;
    }
    Root evs{ events_namespace() };
    if (evs.v.is_nil() || !mod_defs(d, DEFS) || !mod_put(d, "events", evs.v) ||
        !mod_put(d, "DISABLE", home->disable) || !mod_put(d, "MISSING", home->missing))
        return Value();
    // PEP 669 hands these four out so that two tools do not collide.
    if (!mod_int(d, "DEBUGGER_ID", MON_DEBUGGER_ID) ||
        !mod_int(d, "COVERAGE_ID", MON_COVERAGE_ID) ||
        !mod_int(d, "PROFILER_ID", MON_PROFILER_ID) ||
        !mod_int(d, "OPTIMIZER_ID", MON_OPTIMIZER_ID))
        return Value();
    return mod.v;
}
