// `unicodedata`, over ucd.cpp's tables.
#include "exc.h"
#include "gc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "ucd.h"
#include "ustr.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// The one character a function is about; false with a TypeError pending.
// `numbered` for a function of more than one parameter, whose message says
// which argument it means.
bool one_char(const CallArgs &a, Str who, u32 &cp, bool numbered = false)
{
    Value v   = a.args[0];
    Str which = numbered ? Str(" argument 1") : Str(" argument");
    if (!is_str(v)) {
        Buf<96> b;
        b.put(who).put("()").put(which).put(" must be a unicode character, not ");
        return err_set("TypeError", b.put(type_name(v)).str()), false;
    }
    StrObj *s = str_of(v);
    if (s->chars != 1) {
        Buf<128> b;
        b.put(who).put("():").put(which);
        b.put(" must be a unicode character, not a string of length ");
        return err_set("TypeError", b.put(usize(s->chars)).str()), false;
    }
    cp = str_char_at(s, 0);
    return true;
}

bool count(const CallArgs &a, Str who, u32 least, u32 most)
{
    if (a.nkw) {
        Buf<96> b;
        b.put(who).put("() takes no keyword arguments");
        return err_set("TypeError", b.str()), false;
    }
    if (a.nargs >= least && a.nargs <= most)
        return true;
    Buf<96> b;
    b.put(who);
    if (least == most) {
        b.put("() takes exactly ").put(least).put(least == 1 ? " argument (" : " arguments (");
        b.put(a.nargs).put(" given)");
    } else if (a.nargs < least) {
        b.put(" expected at least ")
            .put(least)
            .put(least == 1 ? " argument, got " : " arguments, got ");
        b.put(a.nargs);
    } else {
        b.put(" expected at most ")
            .put(most)
            .put(most == 1 ? " argument, got " : " arguments, got ");
        b.put(a.nargs);
    }
    return err_set("TypeError", b.str()), false;
}

R key_error_text(Str text)
{
    Root s{ str_new(text) };
    return s.v.is_nil() ? R::Err : key_error(s.v);
}

R put_str(Str s, Value &out)
{
    out = str_new(s);
    return out.is_nil() ? R::Err : R::Ok;
}

R u_category(const CallArgs &a, Value &out)
{
    u32 cp = 0;
    if (!count(a, "category", 1, 1) || !one_char(a, "category", cp))
        return R::Err;
    return put_str(ucd_category(cp), out);
}

R u_bidirectional(const CallArgs &a, Value &out)
{
    u32 cp = 0;
    if (!count(a, "bidirectional", 1, 1) || !one_char(a, "bidirectional", cp))
        return R::Err;
    return put_str(ucd_bidirectional(cp), out);
}

R u_east_asian_width(const CallArgs &a, Value &out)
{
    u32 cp = 0;
    if (!count(a, "east_asian_width", 1, 1) || !one_char(a, "east_asian_width", cp))
        return R::Err;
    return put_str(ucd_east_asian_width(cp), out);
}

R u_combining(const CallArgs &a, Value &out)
{
    u32 cp = 0;
    if (!count(a, "combining", 1, 1) || !one_char(a, "combining", cp))
        return R::Err;
    out = Value::of_int(ucd_combining(cp));
    return R::Ok;
}

R u_mirrored(const CallArgs &a, Value &out)
{
    u32 cp = 0;
    if (!count(a, "mirrored", 1, 1) || !one_char(a, "mirrored", cp))
        return R::Err;
    out = Value::of_int(ucd_is(cp, UCD_MIRRORED) ? 1 : 0);
    return R::Ok;
}

R u_decomposition(const CallArgs &a, Value &out)
{
    u32 cp = 0;
    if (!count(a, "decomposition", 1, 1) || !one_char(a, "decomposition", cp))
        return R::Err;
    String s;
    if (!ucd_decomposition(cp, s))
        return oom();
    return put_str(s.str(), out);
}

