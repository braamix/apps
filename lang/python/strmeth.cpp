// str's methods. Everything is counted and indexed in codepoints, as the
// language promises. The bytes are UTF-8, and they are converted only at the
// boundary: an argument coming in, an index going out.
#include "call.h"
#include "codec.h"
#include "format.h"
#include "gc.h"
#include "gen.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"
#include "ucd.h"
#include "ustr.h"

namespace {

// ------------------------------------------------------------------ helpers

// The character index of byte offset `at`.
usize chars_before(const StrObj *s, usize at)
{
    if (s->flags & OBJ_ASCII)
        return at;
    usize n = 0;
    for (usize i = 0; i < at && i < s->len; n++) {
        u8 c = u8(s->bytes()[i]);
        i += c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : 4;
    }
    return n;
}

// A start/end pair as Python clamps it, in characters. Either may be None.
bool span_of(Value lo, Value hi, usize len, usize &from, usize &to)
{
    i64 n = i64(len), i = 0, j = n;
    if (!lo.is_nil() && !is_none(lo) && !as_index(lo, i))
        return err_set("TypeError", "slice indices must be integers or None"), false;
    if (!hi.is_nil() && !is_none(hi) && !as_index(hi, j))
        return err_set("TypeError", "slice indices must be integers or None"), false;
    if (i < 0)
        i += n;
    if (j < 0)
        j += n;
    i    = i < 0 ? 0 : i > n ? n : i;
    j    = j < 0 ? 0 : j > n ? n : j;
    from = usize(i);
    to   = usize(j > i ? j : i);
    return true;
}

// The bytes of the characters [from, to) of `s`.
Str slice_bytes(const StrObj *s, usize from, usize to)
{
    usize a = str_offset_of(s, from);
    usize b = str_offset_of(s, to);
    return Str(s->bytes() + a, b - a);
}

// The second operand of a two-string method, checked.
StrObj *other_str(Value v, Str who)
{
    if (!is_str(v)) {
        Buf<96> b;
        b.put(who).put("() argument must be a str");
        return err_set2("TypeError", b.str(), type_name(v)), nullptr;
    }
    return str_of(v);
}

Value made(const String &b)
{
    return str_of_bytes(b.str());
}

// ---------------------------------------------------------------- case work

// Every character of `s`, decoded once. The case work looks both ways from a
// sigma, so it wants them all to hand.
bool chars_of(const StrObj *s, Vec<u32> &out)
{
    if (!out.reserve(s->chars))
        return false;
    for (usize i = 0; i < s->len;) {
        u32 cp = 0;
        i += cp_decode(s->str(), i, cp);
        if (!out.push(cp))
            return false;
    }
    return true;
}

// Final_Sigma: a capital sigma after a cased letter, and with none after it,
// ignoring what is case-ignorable either side.
u32 lower_sigma(const Vec<u32> &cs, usize i)
{
    usize j = i;
    while (j > 0 && ucd_is(cs[j - 1], UCD_CASE_IGNORABLE))
        j--;
    bool final = j > 0 && ucd_is(cs[j - 1], UCD_CASED);
    if (final) {
        usize k = i + 1;
        while (k < cs.size() && ucd_is(cs[k], UCD_CASE_IGNORABLE))
            k++;
        final = k == cs.size() || !ucd_is(cs[k], UCD_CASED);
    }
    return final ? 0x3c2 : 0x3c3;
}

usize lower_at(const Vec<u32> &cs, usize i, u32 *out)
{
    if (cs[i] == 0x3a3) {
        out[0] = lower_sigma(cs, i);
        return 1;
    }
    return ucd_map(cs[i], UcdMap::Lower, out);
}

enum class Case : u8 { Lower, Upper, Fold, Swap, Title, Capitalize };

R map_case(const CallArgs &a, Str who, Case how, Value &out)
{
    StrObj *s = self_str(a, who);
    if (!s || !meth_args(a, who, 0, 0))
        return R::Err;
    String b;
    if (s->flags & OBJ_ASCII) {
        // The common case: a byte is a character, and ASCII maps to ASCII.
        bool start = true;
        for (usize i = 0; i < s->len; i++) {
            char c  = s->bytes()[i];
            bool up = c >= 'A' && c <= 'Z', lo = c >= 'a' && c <= 'z';
            bool big = false;
            switch (how) {
            case Case::Lower:
            case Case::Fold:
                big = false;
                break;
            case Case::Upper:
                big = true;
                break;
            case Case::Swap:
                big = lo;
                break;
            case Case::Title:
                big = start;
                break;
            case Case::Capitalize:
                big = i == 0;
                break;
            }
            if (up && !big)
                c = char(c + 32);
            else if (lo && big)
                c = char(c - 32);
            start = !(up || lo);
            if (!b.push(c))
                return oom_err();
        }
        out = made(b);
        return out.is_nil() ? R::Err : R::Ok;
    }
    Vec<u32> cs;
    if (!chars_of(s, cs))
        return oom_err();
    bool cased = false;
    for (usize i = 0; i < cs.size(); i++) {
        u32 cp = cs[i];
        u32 m[UCD_MAP_MAX];
        usize n = 1;
        m[0]    = cp;
        switch (how) {
        case Case::Lower:
            n = lower_at(cs, i, m);
            break;
        case Case::Upper:
            n = ucd_map(cp, UcdMap::Upper, m);
            break;
        case Case::Fold:
            n = ucd_map(cp, UcdMap::Fold, m);
            break;
        case Case::Swap:
            if (ucd_is(cp, UCD_UPPER))
                n = lower_at(cs, i, m);
            else if (ucd_is(cp, UCD_LOWER))
                n = ucd_map(cp, UcdMap::Upper, m);
            break;
        case Case::Title:
            n     = cased ? lower_at(cs, i, m) : ucd_map(cp, UcdMap::Title, m);
            cased = ucd_is(cp, UCD_CASED);
            break;
        case Case::Capitalize:
            n = i ? lower_at(cs, i, m) : ucd_map(cp, UcdMap::Title, m);
            break;
        }
        for (usize k = 0; k < n; k++)
            if (!cp_append(b, m[k]))
                return oom_err();
    }
    out = made(b);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_lower(const CallArgs &a, Value &out)
{
    return map_case(a, "lower", Case::Lower, out);
}

R m_upper(const CallArgs &a, Value &out)
{
    return map_case(a, "upper", Case::Upper, out);
}

R m_swapcase(const CallArgs &a, Value &out)
{
    return map_case(a, "swapcase", Case::Swap, out);
}

R m_casefold(const CallArgs &a, Value &out)
{
    return map_case(a, "casefold", Case::Fold, out);
}

// The first character title case, the rest lower whatever they were.
R m_capitalize(const CallArgs &a, Value &out)
{
    return map_case(a, "capitalize", Case::Capitalize, out);
}

// A word starts after anything that is not cased.
R m_title(const CallArgs &a, Value &out)
{
    return map_case(a, "title", Case::Title, out);
}

// ------------------------------------------------------------- is-predicates

// True when every character has one of `flags` and there is at least one,
// or none at all where `empty` says so.
R every_char(const CallArgs &a, Str who, u16 flags, bool empty, Value &out)
{
    StrObj *s = self_str(a, who);
    if (!s || !meth_args(a, who, 0, 0))
        return R::Err;
    bool all = s->chars ? true : empty;
    for (usize i = 0; i < s->len && all;) {
        u32 cp = 0;
        i += cp_decode(s->str(), i, cp);
        all = (ucd_rec(cp).flags & flags) != 0;
    }
    out = value_bool(all);
    return R::Ok;
}

R m_isalpha(const CallArgs &a, Value &out)
{
    return every_char(a, "isalpha", UCD_ALPHA, false, out);
}

R m_isalnum(const CallArgs &a, Value &out)
{
    return every_char(a, "isalnum", UCD_ALPHA | UCD_DECIMAL | UCD_DIGIT | UCD_NUMERIC, false, out);
}

R m_isdigit(const CallArgs &a, Value &out)
{
    return every_char(a, "isdigit", UCD_DIGIT, false, out);
}

R m_isdecimal(const CallArgs &a, Value &out)
{
    return every_char(a, "isdecimal", UCD_DECIMAL, false, out);
}

R m_isnumeric(const CallArgs &a, Value &out)
{
    return every_char(a, "isnumeric", UCD_NUMERIC, false, out);
}

R m_isspace(const CallArgs &a, Value &out)
{
    return every_char(a, "isspace", UCD_SPACE, false, out);
}

// The empty string is printable, where the others say no.
R m_isprintable(const CallArgs &a, Value &out)
{
    return every_char(a, "isprintable", UCD_PRINTABLE, true, out);
}

// isascii is true of the empty string too.
R m_isascii(const CallArgs &a, Value &out)
{
    StrObj *s = self_str(a, "isascii");
    if (!s || !meth_args(a, "isascii", 0, 0))
        return R::Err;
    out = value_bool((s->flags & OBJ_ASCII) != 0);
    return R::Ok;
}

// Only cased characters decide these two, and a title-case one refuses both.
R cased(const CallArgs &a, Str who, bool want_upper, Value &out)
{
    StrObj *s = self_str(a, who);
    if (!s || !meth_args(a, who, 0, 0))
        return R::Err;
    bool any = false;
    u16 bad  = u16(UCD_TITLE | (want_upper ? UCD_LOWER : UCD_UPPER));
    u16 good = want_upper ? UCD_UPPER : UCD_LOWER;
    for (usize i = 0; i < s->len;) {
        u32 cp = 0;
        i += cp_decode(s->str(), i, cp);
        u16 f = ucd_rec(cp).flags;
        if (f & bad) {
            out = value_bool(false);
            return R::Ok;
        }
        any = any || (f & good);
    }
    out = value_bool(any);
    return R::Ok;
}

R m_islower(const CallArgs &a, Value &out)
{
    return cased(a, "islower", false, out);
}

R m_isupper(const CallArgs &a, Value &out)
{
    return cased(a, "isupper", true, out);
}

R m_istitle(const CallArgs &a, Value &out)
{
    StrObj *s = self_str(a, "istitle");
    if (!s || !meth_args(a, "istitle", 0, 0))
        return R::Err;
    bool any = false, previous = false;
    for (usize i = 0; i < s->len;) {
        u32 cp = 0;
        i += cp_decode(s->str(), i, cp);
        u16 f = ucd_rec(cp).flags;
        if (f & (UCD_UPPER | UCD_TITLE)) {
            if (previous) {
                out = value_bool(false);
                return R::Ok;
            }
            previous = any = true;
        } else if (f & UCD_LOWER) {
            if (!previous) {
                out = value_bool(false);
                return R::Ok;
            }
            previous = any = true;
        } else {
            previous = false;
        }
    }
    out = value_bool(any);
    return R::Ok;
}

R m_isidentifier(const CallArgs &a, Value &out)
{
    StrObj *s = self_str(a, "isidentifier");
    if (!s || !meth_args(a, "isidentifier", 0, 0))
        return R::Err;
    bool ok = s->chars != 0, first = true;
    for (usize i = 0; i < s->len && ok;) {
        u32 cp = 0;
        i += cp_decode(s->str(), i, cp);
        ok    = first ? (ucd_is(cp, UCD_XID_START) || cp == '_') : ucd_is(cp, UCD_XID_CONTINUE);
        first = false;
    }
    out = value_bool(ok);
    return R::Ok;
}

// ------------------------------------------------------------------ search

// The byte offset of `sub` in the characters [from, to) of `s`, or npos.
usize find_in(const StrObj *s, Str sub, usize from, usize to, bool last)
{
    usize a  = str_offset_of(s, from);
    usize b  = str_offset_of(s, to);
    Str hay  = Str(s->bytes() + a, b - a);
    usize at = Str::npos;
    for (usize i = hay.find(sub); i != Str::npos; i = hay.find(sub, i + 1)) {
        at = i;
        if (!last)
            break;
        if (sub.empty())
            break;
    }
    if (last && sub.empty())
        at = hay.size();
    return at == Str::npos ? Str::npos : at + a;
}

R search(const CallArgs &a, Str who, bool last, bool raising, Value &out)
{
    StrObj *s = self_str(a, who);
    if (!s)
        return R::Err;
    static const Str NAMES[] = { "sub", "start", "end" };
    Value got[3];
    if (!meth_take(a, who, NAMES, 1, got))
        return R::Err;
    StrObj *sub = other_str(got[0], who);
    if (!sub)
        return R::Err;
    usize from = 0, to = 0;
    if (!span_of(got[1], got[2], s->chars, from, to))
        return R::Err;
    usize at = find_in(s, sub->str(), from, to, last);
    if (at == Str::npos) {
        if (raising)
            return err_set("ValueError", "substring not found");
        out = Value::of_int(-1);
        return R::Ok;
    }
    out = Value::of_int(i32(chars_before(s, at)));
    return R::Ok;
}

R m_find(const CallArgs &a, Value &out)
{
    return search(a, "find", false, false, out);
}

// str.format(*args, **kwargs). The positional arguments become a tuple and
// the keywords a dict, which is what a replacement field looks names up in.
R m_format(const CallArgs &a, Value &out)
{
    StrObj *self = self_str(a, "format");
    if (!self)
        return R::Err;
    Root args{ obj_value(tuple_new(a.nargs - 1)) };
    if (args.v.is_nil())
        return oom_err();
    TupleObj *t = static_cast<TupleObj *>(args.v.obj());
    for (u32 i = 1; i < a.nargs; i++)
        t->items()[i - 1] = a.args[i];

    Root kw;
    if (a.nkw) {
        kw = obj_value(dict_new());
        if (kw.v.is_nil())
            return oom_err();
        for (u32 i = 0; i < a.nkw; i++)
            if (dict_set(static_cast<DictObj *>(kw.v.obj()), a.kwnames[i], a.kwvals[i]) != R::Ok)
                return R::Err;
    }
    out = str_format_call(obj_value(self), args.v, kw.v, Value());
    return out.is_nil() ? R::Err : R::Ok;
}

// str.format_map(mapping): the same, but a name is looked up in the mapping
// rather than copied into a dict first, so a class with __getitem__ works.
R m_format_map(const CallArgs &a, Value &out)
{
    StrObj *self = self_str(a, "format_map");
    if (!self || !meth_args(a, "format_map", 1, 1))
        return R::Err;
    Root args{ obj_value(tuple_new(0)) };
    if (args.v.is_nil())
        return oom_err();
    out = str_format_call(obj_value(self), args.v, Value(), a.args[1]);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_rfind(const CallArgs &a, Value &out)
{
    return search(a, "rfind", true, false, out);
}

R m_index(const CallArgs &a, Value &out)
{
    return search(a, "index", false, true, out);
}

R m_rindex(const CallArgs &a, Value &out)
{
    return search(a, "rindex", true, true, out);
}

R m_count(const CallArgs &a, Value &out)
{
    StrObj *s = self_str(a, "count");
    if (!s)
        return R::Err;
    static const Str NAMES[] = { "sub", "start", "end" };
    Value got[3];
    if (!meth_take(a, "count", NAMES, 1, got))
        return R::Err;
    StrObj *sub = other_str(got[0], "count");
    if (!sub)
        return R::Err;
    usize from = 0, to = 0;
    if (!span_of(got[1], got[2], s->chars, from, to))
        return R::Err;

    Str hay = slice_bytes(s, from, to);
    Str nee = sub->str();
    i64 n   = 0;
    if (nee.empty()) {
        // The empty string sits in every gap, the two ends included.
        n = i64(to - from) + 1;
    } else {
        for (usize i = hay.find(nee); i != Str::npos; i = hay.find(nee, i + nee.size()))
            n++;
    }
    out = Value::of_int(i32(n));
    return R::Ok;
}

// startswith and endswith. Either takes a tuple of candidates.
R affix(const CallArgs &a, Str who, bool end, Value &out)
{
    StrObj *s = self_str(a, who);
    if (!s)
        return R::Err;
    static const Str NAMES[] = { "prefix", "start", "end" };
    Value got[3];
    if (!meth_take(a, who, NAMES, 1, got))
        return R::Err;
    usize from = 0, to = 0;
    if (!span_of(got[1], got[2], s->chars, from, to))
        return R::Err;
    Str hay = slice_bytes(s, from, to);

    Value one[1]       = { got[0] };
    const Value *cands = one;
    usize n            = 1;
    if (is_tuple(got[0])) {
        cands = static_cast<TupleObj *>(got[0].obj())->items();
        n     = static_cast<TupleObj *>(got[0].obj())->len;
    }
    for (usize i = 0; i < n; i++) {
        StrObj *c = other_str(cands[i], who);
        if (!c)
            return R::Err;
        if (end ? hay.ends_with(c->str()) : hay.starts_with(c->str())) {
            out = value_bool(true);
            return R::Ok;
        }
    }
    out = value_bool(false);
    return R::Ok;
}

R m_startswith(const CallArgs &a, Value &out)
{
    return affix(a, "startswith", false, out);
}

R m_endswith(const CallArgs &a, Value &out)
{
    return affix(a, "endswith", true, out);
}

// --------------------------------------------------------------- stripping

// Is `cp` in `chars`? An empty `chars` means whitespace, which is what a
// missing argument asks for.
bool in_set(Str chars, bool space, u32 cp)
{
    if (space)
        return ucd_is(cp, UCD_SPACE);
    for (usize i = 0; i < chars.size();) {
        u32 c = 0;
        i += cp_decode(chars, i, c);
        if (c == cp)
            return true;
    }
    return false;
}

R strip_one(const CallArgs &a, Str who, bool left, bool right, Value &out)
{
    StrObj *s = self_str(a, who);
    if (!s)
        return R::Err;
    static const Str NAMES[] = { "chars" };
    Value got[1];
    if (!meth_take(a, who, NAMES, 0, got))
        return R::Err;
    bool space = got[0].is_nil() || is_none(got[0]);
    Str chars;
    if (!space) {
        StrObj *c = other_str(got[0], who);
        if (!c)
            return R::Err;
        chars = c->str();
    }

    usize from = 0, to = s->chars;
    while (left && from < to && in_set(chars, space, str_char_at(s, from)))
        from++;
    while (right && to > from && in_set(chars, space, str_char_at(s, to - 1)))
        to--;
    // Nothing stripped: hand back the string itself, as CPython does.
    if (!from && to == s->chars) {
        out = obj_value(s);
        return R::Ok;
    }
    out = str_of_bytes(slice_bytes(s, from, to));
    return out.is_nil() ? R::Err : R::Ok;
}

R m_strip(const CallArgs &a, Value &out)
{
    return strip_one(a, "strip", true, true, out);
}

R m_lstrip(const CallArgs &a, Value &out)
{
    return strip_one(a, "lstrip", true, false, out);
}

R m_rstrip(const CallArgs &a, Value &out)
{
    return strip_one(a, "rstrip", false, true, out);
}

R m_removeprefix(const CallArgs &a, Value &out)
{
    StrObj *s = self_str(a, "removeprefix");
    if (!s || !meth_args(a, "removeprefix", 1, 1))
        return R::Err;
    StrObj *p = other_str(a.args[1], "removeprefix");
    if (!p)
        return R::Err;
    Str all = s->str();
    out     = str_of_bytes(all.starts_with(p->str()) ? all.substr(p->len) : all);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_removesuffix(const CallArgs &a, Value &out)
{
    StrObj *s = self_str(a, "removesuffix");
    if (!s || !meth_args(a, "removesuffix", 1, 1))
        return R::Err;
    StrObj *p = other_str(a.args[1], "removesuffix");
    if (!p)
        return R::Err;
    Str all = s->str();
    out =
        str_of_bytes(p->len && all.ends_with(p->str()) ? all.substr(0, all.size() - p->len) : all);
    return out.is_nil() ? R::Err : R::Ok;
}

// ---------------------------------------------------------------- splitting

bool push_str(ListObj *l, Str s)
{
    Root rl{ obj_value(l) };
    Value v = str_of_bytes(s);
    return !v.is_nil() && list_push(list_of(rl.v), v);
}

// The width of the whitespace character at `i`, or 0.
usize space_at(Str s, usize i)
{
    u8 c = u8(s[i]);
    if (c < 0x80)
        return c == ' ' || (c >= 9 && c <= 13) || (c >= 0x1c && c <= 0x1f) ? 1 : 0;
    u32 cp  = 0;
    usize w = cp_decode(s, i, cp);
    return ucd_is(cp, UCD_SPACE) ? w : 0;
}

// Where the character that ends at `i` starts.
usize char_before(Str s, usize i)
{
    do
        i--;
    while (i > 0 && (u8(s[i]) & 0xc0) == 0x80);
    return i;
}

bool space_before(Str s, usize i)
{
    return space_at(s, char_before(s, i)) != 0;
}

usize skip_space(Str s, usize i)
{
    for (usize w; i < s.size() && (w = space_at(s, i));)
        i += w;
    return i;
}

usize skip_word(Str s, usize i)
{
    while (i < s.size() && !space_at(s, i))
        i += cp_width(u8(s[i]));
    return i;
}

// sep=None: runs of whitespace, with the ends dropped. A different algorithm
// from a named separator, not a special case of it.
R split_space(Str all, i64 most, bool right, Value &out)
{
    ListObj *l = list_new();
    if (!l)
        return oom_err();
    Root rl{ obj_value(l) };

    Vec<Str> parts;
    if (!right) {
        usize i = 0;
        while (i < all.size()) {
            i = skip_space(all, i);
            if (i >= all.size())
                break;
            if (most >= 0 && i64(parts.size()) == most) {
                if (!parts.push(all.substr(i)))
                    return oom_err();
                break;
            }
            usize b = i;
            i       = skip_word(all, i);
            if (!parts.push(all.substr(b, i - b)))
                return oom_err();
        }
    } else {
        usize i = all.size();
        while (i > 0) {
            while (i > 0 && space_before(all, i))
                i = char_before(all, i);
            if (i == 0)
                break;
            if (most >= 0 && i64(parts.size()) == most) {
                if (!parts.push(all.substr(0, i)))
                    return oom_err();
                break;
            }
            usize e = i;
            while (i > 0 && !space_before(all, i))
                i = char_before(all, i);
            if (!parts.push(all.substr(i, e - i)))
                return oom_err();
        }
        for (usize k = 0; k < parts.size() / 2; k++) {
            Str t                       = parts[k];
            parts[k]                    = parts[parts.size() - 1 - k];
            parts[parts.size() - 1 - k] = t;
        }
    }
    for (usize k = 0; k < parts.size(); k++)
        if (!push_str(list_of(rl.v), parts[k]))
            return oom_err();
    out = rl.v;
    return R::Ok;
}

R split_sep(Str all, Str sep, i64 most, bool right, Value &out)
{
    if (sep.empty())
        return err_set("ValueError", "empty separator");
    ListObj *l = list_new();
    if (!l)
        return oom_err();
    Root rl{ obj_value(l) };

    Vec<Str> parts;
    if (!right) {
        usize at = 0;
        for (;;) {
            if (most >= 0 && i64(parts.size()) == most)
                break;
            usize i = all.find(sep, at);
            if (i == Str::npos)
                break;
            if (!parts.push(all.substr(at, i - at)))
                return oom_err();
            at = i + sep.size();
        }
        if (!parts.push(all.substr(at)))
            return oom_err();
    } else {
        usize end = all.size();
        for (;;) {
            if (most >= 0 && i64(parts.size()) == most)
                break;
            // The last occurrence at or before `end`.
            usize found = Str::npos;
            for (usize i = all.find(sep); i != Str::npos && i + sep.size() <= end;
                 i       = all.find(sep, i + 1))
                found = i;
            if (found == Str::npos)
                break;
            if (!parts.push(all.substr(found + sep.size(), end - found - sep.size())))
                return oom_err();
            end = found;
        }
        if (!parts.push(all.substr(0, end)))
            return oom_err();
        for (usize k = 0; k < parts.size() / 2; k++) {
            Str t                       = parts[k];
            parts[k]                    = parts[parts.size() - 1 - k];
            parts[parts.size() - 1 - k] = t;
        }
    }
    for (usize k = 0; k < parts.size(); k++)
        if (!push_str(list_of(rl.v), parts[k]))
            return oom_err();
    out = rl.v;
    return R::Ok;
}

R split_any(const CallArgs &a, Str who, bool right, Value &out)
{
    StrObj *s = self_str(a, who);
    if (!s)
        return R::Err;
    static const Str NAMES[] = { "sep", "maxsplit" };
    Value got[2];
    if (!meth_take(a, who, NAMES, 0, got))
        return R::Err;
    i64 most = -1;
    if (!got[1].is_nil() && !as_index(got[1], most))
        return err_set2("TypeError", "maxsplit must be an integer", type_name(got[1]));
    if (got[0].is_nil() || is_none(got[0]))
        return split_space(s->str(), most, right, out);
    StrObj *sep = other_str(got[0], who);
    if (!sep)
        return R::Err;
    return split_sep(s->str(), sep->str(), most, right, out);
}

R m_split(const CallArgs &a, Value &out)
{
    return split_any(a, "split", false, out);
}

R m_rsplit(const CallArgs &a, Value &out)
{
    return split_any(a, "rsplit", true, out);
}

R m_splitlines(const CallArgs &a, Value &out)
{
    StrObj *s = self_str(a, "splitlines");
    if (!s)
        return R::Err;
    static const Str NAMES[] = { "keepends" };
    Value got[1];
    if (!meth_take(a, "splitlines", NAMES, 0, got))
        return R::Err;
    bool keep = !got[0].is_nil() && py_truth(got[0]);

    ListObj *l = list_new();
    if (!l)
        return oom_err();
    Root rl{ obj_value(l) };
    Str all = s->str();
    usize i = 0;
    // The width of the line break at `k`, or 0: \n, \r, \v, \f, the three
    // separators, and past ASCII what Unicode calls one.
    auto brk = [&](usize k) -> usize {
        u8 c = u8(all[k]);
        if (c < 0x80)
            return (c >= 0x0a && c <= 0x0d) || (c >= 0x1c && c <= 0x1e) ? 1 : 0;
        u32 cp  = 0;
        usize w = cp_decode(all, k, cp);
        return ucd_is(cp, UCD_LINEBREAK) ? w : 0;
    };
    while (i < all.size()) {
        usize b = i, w = 0;
        while (i < all.size() && !(w = brk(i)))
            i += cp_width(u8(all[i]));
        usize e = i;
        if (i < all.size()) {
            bool cr = all[i] == '\r';
            i += w;
            if (cr && i < all.size() && all[i] == '\n')
                i++;
        }
        if (!push_str(list_of(rl.v), all.substr(b, (keep ? i : e) - b)))
            return oom_err();
    }
    out = rl.v;
    return R::Ok;
}

R partition_any(const CallArgs &a, Str who, bool right, Value &out)
{
    StrObj *s = self_str(a, who);
    if (!s || !meth_args(a, who, 1, 1))
        return R::Err;
    StrObj *sep = other_str(a.args[1], who);
    if (!sep)
        return R::Err;
    if (!sep->len)
        return err_set("ValueError", "empty separator");

    Str all  = s->str();
    Str nee  = sep->str();
    usize at = Str::npos;
    if (!right) {
        at = all.find(nee);
    } else {
        for (usize i = all.find(nee); i != Str::npos; i = all.find(nee, i + 1))
            at = i;
    }

    Str head, mid, tail;
    if (at == Str::npos) {
        // The whole string goes to the near end; the other two are empty.
        head = right ? Str() : all;
        tail = right ? all : Str();
    } else {
        head = all.substr(0, at);
        mid  = nee;
        tail = all.substr(at + nee.size());
    }
    TupleObj *t = tuple_new(3);
    if (!t)
        return oom_err();
    Root rt{ obj_value(t) };
    Str parts[3] = { head, mid, tail };
    for (usize k = 0; k < 3; k++) {
        Value v = str_of_bytes(parts[k]);
        if (v.is_nil())
            return R::Err;
        static_cast<TupleObj *>(rt.v.obj())->items()[k] = v;
    }
    out = rt.v;
    return R::Ok;
}

R m_partition(const CallArgs &a, Value &out)
{
    return partition_any(a, "partition", false, out);
}

R m_rpartition(const CallArgs &a, Value &out)
{
    return partition_any(a, "rpartition", true, out);
}

// ------------------------------------------------------------------ joining

R m_join(const CallArgs &a, Value &out)
{
    StrObj *s = self_str(a, "join");
    if (!s || !meth_args(a, "join", 1, 1))
        return R::Err;
    if (iter_needs_vm(a.args[1]))
        return iter_park(a, 1, m_join, out);
    Root sep{ obj_value(s) };
    ListObj *items = py_list_of(a.args[1]);
    if (!items)
        return R::Err;
    Root ri{ obj_value(items) };

    String b;
    Vec<Value> &xs = list_of(ri.v)->items;
    for (usize i = 0; i < xs.size(); i++) {
        if (!is_str(xs[i]))
            return err_set2("TypeError", "sequence item: expected str", type_name(xs[i]));
        if (i && !b.append(str_of(sep.v)->str()))
            return oom_err();
        if (!b.append(str_of(xs[i])->str()))
            return oom_err();
    }
    out = made(b);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_replace(const CallArgs &a, Value &out)
{
    StrObj *s = self_str(a, "replace");
    if (!s)
        return R::Err;
    static const Str NAMES[] = { "old", "new", "count" };
    Value got[3];
    if (!meth_take(a, "replace", NAMES, 2, got))
        return R::Err;
    StrObj *old = other_str(got[0], "replace");
    if (!old)
        return R::Err;
    StrObj *neu = other_str(got[1], "replace");
    if (!neu)
        return R::Err;
    i64 most = -1;
    if (!got[2].is_nil() && !as_index(got[2], most))
        return err_set2("TypeError", "count must be an integer", type_name(got[2]));

    Str all = s->str(), o = old->str(), n = neu->str();
    String b;
    if (o.empty()) {
        // Between every pair of characters, and at both ends.
        i64 done = 0;
        usize i  = 0;
        for (;;) {
            if (most < 0 || done < most) {
                if (!b.append(n))
                    return oom_err();
                done++;
            }
            if (i >= all.size())
                break;
            usize w = cp_width(u8(all[i]));
            if (!b.append(all.substr(i, w)))
                return oom_err();
            i += w;
        }
    } else {
        usize at = 0;
        i64 done = 0;
        for (;;) {
            usize i = (most >= 0 && done >= most) ? Str::npos : all.find(o, at);
            if (i == Str::npos)
                break;
            if (!b.append(all.substr(at, i - at)) || !b.append(n))
                return oom_err();
            at = i + o.size();
            done++;
        }
        if (!b.append(all.substr(at)))
            return oom_err();
    }
    out = made(b);
    return out.is_nil() ? R::Err : R::Ok;
}

// ------------------------------------------------------------------ padding

// The fill character of center, ljust and rjust. Exactly one character.
R fill_of(Value v, Str who, Str &out)
{
    if (v.is_nil())
        return out = " ", R::Ok;
    if (!is_str(v) || str_of(v)->chars != 1) {
        Buf<96> b;
        b.put(who).put("() fill character must be exactly one character");
        return err_set("TypeError", b.str());
    }
    out = str_of(v)->str();
    return R::Ok;
}

R pad(const CallArgs &a, Str who, int side, Value &out)
{
    StrObj *s = self_str(a, who);
    if (!s)
        return R::Err;
    static const Str NAMES[] = { "width", "fillchar" };
    Value got[2];
    if (!meth_take(a, who, NAMES, 1, got))
        return R::Err;
    i64 width = 0;
    if (!as_index(got[0], width))
        return err_set2("TypeError", "width must be an integer", type_name(got[0]));
    Str fill;
    if (fill_of(got[1], who, fill) != R::Ok)
        return R::Err;

    i64 need = width - i64(s->chars);
    if (need <= 0) {
        out = obj_value(s);
        return R::Ok;
    }
    // center puts the odd character on the right, as CPython does.
    i64 left  = side < 0 ? 0 : side > 0 ? need : need / 2;
    i64 right = need - left;
    String b;
    for (i64 i = 0; i < left; i++)
        if (!b.append(fill))
            return oom_err();
    if (!b.append(s->str()))
        return oom_err();
    for (i64 i = 0; i < right; i++)
        if (!b.append(fill))
            return oom_err();
    out = made(b);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_ljust(const CallArgs &a, Value &out)
{
    return pad(a, "ljust", -1, out);
}

R m_rjust(const CallArgs &a, Value &out)
{
    return pad(a, "rjust", 1, out);
}

R m_center(const CallArgs &a, Value &out)
{
    return pad(a, "center", 0, out);
}

// zfill keeps a leading sign in front of the zeros.
R m_zfill(const CallArgs &a, Value &out)
{
    StrObj *s = self_str(a, "zfill");
    if (!s || !meth_args(a, "zfill", 1, 1))
        return R::Err;
    i64 width = 0;
    if (!as_index(a.args[1], width))
        return err_set2("TypeError", "width must be an integer", type_name(a.args[1]));
    i64 need = width - i64(s->chars);
    if (need <= 0) {
        out = obj_value(s);
        return R::Ok;
    }
    Str all  = s->str();
    bool sgn = all.size() && (all[0] == '+' || all[0] == '-');
    String b;
    if (sgn && !b.push(all[0]))
        return oom_err();
    for (i64 i = 0; i < need; i++)
        if (!b.push('0'))
            return oom_err();
    if (!b.append(sgn ? all.substr(1) : all))
        return oom_err();
    out = made(b);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_expandtabs(const CallArgs &a, Value &out)
{
    StrObj *s = self_str(a, "expandtabs");
    if (!s)
        return R::Err;
    static const Str NAMES[] = { "tabsize" };
    Value got[1];
    if (!meth_take(a, "expandtabs", NAMES, 0, got))
        return R::Err;
    i64 tab = 8;
    if (!got[0].is_nil() && !as_index(got[0], tab))
        return err_set2("TypeError", "tabsize must be an integer", type_name(got[0]));

    String b;
    i64 col = 0;
    for (usize i = 0; i < s->len;) {
        u32 cp  = 0;
        usize w = cp_decode(s->str(), i, cp);
        if (cp == '\t') {
            i64 n = tab > 0 ? tab - col % tab : 0;
            for (i64 k = 0; k < n; k++)
                if (!b.push(' '))
                    return oom_err();
            col += n;
        } else {
            if (!b.append(s->str().substr(i, w)))
                return oom_err();
            col = (cp == '\n' || cp == '\r') ? 0 : col + 1;
        }
        i += w;
    }
    out = made(b);
    return out.is_nil() ? R::Err : R::Ok;
}

// ------------------------------------------------------- encode and translate

R m_encode(const CallArgs &a, Value &out)
{
    StrObj *s = self_str(a, "encode");
    if (!s)
        return R::Err;
    static const Str NAMES[] = { "encoding", "errors" };
    Value got[2];
    if (!meth_take(a, "encode", NAMES, 0, got))
        return R::Err;
    return text_encode(obj_value(s), got[0], got[1], out);
}

// str.maketrans(x[, y[, z]]): a dict from an ordinal to its replacement.
R m_maketrans(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs < 1 || a.nargs > 3)
        return err_set("TypeError", "maketrans() takes from 1 to 3 arguments");
    DictObj *d = dict_new();
    if (!d)
        return oom_err();
    Root rd{ obj_value(d) };

    if (a.nargs == 1) {
        if (!is_dict(a.args[0]))
            return err_set("TypeError", "maketrans() with one argument takes a dict");
        usize at = 0;
        Value k, v;
        Root src{ a.args[0] };
        while (table_next(static_cast<DictObj *>(src.v.obj())->t, at, k, v)) {
            Value key = k;
            if (is_str(key)) {
                if (str_of(key)->chars != 1)
                    return err_set("ValueError", "string keys must be of length 1");
                key = Value::of_int(i32(str_char_at(str_of(key), 0)));
            }
            if (dict_set(static_cast<DictObj *>(rd.v.obj()), key, v) != R::Ok)
                return R::Err;
        }
        out = rd.v;
        return R::Ok;
    }

    if (!is_str(a.args[0]) || !is_str(a.args[1]))
        return err_set("TypeError", "maketrans() arguments must be str");
    StrObj *x = str_of(a.args[0]), *y = str_of(a.args[1]);
    if (x->chars != y->chars)
        return err_set("ValueError", "the first two maketrans arguments must have equal length");
    Root rx{ a.args[0] }, ry{ a.args[1] };
    for (usize i = 0; i < x->chars; i++) {
        // Ordinal to ordinal, as CPython's table holds it.
        Value to  = Value::of_int(i32(str_char_at(str_of(ry.v), i)));
        Value key = Value::of_int(i32(str_char_at(str_of(rx.v), i)));
        if (dict_set(static_cast<DictObj *>(rd.v.obj()), key, to) != R::Ok)
            return R::Err;
    }
    if (a.nargs == 3) {
        if (!is_str(a.args[2]))
            return err_set("TypeError", "maketrans() arguments must be str");
        Root rz{ a.args[2] };
        for (usize i = 0; i < str_of(rz.v)->chars; i++) {
            Value key = Value::of_int(i32(str_char_at(str_of(rz.v), i)));
            if (dict_set(static_cast<DictObj *>(rd.v.obj()), key, value_none()) != R::Ok)
                return R::Err;
        }
    }
    out = rd.v;
    return R::Ok;
}

R m_translate(const CallArgs &a, Value &out)
{
    StrObj *s = self_str(a, "translate");
    if (!s || !meth_args(a, "translate", 1, 1))
        return R::Err;
    Root rs{ obj_value(s) }, tab{ a.args[1] };
    String b;
    for (usize i = 0; i < str_of(rs.v)->len;) {
        u32 cp   = 0;
        usize w  = cp_decode(str_of(rs.v)->str(), i, cp);
        Str here = str_of(rs.v)->str().substr(i, w);
        i += w;

        Value to;
        R r = py_getitem(tab.v, Value::of_int(i32(cp)), to);
        if (r == R::Err) {
            // A missing key leaves the character as it was.
            if (err_kind() != "KeyError" && err_kind() != "IndexError")
                return R::Err;
            err_clear();
            if (!b.append(here))
                return oom_err();
            continue;
        }
        if (is_none(to))
            continue;
        i64 n = 0;
        if (as_index(to, n)) {
            if (n < 0 || n > 0x10ffff)
                return err_set("ValueError", "character mapping must be in range(0x110000)");
            if (!cp_append(b, u32(n)))
                return oom_err();
        } else if (is_str(to)) {
            if (!b.append(str_of(to)->str()))
                return oom_err();
        } else {
            return err_set2("TypeError", "character mapping must be an int, a str or None",
                            type_name(to));
        }
    }
    out = made(b);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method STR[] = {
    { "capitalize", m_capitalize },
    { "casefold", m_casefold },
    { "center", m_center },
    { "count", m_count },
    { "encode", m_encode },
    { "endswith", m_endswith },
    { "expandtabs", m_expandtabs },
    { "find", m_find },
    { "format", m_format },
    { "format_map", m_format_map },
    { "index", m_index },
    { "isalnum", m_isalnum },
    { "isalpha", m_isalpha },
    { "isascii", m_isascii },
    { "isdecimal", m_isdecimal },
    { "isdigit", m_isdigit },
    { "isidentifier", m_isidentifier },
    { "islower", m_islower },
    { "isnumeric", m_isnumeric },
    { "isprintable", m_isprintable },
    { "isspace", m_isspace },
    { "istitle", m_istitle },
    { "isupper", m_isupper },
    { "join", m_join },
    { "ljust", m_ljust },
    { "lower", m_lower },
    { "lstrip", m_lstrip },
    { "maketrans", m_maketrans, true },
    { "partition", m_partition },
    { "removeprefix", m_removeprefix },
    { "removesuffix", m_removesuffix },
    { "replace", m_replace },
    { "rfind", m_rfind },
    { "rindex", m_rindex },
    { "rjust", m_rjust },
    { "rpartition", m_rpartition },
    { "rsplit", m_rsplit },
    { "rstrip", m_rstrip },
    { "split", m_split },
    { "splitlines", m_splitlines },
    { "startswith", m_startswith },
    { "strip", m_strip },
    { "swapcase", m_swapcase },
    { "title", m_title },
    { "translate", m_translate },
    { "upper", m_upper },
    { "zfill", m_zfill },
};

} // namespace

bool str_methods()
{
    return method_install(&str_type, STR);
}
