// The traceback object: one frame an exception passed through, and the next.
#include "exc.h"
#include "frame.h"
#include "gc.h"
#include "kernel/fmt.h"
#include "posix.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

void tb_trace(Obj *o)
{
    TracebackObj *t = static_cast<TracebackObj *>(o);
    gc_mark(t->next);
    gc_mark(t->frame);
}

R tb_repr(Value v, String &out)
{
    char tmp[24];
    Buf<64> b;
    b.put("<traceback object at ").put(addr_text(tmp, sizeof tmp, v.obj())).put('>');
    return out.append(b.str()) ? R::Ok : oom();
}

R tb_getattr(Value v, StrObj *name, Value &out)
{
    TracebackObj *t = tb_of(v);
    Str n           = name->str();
    if (n == "tb_next")
        out = t->next.is_nil() ? value_none() : t->next;
    else if (n == "tb_frame")
        out = t->frame;
    else if (n == "tb_lasti")
        out = Value::of_int(t->lasti);
    else if (n == "tb_lineno")
        out = t->lineno < 0 ? value_none() : Value::of_int(t->lineno);
    else
        return R::NotImpl;
    return R::Ok;
}

// Only tb_next is writable, and not into a loop.
R tb_setattr(Value v, StrObj *name, Value x)
{
    Str n = name->str();
    if (n != "tb_next") {
        if (n == "tb_frame" || n == "tb_lasti" || n == "tb_lineno") {
            Buf<96> b;
            b.put("attribute '").put(n).put("' of 'traceback' objects is not writable");
            return err_set("AttributeError", b.str());
        }
        return R::NotImpl;
    }
    if (x.is_nil())
        return err_set("TypeError", "can't delete tb_next attribute");
    if (!is_none(x) && !is_traceback(x)) {
        Buf<96> b;
        b.put("expected traceback object or None, got '").put(type_name(x)).put('\'');
        return err_set("TypeError", b.str());
    }
    for (Value c = x; is_traceback(c); c = tb_of(c)->next)
        if (c.w == v.w)
            return err_set("ValueError", "traceback loop detected");
    tb_of(v)->next = is_none(x) ? Value() : x;
    return R::Ok;
}

// TracebackType(tb_next, tb_frame, tb_lasti, tb_lineno)
R tb_ctor(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "tb_next", "tb_frame", "tb_lasti", "tb_lineno" };
    Value v[4];
    if (!fn_take(a, "traceback", NAMES, 4, v))
        return R::Err;
    if (!is_none(v[0]) && !is_traceback(v[0])) {
        Buf<96> b;
        b.put("expected traceback object or None, got '").put(type_name(v[0])).put('\'');
        return err_set("TypeError", b.str());
    }
    if (!is_frame(v[1])) {
        Buf<96> b;
        b.put("TracebackType() argument 'tb_frame' must be frame, not ").put(type_name(v[1]));
        return err_set("TypeError", b.str());
    }
    i64 lasti = 0, lineno = 0;
    if (!as_index(v[2], lasti) || !as_index(v[3], lineno))
        return err_set("TypeError", "'str' object cannot be interpreted as an integer");
    out = tb_new(is_none(v[0]) ? Value() : v[0], v[1], i32(lasti), i32(lineno));
    return out.is_nil() ? R::Err : R::Ok;
}

} // namespace

constexpr Type traceback_type{ .name    = "traceback",
                               .trace   = tb_trace,
                               .repr    = tb_repr,
                               .getattr = tb_getattr,
                               .setattr = tb_setattr,
                               .final   = true };

Value tb_new(Value next, Value frame, i32 lasti, i32 lineno)
{
    Root rn{ next }, rf{ frame };
    TracebackObj *t = static_cast<TracebackObj *>(obj_alloc(&traceback_type, sizeof(TracebackObj)));
    if (!t)
        return oom(), Value();
    t->next   = rn.v;
    t->frame  = rf.v;
    t->lasti  = lasti;
    t->lineno = lineno;
    return obj_value(t);
}

// BaseException.with_traceback(tb): sets __traceback__ and answers self.
R exc_with_traceback(const CallArgs &a, Value &out)
{
    if (a.nargs != 2 || a.nkw || !is_exc(a.args[0]))
        return err_set("TypeError", "with_traceback() takes exactly one argument");
    Value tb = a.args[1];
    if (!is_none(tb) && !is_traceback(tb))
        return err_set("TypeError", "__traceback__ must be a traceback or None");
    static_cast<ExcObj *>(a.args[0].obj())->tb = is_none(tb) ? Value() : tb;
    out                                        = a.args[0];
    return R::Ok;
}

R traceback_ctor(const CallArgs &a, Value &out)
{
    return tb_ctor(a, out);
}
