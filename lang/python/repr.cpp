// repr for every type, quoting and all.
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "kernel/text.h"
#include "ops.h"

namespace {

// What repr() is already inside, so a self-holding container prints `[...]`.
Vec<Obj *> *active;

bool enter(Value v)
{
    if (!active)
        active = heap_new<Vec<Obj *>>();
    if (!active)
        return false;
    for (usize i = 0; i < active->size(); i++)
        if ((*active)[i] == v.obj())
            return false;
    return active->push(v.obj());
}

void leave()
{
    if (active && active->size())
        active->pop();
}

bool put_hex(String &out, Str prefix, u32 value, usize digits)
{
    const char *HEX = "0123456789abcdef";
    if (!out.append(prefix))
        return false;
    for (usize i = digits; i-- > 0;)
        if (!out.push(HEX[(value >> (i * 4)) & 0xf]))
            return false;
    return true;
}

// The other quote only where the text holds this one, as CPython does.
char quote_for(Str s)
{
    bool single = s.find('\'') != Str::npos;
    bool dbl    = s.find('"') != Str::npos;
    return single && !dbl ? '"' : '\'';
}

R oom()
{
    return err_set("MemoryError", "out of memory");
}

R comma(String &out, bool &first)
{
    if (!first && !out.append(", "))
        return oom();
    first = false;
    return R::Ok;
}

R items_repr(String &out, Value v, const Value *items, usize n, Str open, Str close)
{
    if (!enter(v))
        return out.append(open) && out.append("...") && out.append(close) ? R::Ok : oom();
    if (!out.append(open))
        return leave(), oom();
    bool first = true;
    for (usize i = 0; i < n; i++) {
        if (comma(out, first) != R::Ok || py_repr(items[i], out) != R::Ok)
            return leave(), R::Err;
    }
    if (!out.append(close))
        return leave(), oom();
    leave();
    return R::Ok;
}

} // namespace

R none_repr(Value, String &out)
{
    return out.append("None") ? R::Ok : oom();
}

R bool_repr(Value v, String &out)
{
    return out.append(is_true(v) ? "True" : "False") ? R::Ok : oom();
}

R notimpl_repr(Value, String &out)
{
    return out.append("NotImplemented") ? R::Ok : err_set("MemoryError", "out of memory");
}

R ellipsis_repr(Value, String &out)
{
    return out.append("Ellipsis") ? R::Ok : oom();
}

R str_repr(Value v, String &out)
{
    StrObj *s = str_of(v);
    char q    = quote_for(s->str());
    if (!out.push(q))
        return oom();
    for (usize i = 0; i < s->len;) {
        char32_t cp = 0;
        usize w     = utf8_decode(s->str(), i, cp);
        if (w == 0)
            break;
        i += w;
        bool ok = true;
        if (cp == u32(q) || cp == '\\')
            ok = out.push('\\') && out.push(char(cp));
        else if (cp == '\n')
            ok = out.append("\\n");
        else if (cp == '\r')
            ok = out.append("\\r");
        else if (cp == '\t')
            ok = out.append("\\t");
        else if (cp < 0x20 || cp == 0x7f)
            ok = put_hex(out, "\\x", u32(cp), 2);
        else if (cp < 0x80)
            ok = out.push(char(cp));
        else if (cp < 0xa0)
            ok = put_hex(out, "\\x", u32(cp), 2);
        else {
            // No printability table: U+00A0 up is written as itself, where
            // CPython escapes the unprintable ones.
            char tmp[4];
            usize n = utf8_encode(cp, tmp);
            ok      = out.append(Str(tmp, n));
        }
        if (!ok)
            return oom();
    }
    return out.push(q) ? R::Ok : oom();
}

namespace {

R octets_repr(Str s, Str open, Str close, String &out)
{
    char q = quote_for(s);
    if (!out.append(open) || !out.push(q))
        return oom();
    for (usize i = 0; i < s.size(); i++) {
        u8 c    = u8(s[i]);
        bool ok = true;
        if (c == u8(q) || c == '\\')
            ok = out.push('\\') && out.push(char(c));
        else if (c == '\n')
            ok = out.append("\\n");
        else if (c == '\r')
            ok = out.append("\\r");
        else if (c == '\t')
            ok = out.append("\\t");
        else if (c < 0x20 || c >= 0x7f)
            ok = put_hex(out, "\\x", c, 2);
        else
            ok = out.push(char(c));
        if (!ok)
            return oom();
    }
    return out.push(q) && out.append(close) ? R::Ok : oom();
}

} // namespace

R bytes_repr(Value v, String &out)
{
    return octets_repr(static_cast<BytesObj *>(v.obj())->str(), "b", "", out);
}

R array_repr(Value v, String &out)
{
    return octets_repr(array_of(v)->str(), "bytearray(b", ")", out);
}

R tuple_repr(Value v, String &out)
{
    TupleObj *t = static_cast<TupleObj *>(v.obj());
    // A one-item tuple keeps the comma that makes it one.
    if (t->len == 1) {
        if (!enter(v))
            return out.append("(...)") ? R::Ok : oom();
        bool ok = out.push('(') && py_repr(t->items()[0], out) == R::Ok && out.append(",)");
        leave();
        return ok ? R::Ok : R::Err;
    }
    return items_repr(out, v, t->items(), t->len, "(", ")");
}

R list_repr(Value v, String &out)
{
    ListObj *l = list_of(v);
    return items_repr(out, v, l->items.data(), l->items.size(), "[", "]");
}

R dict_repr(Value v, String &out)
{
    DictObj *d = static_cast<DictObj *>(v.obj());
    if (!enter(v))
        return out.append("{...}") ? R::Ok : oom();
    if (!out.push('{'))
        return leave(), oom();
    usize at = 0;
    Value k, val;
    bool first = true;
    while (table_next(d->t, at, k, val)) {
        if (comma(out, first) != R::Ok || py_repr(k, out) != R::Ok || !out.append(": ") ||
            py_repr(val, out) != R::Ok)
            return leave(), R::Err;
    }
    leave();
    return out.push('}') ? R::Ok : oom();
}

R set_repr(Value v, String &out)
{
    SetObj *s = set_at(v);
    if (s->t.live == 0)
        return out.append("set()") ? R::Ok : oom();
    if (!enter(v))
        return out.append("{...}") ? R::Ok : oom();
    if (!out.push('{'))
        return leave(), oom();
    usize at = 0;
    Value k, val;
    bool first = true;
    while (table_next(s->t, at, k, val)) {
        if (comma(out, first) != R::Ok || py_repr(k, out) != R::Ok)
            return leave(), R::Err;
    }
    leave();
    return out.push('}') ? R::Ok : oom();
}

R frozenset_repr(Value v, String &out)
{
    SetObj *s = set_at(v);
    if (s->t.live == 0)
        return out.append("frozenset()") ? R::Ok : oom();
    if (!out.append("frozenset("))
        return oom();
    if (set_repr(v, out) != R::Ok)
        return R::Err;
    return out.push(')') ? R::Ok : oom();
}