// decimal, digit and numeric: the value, the default, or ValueError.
R number(const CallArgs &a, Str who, int which, Str missing, Value &out)
{
    u32 cp = 0;
    if (!count(a, who, 1, 2) || !one_char(a, who, cp, true))
        return R::Err;
    f64 v   = 0;
    bool ok = false;
    if (which < 2) {
        int d = which == 0 ? ucd_decimal(cp) : ucd_digit(cp);
        if (d >= 0) {
            out = Value::of_int(d);
            return R::Ok;
        }
    } else {
        ok = ucd_numeric(cp, v);
    }
    if (ok) {
        out = float_new(v);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (a.nargs == 2) {
        out = a.args[1];
        return R::Ok;
    }
    return err_set("ValueError", missing);
}

R u_decimal(const CallArgs &a, Value &out)
{
    return number(a, "decimal", 0, "not a decimal", out);
}

R u_digit(const CallArgs &a, Value &out)
{
    return number(a, "digit", 1, "not a digit", out);
}

R u_numeric(const CallArgs &a, Value &out)
{
    return number(a, "numeric", 2, "not a numeric character", out);
}

R u_name(const CallArgs &a, Value &out)
{
    u32 cp = 0;
    if (!count(a, "name", 1, 2) || !one_char(a, "name", cp, true))
        return R::Err;
    String s;
    if (ucd_name(cp, s))
        return put_str(s.str(), out);
    if (a.nargs == 2) {
        out = a.args[1];
        return R::Ok;
    }
    return err_set("ValueError", "no such name");
}

R u_lookup(const CallArgs &a, Value &out)
{
    if (!count(a, "lookup", 1, 1))
        return R::Err;
    Str name;
    if (is_str(a.args[0]))
        name = str_of(a.args[0])->str();
    else if (!bytes_like(a.args[0], name))
        return err_not("a bytes-like object is required", a.args[0], true);
    if (name.size() > ucd_longest_name())
        return key_error_text("name too long");
    Vec<u32> got;
    if (!ucd_lookup(name, true, got)) {
        Buf<256> b;
        b.put("undefined character name '").put(name).put("'");
        return key_error_text(b.str());
    }
    String s;
    for (usize i = 0; i < got.size(); i++)
        if (!cp_append(s, got[i]))
            return oom();
    return put_str(s.str(), out);
}

bool form_of(Value v, Str who, UcdForm &out)
{
    if (!is_str(v)) {
        Buf<96> b;
        b.put(who).put("() argument 1 must be str");
        return err_not(b.str(), v), false;
    }
    Str f = str_of(v)->str();
    if (f == "NFC")
        out = UcdForm::NFC;
    else if (f == "NFD")
        out = UcdForm::NFD;
    else if (f == "NFKC")
        out = UcdForm::NFKC;
    else if (f == "NFKD")
        out = UcdForm::NFKD;
    else
        return err_set("ValueError", "invalid normalization form"), false;
    return true;
}

bool codepoints(Str s, Vec<u32> &out)
{
    for (usize at = 0; at < s.size();) {
        u32 cp = 0;
        at += cp_decode(s, at, cp);
        if (!out.push(cp))
            return false;
    }
    return true;
}

// The normalized text, and whether it is the same as the input.
R normalized(const CallArgs &a, Str who, String &text, bool &same)
{
    if (!count(a, who, 2, 2))
        return R::Err;
    UcdForm form = UcdForm::NFC;
    if (!form_of(a.args[0], who, form))
        return R::Err;
    if (!is_str(a.args[1])) {
        Buf<96> b;
        b.put(who).put("() argument 2 must be str");
        return err_not(b.str(), a.args[1]);
    }
    StrObj *s = str_of(a.args[1]);
    same      = true;
    // ASCII is in every form already.
    if (s->flags & OBJ_ASCII)
        return R::Ok;
    Vec<u32> cps;
    if (!codepoints(s->str(), cps) || !ucd_normalize(form, cps))
        return oom();
    for (usize i = 0; i < cps.size(); i++)
        if (!cp_append(text, cps[i]))
            return oom();
    same = text.str() == s->str();
    return R::Ok;
}

R u_normalize(const CallArgs &a, Value &out)
{
    String text;
    bool same = true;
    if (normalized(a, "normalize", text, same) != R::Ok)
        return R::Err;
    if (same) {
        out = a.args[1];
        return R::Ok;
    }
    return put_str(text.str(), out);
}

R u_is_normalized(const CallArgs &a, Value &out)
{
    String text;
    bool same = true;
    if (normalized(a, "is_normalized", text, same) != R::Ok)
        return R::Err;
    out = value_bool(same);
    return R::Ok;
}

constexpr ModDef DEFS[] = {
    { "lookup", u_lookup },
    { "name", u_name },
    { "decimal", u_decimal },
    { "digit", u_digit },
    { "numeric", u_numeric },
    { "category", u_category },
    { "bidirectional", u_bidirectional },
    { "combining", u_combining },
    { "east_asian_width", u_east_asian_width },
    { "mirrored", u_mirrored },
    { "decomposition", u_decomposition },
    { "normalize", u_normalize },
    { "is_normalized", u_is_normalized },
};

} // namespace

bool unicodedata_install(DictObj *into)
{
    return mod_defs(into, DEFS) && mod_str(into, "unidata_version", UCD_VERSION);
}
