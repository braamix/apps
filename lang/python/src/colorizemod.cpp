// `_colorize`, without colour.
//
// CPython's _colorize.py is built on dataclasses, which wait for phase 27.
// argparse and traceback reach it lazily and only to ask whether to colour;
// the answer here is always no, and a theme is the stand-in traceback keeps
// for late shutdown: every section and every field of it is empty. ANSIColors
// and NoColors are that same object, since a code that is never written is
// the empty string either way; doctest names both.
#include "gc.h"
#include "method.h"
#include "module.h"
#include "posix.h"

namespace {

extern const Type nocolor_type;

R oom()
{
    return err_set("MemoryError", "out of memory");
}

R nocolor_repr(Value, String &out)
{
    return out.append("<_colorize.theme_no_color>") ? R::Ok : oom();
}

R nocolor_str(Value, String &)
{
    return R::Ok;
}

R nocolor_getattr(Value v, StrObj *name, Value &out)
{
    if (name->str().starts_with("__"))
        return R::NotImpl;
    out = v;
    return R::Ok;
}

R nocolor_getitem(Value, Value, Value &out)
{
    out = str_new("");
    return out.is_nil() ? R::Err : R::Ok;
}

// theme + text and text + theme are the text.
R nocolor_binop(Value a, Value b, Op op, Value &out)
{
    if (op != Op::Add)
        return R::NotImpl;
    out = a.is_obj() && a.obj()->type == &nocolor_type ? b : a;
    return R::Ok;
}

constexpr Type nocolor_type{ .name    = "_NoColor",
                             .repr    = nocolor_repr,
                             .str     = nocolor_str,
                             .getitem = nocolor_getitem,
                             .binop   = nocolor_binop,
                             .getattr = nocolor_getattr,
                             .final   = true };

// `ANSIColors()` is a class in CPython, so a program may call the name it
// found. Here it answers itself, which is the same empty theme.
R nocolor_call(const CallArgs &a, Value &out)
{
    out = method_self(a.args[0]);
    return out.is_nil() ? err_set("TypeError", "not a theme") : R::Ok;
}

constexpr Method NOCOLOR_METHODS[] = { { "__call__", nocolor_call } };

Value nocolor()
{
    Obj *o = obj_alloc(&nocolor_type, sizeof(Obj));
    return o ? obj_value(o) : (oom(), Value());
}

R m_can_colorize(const CallArgs &a, Value &out)
{
    if (a.nargs)
        return err_set("TypeError", "can_colorize() takes 0 positional arguments");
    for (u32 k = 0; k < a.nkw; k++)
        if (!is_str(a.kwnames[k]) || str_of(a.kwnames[k])->str() != "file")
            return err_set2("TypeError", "can_colorize() got an unexpected keyword argument",
                            is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str("?"));
    out = value_bool(false);
    return R::Ok;
}

R m_theme(const CallArgs &, Value &out)
{
    out = nocolor();
    return out.is_nil() ? R::Err : R::Ok;
}

R m_none(const CallArgs &, Value &out)
{
    out = value_none();
    return R::Ok;
}

// decolor(text): without its "\x1b[...m" sequences.
R m_decolor(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "text" };
    Value v[1];
    if (!fn_take(a, "decolor", NAMES, 1, v))
        return R::Err;
    if (!is_str(v[0]))
        return err_set2("TypeError", "decolor() argument must be str", type_name(v[0]));
    Str s = str_of(v[0])->str();
    String b;
    for (usize i = 0; i < s.size(); i++) {
        if (s[i] == '\x1b' && i + 1 < s.size() && s[i + 1] == '[') {
            usize j = i + 2;
            while (j < s.size() && ((s[j] >= '0' && s[j] <= '9') || s[j] == ';'))
                j++;
            if (j < s.size() && s[j] == 'm') {
                i = j;
                continue;
            }
        }
        if (!b.push(s[i]))
            return oom();
    }
    out = str_new(b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr ModDef DEFS[] = {
    { "can_colorize", m_can_colorize }, { "get_theme", m_theme },
    { "get_colors", m_theme },          { "set_theme", m_none },
    { "decolor", m_decolor },
};

} // namespace

bool colorize_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!method_install(&nocolor_type, NOCOLOR_METHODS) || !mod_defs(d, DEFS) ||
        !mod_put(d, "COLORIZE", value_bool(false)))
        return false;
    Root t{ nocolor() };
    d = static_cast<DictObj *>(rd.v.obj());
    return !t.v.is_nil() && mod_put(d, "theme_no_color", t.v) && mod_put(d, "default_theme", t.v) &&
           mod_put(d, "ANSIColors", t.v) && mod_put(d, "NoColors", t.v);
}
