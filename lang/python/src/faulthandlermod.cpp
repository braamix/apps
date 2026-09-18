// `faulthandler`: the Python stack, written straight to a descriptor.
//
// A fault here is a wasm trap, and a trap ends the Worker before anything can
// run, so enable() records the request and nothing more. There is no thread to
// dump a traceback later from, and no signal to register a handler for:
// dump_traceback_later raises, and register and unregister are absent, as on
// Windows.
#include "bigint.h"
#include "builtin.h"
#include "call.h"
#include "code.h"
#include "frame.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "module.h"
#include "ops.h"
#include "posix.h"
#include "proc/rt.h"
#include "type.h"
#include "vm.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

bool enabled;

// "  File "x.py", line 3 in f", innermost first, as _Py_DumpTraceback does.
bool dump_frames(String &out)
{
    ListObj *l = vm_frames();
    if (!l)
        return false;
    if (l->items.empty())
        return out.append("  <no Python frame>\n");
    char t[24];
    for (usize i = 0; i < l->items.size(); i++) {
        FrameObj *f = frame_of(l->items[i]);
        CodeObj *c  = code_of(f->code);
        Str file    = is_str(c->filename) ? str_of(c->filename)->str() : Str("???");
        Str name    = is_str(c->name) ? str_of(c->name)->str() : Str("???");
        u32 line    = code_line(c, f->pc ? f->pc - 1 : 0);
        if (!out.append("  File \"") || !out.append(file) || !out.append("\", line ") ||
            !out.append(int_text(t, sizeof t, i64(line))) || !out.append(" in ") ||
            !out.append(name) || !out.push('\n'))
            return false;
    }
    return true;
}

// The one thread, under the header CPython gives the current one.
bool dump_all(String &out)
{
    Buf<64> b;
    b.put("Current thread 0x");
    u64 id = u64(proc_pid());
    for (int shift = 60; shift >= 0; shift -= 4)
        b.put("0123456789abcdef"[(id >> shift) & 15]);
    b.put(" (most recent call first):\n");
    return out.append(b.str()) && dump_frames(out);
}

// s[0] the file argument, s[1] the text to write (Nil to write nothing),
// s[2] what the call answers. i is the step; j is set for enable().
R write_step(ContObj *k, Value in)
{
    switch (k->i) {
    case 0: {
        // An int is the descriptor itself; anything else is asked for one.
        k->i = 2;
        if (is_intval(k->s[0])) {
            in = k->s[0];
            break;
        }
        StrObj *n = str_intern("fileno");
        if (!n)
            return oom();
        Value fn;
        Got g = py_attr(k->s[0], n, fn);
        if (g == Got::Error)
            return R::Err;
        if (g == Got::Missing) {
            Buf<128> b;
            b.put("'").put(type_name(k->s[0])).put("' object has no attribute 'fileno'");
            return err_set("AttributeError", b.str());
        }
        if (g == Got::Call) {
            k->i = 1;
            return cont_await(k, fn);
        }
        return cont_call(k, fn, Value(), 0);
    }
    case 1:
        k->i = 2;
        return cont_call(k, in, Value(), 0);
    case 2:
        break;
    default:
        return cont_done(k, k->s[2]);
    }
    i64 fd = -1;
    if (!is_intval(in) || !int_to_i64(in, fd) || fd < 0)
        return err_set("ValueError", "file is not a valid file descriptor");
    k->i = 3;
    if (k->j)
        enabled = true;
    if (k->s[1].is_nil())
        return cont_done(k, k->s[2]);
    Str text = str_of(k->s[1])->str();
    // The two the VM buffers itself, so they keep their order with print's.
    String *sink = fd == 1 ? vm_out() : fd == 2 ? vm_errout() : nullptr;
    if (sink)
        return sink->append(text) ? cont_done(k, k->s[2]) : oom();
    Value w;
    StrObj *n = str_intern("write");
    Root posix{ builtin_module("posix") };
    if (!n || posix.v.is_nil() || dict_get(module_dict(posix.v), obj_value(n), w) != R::Ok)
        return err_pending() ? R::Err : oom();
    Root rw{ w };
    Value data = bytes_new(text);
    if (data.is_nil())
        return R::Err;
    return cont_call(k, rw.v, in, 2, data);
}

// Where the text goes: `file`, or sys.stderr when that is None or missing.
R write_to(Value file, Str text, bool any, Value answer, Value &out)
{
    Root rf{ file.is_nil() || is_none(file) ? sys_stream("stderr") : file };
    if (rf.v.is_nil() || is_none(rf.v))
        return err_set("RuntimeError", "sys.stderr is None");
    Root rt{ any ? str_new(text) : Value() }, ra{ answer };
    if (any && rt.v.is_nil())
        return R::Err;
    Value kv = cont_new(write_step);
    if (kv.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv);
    k->s[0]    = rf.v;
    k->s[1]    = rt.v;
    k->s[2]    = ra.v;
    out        = kv;
    return R::Ok;
}

// all_threads, which is True unless given and false.
bool all_threads(Value v)
{
    return v.is_nil() || py_truth(v);
}

R b_dump_traceback(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "file", "all_threads" };
    Value v[2];
    if (!fn_take(a, "dump_traceback", NAMES, 0, v))
        return R::Err;
    String text;
    if (!(all_threads(v[1])
              ? dump_all(text)
              : text.append("Stack (most recent call first):\n") && dump_frames(text)))
        return err_pending() ? R::Err : oom();
    return write_to(v[0], text.str(), true, value_none(), out);
}

R b_dump_c_stack(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "file" };
    Value v[1];
    if (!fn_take(a, "dump_c_stack", NAMES, 0, v))
        return R::Err;
    return write_to(v[0],
                    "Current thread's C stack trace (most recent call first):\n"
                    "  <cannot get C stack on this system>\n",
                    true, value_none(), out);
}

R b_enable(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "file", "all_threads" };
    Value v[2];
    if (!fn_take(a, "enable", NAMES, 0, v))
        return R::Err;
    // The file is checked as CPython checks it, and the request is only
    // remembered: j says to, once the descriptor is known to be good.
    if (write_to(v[0], Str(), false, value_none(), out) != R::Ok)
        return R::Err;
    cont_of(out)->j = 1;
    return R::Ok;
}

R b_disable(const CallArgs &a, Value &out)
{
    if (!args_only(a, "disable", 0, 0))
        return R::Err;
    out     = value_bool(enabled);
    enabled = false;
    return R::Ok;
}

R b_is_enabled(const CallArgs &a, Value &out)
{
    if (!args_only(a, "is_enabled", 0, 0))
        return R::Err;
    out = value_bool(enabled);
    return R::Ok;
}

R b_dump_traceback_later(const CallArgs &, Value &)
{
    return err_set("NotImplementedError",
                   "dump_traceback_later needs a second thread, and there is one");
}

R b_cancel_dump_traceback_later(const CallArgs &a, Value &out)
{
    if (!args_only(a, "cancel_dump_traceback_later", 0, 0))
        return R::Err;
    out = value_none();
    return R::Ok;
}

constexpr ModDef DEFS[] = {
    { "enable", b_enable },
    { "disable", b_disable },
    { "is_enabled", b_is_enabled },
    { "dump_traceback", b_dump_traceback },
    { "dump_c_stack", b_dump_c_stack },
    { "dump_traceback_later", b_dump_traceback_later },
    { "cancel_dump_traceback_later", b_cancel_dump_traceback_later },
};

} // namespace

bool faulthandler_install(DictObj *into)
{
    return mod_defs(into, DEFS);
}
