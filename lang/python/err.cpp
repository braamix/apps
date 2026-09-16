#include "err.h"

#include "gc.h"
#include "kernel/alloc.h"

namespace {

// String has a destructor, so this lives in a heap block, not at file scope.
struct Pending {
    String kind;
    String message;
};

Pending *pending;
bool live;
u32 at_line;
u32 at_col;
Value object; // the exception a raise named, or Nil

Pending *slot()
{
    if (!pending)
        pending = heap_new<Pending>();
    return pending;
}

} // namespace

R err_set(Str kind, Str message)
{
    Pending *p = slot();
    if (p) {
        p->kind.assign(kind);
        p->message.assign(message);
    }
    live    = true;
    at_line = 0;
    at_col  = 0;
    object  = Value();
    return R::Err;
}

R err_set_value(Value v, Str kind)
{
    if (!kind.empty()) {
        Pending *p = slot();
        if (p) {
            p->kind.assign(kind);
            p->message.clear();
        }
    }
    live    = true;
    at_line = 0;
    at_col  = 0;
    object  = v;
    return R::Err;
}

Value err_value()
{
    return live ? object : Value();
}

void err_mark()
{
    if (live)
        gc_mark(object);
}

R err_set_at(Str kind, Str message, u32 line, u32 col)
{
    err_set(kind, message);
    at_line = line;
    at_col  = col;
    return R::Err;
}

R err_set2(Str kind, Str message, Str detail)
{
    err_set(kind, message);
    if (pending) {
        pending->message.append(": ");
        pending->message.append(detail);
    }
    return R::Err;
}

bool err_pending()
{
    return live;
}

u32 err_line()
{
    return live ? at_line : 0;
}

u32 err_col()
{
    return live ? at_col : 0;
}

Str err_kind()
{
    return live && pending ? pending->kind.str() : Str();
}

Str err_message()
{
    return live && pending ? pending->message.str() : Str();
}

void err_clear()
{
    live   = false;
    object = Value();
}

void err_format(String &out)
{
    out.append(err_kind());
    out.append(": ");
    out.append(err_message());
}
