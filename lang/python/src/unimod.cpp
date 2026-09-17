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

// What 3.2.0 says about `cp`, where the caller is ucd_3_2_0: whether it was
// unassigned then, and its record of differences, if any.
struct Old {
    bool unassigned   = false;
    const UcdOld *rec = nullptr;
};

Old old_of(u32 cp, bool old)
{
    Old o;
    if (old) {
        o.unassigned = ucd_old_unassigned(cp);
        o.rec        = o.unassigned ? nullptr : ucd_old(cp);
    }
    return o;
}

R u_category(const CallArgs &a, bool old, Value &out)
{
    u32 cp = 0;
    if (!count(a, "category", 1, 1) || !one_char(a, "category", cp))
        return R::Err;
    Old o = old_of(cp, old);
    if (o.unassigned)
        return put_str("Cn", out);
    if (o.rec && o.rec->cat != UCD_OLD_SAME)
        return put_str(ucd_category_name(o.rec->cat), out);
    return put_str(ucd_category(cp), out);
}

R u_bidirectional(const CallArgs &a, bool old, Value &out)
{
    u32 cp = 0;
    if (!count(a, "bidirectional", 1, 1) || !one_char(a, "bidirectional", cp))
        return R::Err;
    Old o = old_of(cp, old);
    if (o.unassigned)
        return put_str("", out);
    if (o.rec && o.rec->bidi != UCD_OLD_SAME)
        return put_str(ucd_bidi_name(o.rec->bidi), out);
    return put_str(ucd_bidirectional(cp), out);
}

R u_east_asian_width(const CallArgs &a, bool old, Value &out)
{
    u32 cp = 0;
    if (!count(a, "east_asian_width", 1, 1) || !one_char(a, "east_asian_width", cp))
        return R::Err;
    Old o = old_of(cp, old);
    if (o.unassigned)
        return put_str(ucd_width_name(0), out);
    if (o.rec && o.rec->width != UCD_OLD_SAME)
        return put_str(ucd_width_name(o.rec->width), out);
    return put_str(ucd_east_asian_width(cp), out);
}

R u_combining(const CallArgs &a, bool old, Value &out)
{
    u32 cp = 0;
    if (!count(a, "combining", 1, 1) || !one_char(a, "combining", cp))
        return R::Err;
    out = Value::of_int(old_of(cp, old).unassigned ? 0 : ucd_combining(cp));
    return R::Ok;
}

R u_mirrored(const CallArgs &a, bool old, Value &out)
{
    u32 cp = 0;
    if (!count(a, "mirrored", 1, 1) || !one_char(a, "mirrored", cp))
        return R::Err;
    Old o   = old_of(cp, old);
    int yes = ucd_is(cp, UCD_MIRRORED) ? 1 : 0;
    if (o.unassigned)
        yes = 0;
    else if (o.rec && o.rec->mirrored != UCD_OLD_SAME)
        yes = o.rec->mirrored;
    out = Value::of_int(yes);
    return R::Ok;
}

R u_decomposition(const CallArgs &a, bool old, Value &out)
{
    u32 cp = 0;
    if (!count(a, "decomposition", 1, 1) || !one_char(a, "decomposition", cp))
        return R::Err;
    String s;
    if (!old_of(cp, old).unassigned && !ucd_decomposition(cp, s))
        return oom();
    return put_str(s.str(), out);
}

