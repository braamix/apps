#include "err.h"

#include "kernel/alloc.h"

namespace {

// String has a destructor, so this lives in a heap block, not at file scope.
struct Pending {
    String kind;
    String message;
};

Pending *pending;
bool live;

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
    live = true;
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
    live = false;
}

void err_format(String &out)
{
    out.append(err_kind());
    out.append(": ");
    out.append(err_message());
}