// decimal, digit and numeric: the value, the default, or ValueError. 3.2.0's
// digit is this version's, as CPython has it.
R number(const CallArgs &a, bool old, Str who, int which, Str missing, Value &out)
{
    u32 cp = 0;
    if (!count(a, who, 1, 2) || !one_char(a, who, cp, true))
        return R::Err;
    Old o   = old_of(cp, old);
    f64 v   = 0;
    bool ok = false;
    if (which < 2) {
        int d = which == 0 ? ucd_decimal(cp) : ucd_digit(cp);
        if (which == 0 && o.unassigned)
            d = -1;
        else if (which == 0 && o.rec && o.rec->decimal != UCD_OLD_SAME)
            d = o.rec->decimal == UCD_OLD_NONE ? -1 : o.rec->decimal;
        if (d >= 0) {
            out = Value::of_int(d);
            return R::Ok;
        }
    } else if (o.unassigned) {
        ok = false;
    } else if (o.rec && o.rec->numeric != UCD_OLD_SAME) {
        ok = o.rec->numeric != UCD_OLD_NONE;
        if (ok)
            v = ucd_old_number(o.rec->numeric);
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

R u_decimal(const CallArgs &a, bool old, Value &out)
{
    return number(a, old, "decimal", 0, "not a decimal", out);
}

R u_digit(const CallArgs &a, bool old, Value &out)
{
    return number(a, old, "digit", 1, "not a digit", out);
}

R u_numeric(const CallArgs &a, bool old, Value &out)
{
    return number(a, old, "numeric", 2, "not a numeric character", out);
}

R u_name(const CallArgs &a, bool old, Value &out)
{
    u32 cp = 0;
    if (!count(a, "name", 1, 2) || !one_char(a, "name", cp, true))
        return R::Err;
    String s;
    if (!old_of(cp, old).unassigned && ucd_name(cp, s))
        return put_str(s.str(), out);
    if (a.nargs == 2) {
        out = a.args[1];
        return R::Ok;
    }
    return err_set("ValueError", "no such name");
}

// A name to its character. 3.2.0 had no aliases and no named sequences, so
// there the name has to be the character's own.
bool plain_name(Str given, u32 cp)
{
    String s;
    if (!ucd_name(cp, s) || s.size() != given.size())
        return false;
    for (usize i = 0; i < given.size(); i++) {
        char c = given[i];
        if (c >= 'a' && c <= 'z')
            c = char(c - 32);
        if (c != s[i])
            return false;
    }
    return true;
}

R u_lookup(const CallArgs &a, bool old, Value &out)
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
    if (!ucd_lookup(name, true, got) || (old && (got.size() != 1 || !plain_name(name, got[0])))) {
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
R normalized(const CallArgs &a, bool old, Str who, String &text, bool &same)
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
    if (!codepoints(s->str(), cps) || !ucd_normalize(form, cps, old))
        return oom();
    for (usize i = 0; i < cps.size(); i++)
        if (!cp_append(text, cps[i]))
            return oom();
    same = text.str() == s->str();
    return R::Ok;
}

R u_normalize(const CallArgs &a, bool old, Value &out)
{
    String text;
    bool same = true;
    if (normalized(a, old, "normalize", text, same) != R::Ok)
        return R::Err;
    if (same) {
        out = a.args[1];
        return R::Ok;
    }
    return put_str(text.str(), out);
}

R u_is_normalized(const CallArgs &a, bool old, Value &out)
{
    String text;
    bool same = true;
    if (normalized(a, old, "is_normalized", text, same) != R::Ok)
        return R::Err;
    out = value_bool(same);
    return R::Ok;
}

// Each function twice: the module's, and a method of ucd_3_2_0, whose self
// is dropped once it has been checked.
extern const Type ucd_type;

template <R (*F)(const CallArgs &, bool, Value &)>
R as_function(const CallArgs &a, Value &out)
{
    return F(a, false, out);
}

template <R (*F)(const CallArgs &, bool, Value &)>
R as_method(const CallArgs &a, Value &out)
{
    if (!a.nargs || !a.args[0].is_obj() || a.args[0].obj()->type != &ucd_type)
        return err_set("TypeError", "a unicodedata.UCD method needs a UCD object");
    CallArgs rest = a;
    rest.args++;
    rest.nargs--;
    return F(rest, true, out);
}

constexpr ModDef PAIRS[] = {
    { "lookup", as_function<u_lookup> },
    { "lookup", as_method<u_lookup> },
    { "name", as_function<u_name> },
    { "name", as_method<u_name> },
    { "decimal", as_function<u_decimal> },
    { "decimal", as_method<u_decimal> },
    { "digit", as_function<u_digit> },
    { "digit", as_method<u_digit> },
    { "numeric", as_function<u_numeric> },
    { "numeric", as_method<u_numeric> },
    { "category", as_function<u_category> },
    { "category", as_method<u_category> },
    { "bidirectional", as_function<u_bidirectional> },
    { "bidirectional", as_method<u_bidirectional> },
    { "combining", as_function<u_combining> },
    { "combining", as_method<u_combining> },
    { "east_asian_width", as_function<u_east_asian_width> },
    { "east_asian_width", as_method<u_east_asian_width> },
    { "mirrored", as_function<u_mirrored> },
    { "mirrored", as_method<u_mirrored> },
    { "decomposition", as_function<u_decomposition> },
    { "decomposition", as_method<u_decomposition> },
    { "normalize", as_function<u_normalize> },
    { "normalize", as_method<u_normalize> },
    { "is_normalized", as_function<u_is_normalized> },
    { "is_normalized", as_method<u_is_normalized> },
};

constexpr usize NPAIRS = sizeof PAIRS / sizeof PAIRS[0] / 2;

R ucd_getattr(Value, StrObj *name, Value &out)
{
    if (name->str() != Str("unidata_version"))
        return R::NotImpl;
    return put_str("3.2.0", out);
}

constexpr Type ucd_type{ .name = "unicodedata.UCD", .getattr = ucd_getattr, .final = true };

// The one instance, which never goes away.
Obj ucd_320{ &ucd_type, nullptr, nullptr, OBJ_IMMORTAL };

} // namespace

bool unicodedata_install(DictObj *into)
{
    Method methods[NPAIRS];
    for (usize i = 0; i < NPAIRS; i++) {
        if (!mod_defs(into, &PAIRS[i * 2], 1))
            return false;
        methods[i] = Method{ PAIRS[i * 2 + 1].name, PAIRS[i * 2 + 1].fn };
    }
    if (!method_install(&ucd_type, methods, NPAIRS))
        return false;
    return mod_str(into, "unidata_version", UCD_VERSION) && mod_type(into, &ucd_type) &&
           mod_put(into, "ucd_3_2_0", Value::of_obj(&ucd_320));
}
