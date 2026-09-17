// The methods of bytes and bytearray, and memoryview.
//
// One table serves both. Every read-only method reads its octets through
// self_bytes and returns self's own type, as CPython does. bytearray adds the
// mutating half on top.
#include "binfmt.h"
#include "call.h"
#include "codec.h"
#include "gc.h"
#include "gen.h"
#include "iter.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"

namespace {

// ------------------------------------------------------------------ helpers

// self as octets. False leaves a TypeError pending.
bool self_bytes(const CallArgs &a, Str who, Value &self, Str &out)
{
    self = a.nargs ? method_self(a.args[0]) : Value();
    if (!bytes_like(self, out)) {
        Buf<96> b;
        b.put(who).put("() requires a bytes-like self");
        return err_set2("TypeError", b.str(), type_name(self)), false;
    }
    return true;
}

// A result of the same type as self.
Value like(Value self, Str s)
{
    return is_bytearray(self) ? bytearray_new(s) : bytes_new(s);
}

// An argument that must be bytes-like.
bool arg_bytes(Value v, Str who, Str &out)
{
    if (!bytes_like(v, out)) {
        Buf<96> b;
        b.put(who).put("() argument must be bytes-like");
        return err_set2("TypeError", b.str(), type_name(v)), false;
    }
    return true;
}

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

// Case and class are ASCII here. A byte is not a codepoint, and CPython's
// bytes methods are ASCII-only too.
u8 lower_one(u8 c)
{
    return c >= 'A' && c <= 'Z' ? u8(c + 32) : c;
}

u8 upper_one(u8 c)
{
    return c >= 'a' && c <= 'z' ? u8(c - 32) : c;
}

bool alpha_one(u8 c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool digit_one(u8 c)
{
    return c >= '0' && c <= '9';
}

bool space_one(u8 c)
{
    return c == ' ' || (c >= 0x09 && c <= 0x0d);
}

// ------------------------------------------------------------------- case

using ByteFn = u8 (*)(u8);

R map_bytes(const CallArgs &a, Str who, ByteFn fn, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, who, self, s) || !meth_args(a, who, 0, 0))
        return R::Err;
    String b;
    for (usize i = 0; i < s.size(); i++)
        if (!b.push(char(fn(u8(s[i])))))
            return oom_err();
    out = like(self, b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

u8 swap_one(u8 c)
{
    return alpha_one(c) ? (c >= 'a' ? upper_one(c) : lower_one(c)) : c;
}

R m_lower(const CallArgs &a, Value &out)
{
    return map_bytes(a, "lower", lower_one, out);
}

R m_upper(const CallArgs &a, Value &out)
{
    return map_bytes(a, "upper", upper_one, out);
}

R m_swapcase(const CallArgs &a, Value &out)
{
    return map_bytes(a, "swapcase", swap_one, out);
}

R m_capitalize(const CallArgs &a, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, "capitalize", self, s) || !meth_args(a, "capitalize", 0, 0))
        return R::Err;
    String b;
    for (usize i = 0; i < s.size(); i++)
        if (!b.push(char(i ? lower_one(u8(s[i])) : upper_one(u8(s[i])))))
            return oom_err();
    out = like(self, b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R m_title(const CallArgs &a, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, "title", self, s) || !meth_args(a, "title", 0, 0))
        return R::Err;
    String b;
    bool start = true;
    for (usize i = 0; i < s.size(); i++) {
        u8 c = u8(s[i]);
        if (!b.push(char(start ? upper_one(c) : lower_one(c))))
            return oom_err();
        start = !alpha_one(c);
    }
    out = like(self, b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

// --------------------------------------------------------------- predicates

using ByteTest = bool (*)(u8);

R every_byte(const CallArgs &a, Str who, ByteTest fn, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, who, self, s) || !meth_args(a, who, 0, 0))
        return R::Err;
    bool all = !s.empty();
    for (usize i = 0; i < s.size() && all; i++)
        all = fn(u8(s[i]));
    out = value_bool(all);
    return R::Ok;
}

bool alnum_one(u8 c)
{
    return alpha_one(c) || digit_one(c);
}

R m_isalpha(const CallArgs &a, Value &out)
{
    return every_byte(a, "isalpha", alpha_one, out);
}

R m_isalnum(const CallArgs &a, Value &out)
{
    return every_byte(a, "isalnum", alnum_one, out);
}

R m_isdigit(const CallArgs &a, Value &out)
{
    return every_byte(a, "isdigit", digit_one, out);
}

R m_isspace(const CallArgs &a, Value &out)
{
    return every_byte(a, "isspace", space_one, out);
}

R m_isascii(const CallArgs &a, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, "isascii", self, s) || !meth_args(a, "isascii", 0, 0))
        return R::Err;
    bool all = true;
    for (usize i = 0; i < s.size() && all; i++)
        all = u8(s[i]) < 0x80;
    out = value_bool(all);
    return R::Ok;
}

R cased(const CallArgs &a, Str who, bool want_upper, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, who, self, s) || !meth_args(a, who, 0, 0))
        return R::Err;
    bool any = false, all = true;
    for (usize i = 0; i < s.size() && all; i++) {
        u8 c = u8(s[i]);
        if (!alpha_one(c))
            continue;
        any = true;
        all = want_upper ? (c < 'a') : (c >= 'a');
    }
    out = value_bool(any && all);
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
    Value self;
    Str s;
    if (!self_bytes(a, "istitle", self, s) || !meth_args(a, "istitle", 0, 0))
        return R::Err;
    bool any = false, ok = true, start = true;
    for (usize i = 0; i < s.size() && ok; i++) {
        u8 c = u8(s[i]);
        if (alpha_one(c)) {
            any = true;
            ok  = start ? c < 'a' : c >= 'a';
        }
        start = !alpha_one(c);
    }
    out = value_bool(any && ok);
    return R::Ok;
}

// ------------------------------------------------------------------ search

R search(const CallArgs &a, Str who, bool last, bool raising, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, who, self, s))
        return R::Err;
    static const Str NAMES[] = { "sub", "start", "end" };
    Value got[3];
    if (!meth_take(a, who, NAMES, 1, got))
        return R::Err;

    Str nee;
    char one = 0;
    i64 n    = 0;
    if (as_index(got[0], n)) {
        if (n < 0 || n > 255)
            return err_set("ValueError", "byte must be in range(0, 256)");
        one = char(n);
        nee = Str(&one, 1);
    } else if (!arg_bytes(got[0], who, nee)) {
        return R::Err;
    }
    usize from = 0, to = 0;
    if (!span_of(got[1], got[2], s.size(), from, to))
        return R::Err;

    Str hay  = s.substr(from, to - from);
    usize at = Str::npos;
    for (usize i = hay.find(nee); i != Str::npos; i = hay.find(nee, i + 1)) {
        at = i;
        if (!last || nee.empty())
            break;
    }
    if (last && nee.empty())
        at = hay.size();
    if (at == Str::npos) {
        if (raising)
            return err_set("ValueError", "subsection not found");
        out = Value::of_int(-1);
        return R::Ok;
    }
    out = Value::of_int(i32(at + from));
    return R::Ok;
}

R m_find(const CallArgs &a, Value &out)
{
    return search(a, "find", false, false, out);
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
    Value self;
    Str s;
    if (!self_bytes(a, "count", self, s))
        return R::Err;
    static const Str NAMES[] = { "sub", "start", "end" };
    Value got[3];
    if (!meth_take(a, "count", NAMES, 1, got))
        return R::Err;
    Str nee;
    char one = 0;
    i64 n    = 0;
    if (as_index(got[0], n)) {
        if (n < 0 || n > 255)
            return err_set("ValueError", "byte must be in range(0, 256)");
        one = char(n);
        nee = Str(&one, 1);
    } else if (!arg_bytes(got[0], "count", nee)) {
        return R::Err;
    }
    usize from = 0, to = 0;
    if (!span_of(got[1], got[2], s.size(), from, to))
        return R::Err;

    Str hay  = s.substr(from, to - from);
    i64 seen = 0;
    if (nee.empty()) {
        seen = i64(hay.size()) + 1;
    } else {
        for (usize i = hay.find(nee); i != Str::npos; i = hay.find(nee, i + nee.size()))
            seen++;
    }
    out = Value::of_int(i32(seen));
    return R::Ok;
}

R affix(const CallArgs &a, Str who, bool end, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, who, self, s))
        return R::Err;
    static const Str NAMES[] = { "prefix", "start", "end" };
    Value got[3];
    if (!meth_take(a, who, NAMES, 1, got))
        return R::Err;
    usize from = 0, to = 0;
    if (!span_of(got[1], got[2], s.size(), from, to))
        return R::Err;
    Str hay = s.substr(from, to - from);

    Value one[1]       = { got[0] };
    const Value *cands = one;
    usize n            = 1;
    if (is_tuple(got[0])) {
        cands = static_cast<TupleObj *>(got[0].obj())->items();
        n     = static_cast<TupleObj *>(got[0].obj())->len;
    }
    for (usize i = 0; i < n; i++) {
        Str c;
        if (!arg_bytes(cands[i], who, c))
            return R::Err;
        if (end ? hay.ends_with(c) : hay.starts_with(c)) {
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

bool in_set(Str chars, bool space, u8 c)
{
    if (space)
        return space_one(c);
    return chars.find(char(c)) != Str::npos;
}

R strip_one(const CallArgs &a, Str who, bool left, bool right, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, who, self, s))
        return R::Err;
    static const Str NAMES[] = { "bytes" };
    Value got[1];
    if (!meth_take(a, who, NAMES, 0, got))
        return R::Err;
    bool space = got[0].is_nil() || is_none(got[0]);
    Str chars;
    if (!space && !arg_bytes(got[0], who, chars))
        return R::Err;

    usize from = 0, to = s.size();
    while (left && from < to && in_set(chars, space, u8(s[from])))
        from++;
    while (right && to > from && in_set(chars, space, u8(s[to - 1])))
        to--;
    // Nothing stripped: hand back the bytes itself, as CPython does.
    if (!from && to == s.size() && is_bytes(self)) {
        out = self;
        return R::Ok;
    }
    out = like(self, s.substr(from, to - from));
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
    Value self;
    Str s;
    if (!self_bytes(a, "removeprefix", self, s) || !meth_args(a, "removeprefix", 1, 1))
        return R::Err;
    Str p;
    if (!arg_bytes(a.args[1], "removeprefix", p))
        return R::Err;
    out = like(self, s.starts_with(p) ? s.substr(p.size()) : s);
    return out.is_nil() ? R::Err : R::Ok;
}

// A bytes-like argument, in CPython's words when it is not one.
bool buffer_arg(Value v, Str &out)
{
    if (bytes_like(v, out))
        return true;
    Buf<128> b;
    b.put("a bytes-like object is required, not '").put(type_name(v)).put('\'');
    return err_set("TypeError", b.str()), false;
}

R m_translate(const CallArgs &a, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, "translate", self, s))
        return R::Err;
    static const Str NAMES[] = { "table", "delete" };
    Value got[2];
    if (!meth_take(a, "translate", NAMES, 2, 1, got))
        return R::Err;
    Str table, del;
    bool mapped = !is_none(got[0]);
    if (mapped && !buffer_arg(got[0], table))
        return R::Err;
    if (mapped && table.size() != 256)
        return err_set("ValueError", "translation table must be 256 characters long");
    if (!got[1].is_nil() && !buffer_arg(got[1], del))
        return R::Err;
    bool gone[256] = {};
    for (usize i = 0; i < del.size(); i++)
        gone[u8(del[i])] = true;
    if (!mapped && del.empty() && is_bytes(self)) {
        out = self;
        return R::Ok;
    }
    String b;
    for (usize i = 0; i < s.size(); i++) {
        u8 c = u8(s[i]);
        if (gone[c])
            continue;
        if (!b.push(mapped ? table[c] : char(c)))
            return oom_err();
    }
    out = like(self, b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

// bytes.maketrans(frm, to): a table of 256, always bytes.
R m_maketrans(const CallArgs &a, Value &out)
{
    if (!args_only(a, "maketrans", 2, 2))
        return R::Err;
    Str frm, to;
    if (!buffer_arg(a.args[0], frm) || !buffer_arg(a.args[1], to))
        return R::Err;
    if (frm.size() != to.size())
        return err_set("ValueError", "maketrans arguments must have same length");
    char table[256];
    for (usize i = 0; i < 256; i++)
        table[i] = char(i);
    for (usize i = 0; i < frm.size(); i++)
        table[u8(frm[i])] = to[i];
    out = bytes_new(Str(table, 256));
    return out.is_nil() ? R::Err : R::Ok;
}

R m_removesuffix(const CallArgs &a, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, "removesuffix", self, s) || !meth_args(a, "removesuffix", 1, 1))
        return R::Err;
    Str p;
    if (!arg_bytes(a.args[1], "removesuffix", p))
        return R::Err;
    out = like(self, !p.empty() && s.ends_with(p) ? s.substr(0, s.size() - p.size()) : s);
    return out.is_nil() ? R::Err : R::Ok;
}

// ---------------------------------------------------------------- splitting

bool push_like(Value self, ListObj *l, Str s)
{
    Root rs{ self }, rl{ obj_value(l) };
    Value v = like(rs.v, s);
    return !v.is_nil() && list_push(list_of(rl.v), v);
}

R split_any(const CallArgs &a, Str who, bool right, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, who, self, s))
        return R::Err;
    static const Str NAMES[] = { "sep", "maxsplit" };
    Value got[2];
    if (!meth_take(a, who, NAMES, 0, got))
        return R::Err;
    i64 most = -1;
    if (!got[1].is_nil() && !as_index(got[1], most))
        return err_set2("TypeError", "maxsplit must be an integer", type_name(got[1]));
    bool space = got[0].is_nil() || is_none(got[0]);
    Str sep;
    if (!space) {
        if (!arg_bytes(got[0], who, sep))
            return R::Err;
        if (sep.empty())
            return err_set("ValueError", "empty separator");
    }

    Root rs{ self };
    ListObj *l = list_new();
    if (!l)
        return oom_err();
    Root rl{ obj_value(l) };
    Vec<Str> parts;

    if (space && !right) {
        usize i = 0;
        while (i < s.size()) {
            while (i < s.size() && space_one(u8(s[i])))
                i++;
            if (i >= s.size())
                break;
            if (most >= 0 && i64(parts.size()) == most) {
                if (!parts.push(s.substr(i)))
                    return oom_err();
                break;
            }
            usize b = i;
            while (i < s.size() && !space_one(u8(s[i])))
                i++;
            if (!parts.push(s.substr(b, i - b)))
                return oom_err();
        }
    } else if (space) {
        usize i = s.size();
        while (i > 0) {
            while (i > 0 && space_one(u8(s[i - 1])))
                i--;
            if (i == 0)
                break;
            if (most >= 0 && i64(parts.size()) == most) {
                if (!parts.push(s.substr(0, i)))
                    return oom_err();
                break;
            }
            usize e = i;
            while (i > 0 && !space_one(u8(s[i - 1])))
                i--;
            if (!parts.push(s.substr(i, e - i)))
                return oom_err();
        }
    } else if (!right) {
        usize at = 0;
        for (;;) {
            if (most >= 0 && i64(parts.size()) == most)
                break;
            usize i = s.find(sep, at);
            if (i == Str::npos)
                break;
            if (!parts.push(s.substr(at, i - at)))
                return oom_err();
            at = i + sep.size();
        }
        if (!parts.push(s.substr(at)))
            return oom_err();
    } else {
        usize end = s.size();
        for (;;) {
            if (most >= 0 && i64(parts.size()) == most)
                break;
            usize found = Str::npos;
            for (usize i = s.find(sep); i != Str::npos && i + sep.size() <= end;
                 i       = s.find(sep, i + 1))
                found = i;
            if (found == Str::npos)
                break;
            if (!parts.push(s.substr(found + sep.size(), end - found - sep.size())))
                return oom_err();
            end = found;
        }
        if (!parts.push(s.substr(0, end)))
            return oom_err();
    }
    if (right)
        for (usize k = 0; k < parts.size() / 2; k++) {
            Str t                       = parts[k];
            parts[k]                    = parts[parts.size() - 1 - k];
            parts[parts.size() - 1 - k] = t;
        }
    for (usize k = 0; k < parts.size(); k++)
        if (!push_like(rs.v, list_of(rl.v), parts[k]))
            return oom_err();
    out = rl.v;
    return R::Ok;
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
    Value self;
    Str s;
    if (!self_bytes(a, "splitlines", self, s))
        return R::Err;
    static const Str NAMES[] = { "keepends" };
    Value got[1];
    if (!meth_take(a, "splitlines", NAMES, 0, got))
        return R::Err;
    bool keep = !got[0].is_nil() && py_truth(got[0]);

    Root rs{ self };
    ListObj *l = list_new();
    if (!l)
        return oom_err();
    Root rl{ obj_value(l) };
    usize i = 0;
    while (i < s.size()) {
        usize b = i;
        while (i < s.size() && s[i] != '\n' && s[i] != '\r')
            i++;
        usize e = i;
        if (i < s.size()) {
            bool cr = s[i] == '\r';
            i++;
            if (cr && i < s.size() && s[i] == '\n')
                i++;
        }
        if (!push_like(rs.v, list_of(rl.v), s.substr(b, (keep ? i : e) - b)))
            return oom_err();
    }
    out = rl.v;
    return R::Ok;
}

R partition_any(const CallArgs &a, Str who, bool right, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, who, self, s) || !meth_args(a, who, 1, 1))
        return R::Err;
    Str nee;
    if (!arg_bytes(a.args[1], who, nee))
        return R::Err;
    if (nee.empty())
        return err_set("ValueError", "empty separator");

    usize at = Str::npos;
    if (!right) {
        at = s.find(nee);
    } else {
        for (usize i = s.find(nee); i != Str::npos; i = s.find(nee, i + 1))
            at = i;
    }
    Str parts[3];
    if (at == Str::npos) {
        parts[0] = right ? Str() : s;
        parts[2] = right ? s : Str();
    } else {
        parts[0] = s.substr(0, at);
        parts[1] = nee;
        parts[2] = s.substr(at + nee.size());
    }
    Root rs{ self };
    TupleObj *t = tuple_new(3);
    if (!t)
        return oom_err();
    Root rt{ obj_value(t) };
    for (usize k = 0; k < 3; k++) {
        Value v = like(rs.v, parts[k]);
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

R m_join(const CallArgs &a, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, "join", self, s) || !meth_args(a, "join", 1, 1))
        return R::Err;
    if (iter_needs_vm(a.args[1]))
        return iter_park(a, 1, m_join, out);
    Root rs{ self };
    ListObj *items = py_list_of(a.args[1]);
    if (!items)
        return R::Err;
    Root ri{ obj_value(items) };

    String b;
    Vec<Value> &xs = list_of(ri.v)->items;
    for (usize i = 0; i < xs.size(); i++) {
        Str one;
        if (!bytes_like(xs[i], one))
            return err_set2("TypeError", "sequence item: expected a bytes-like object",
                            type_name(xs[i]));
        Str sep;
        bytes_like(rs.v, sep);
        if (i && !b.append(sep))
            return oom_err();
        if (!b.append(one))
            return oom_err();
    }
    out = like(rs.v, b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R m_replace(const CallArgs &a, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, "replace", self, s))
        return R::Err;
    static const Str NAMES[] = { "old", "new", "count" };
    Value got[3];
    if (!meth_take(a, "replace", NAMES, 2, got))
        return R::Err;
    Str o, n;
    if (!arg_bytes(got[0], "replace", o) || !arg_bytes(got[1], "replace", n))
        return R::Err;
    i64 most = -1;
    if (!got[2].is_nil() && !as_index(got[2], most))
        return err_set2("TypeError", "count must be an integer", type_name(got[2]));

    String b;
    if (o.empty()) {
        i64 done = 0;
        for (usize i = 0;; i++) {
            if (most < 0 || done < most) {
                if (!b.append(n))
                    return oom_err();
                done++;
            }
            if (i >= s.size())
                break;
            if (!b.push(s[i]))
                return oom_err();
        }
    } else {
        usize at = 0;
        i64 done = 0;
        for (;;) {
            usize i = (most >= 0 && done >= most) ? Str::npos : s.find(o, at);
            if (i == Str::npos)
                break;
            if (!b.append(s.substr(at, i - at)) || !b.append(n))
                return oom_err();
            at = i + o.size();
            done++;
        }
        if (!b.append(s.substr(at)))
            return oom_err();
    }
    out = like(self, b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

// ------------------------------------------------------------------ padding

R fill_of(Value v, Str who, char &out)
{
    if (v.is_nil())
        return out = ' ', R::Ok;
    Str f;
    if (!bytes_like(v, f) || f.size() != 1) {
        Buf<96> b;
        b.put(who).put("() fill character must be exactly one byte");
        return err_set("TypeError", b.str());
    }
    out = f[0];
    return R::Ok;
}

R pad(const CallArgs &a, Str who, int side, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, who, self, s))
        return R::Err;
    static const Str NAMES[] = { "width", "fillbyte" };
    Value got[2];
    if (!meth_take(a, who, NAMES, 1, got))
        return R::Err;
    i64 width = 0;
    if (!as_index(got[0], width))
        return err_set2("TypeError", "width must be an integer", type_name(got[0]));
    char fill = ' ';
    if (fill_of(got[1], who, fill) != R::Ok)
        return R::Err;

    i64 need = width - i64(s.size());
    if (need <= 0) {
        out = like(self, s);
        return out.is_nil() ? R::Err : R::Ok;
    }
    i64 left = side < 0 ? 0 : side > 0 ? need : need / 2 + (need & width & 1);
    String b;
    for (i64 i = 0; i < left; i++)
        if (!b.push(fill))
            return oom_err();
    if (!b.append(s))
        return oom_err();
    for (i64 i = 0; i < need - left; i++)
        if (!b.push(fill))
            return oom_err();
    out = like(self, b.str());
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

R m_zfill(const CallArgs &a, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, "zfill", self, s) || !meth_args(a, "zfill", 1, 1))
        return R::Err;
    i64 width = 0;
    if (!as_index(a.args[1], width))
        return err_set2("TypeError", "width must be an integer", type_name(a.args[1]));
    i64 need = width - i64(s.size());
    if (need <= 0) {
        out = like(self, s);
        return out.is_nil() ? R::Err : R::Ok;
    }
    bool sgn = !s.empty() && (s[0] == '+' || s[0] == '-');
    String b;
    if (sgn && !b.push(s[0]))
        return oom_err();
    for (i64 i = 0; i < need; i++)
        if (!b.push('0'))
            return oom_err();
    if (!b.append(sgn ? s.substr(1) : s))
        return oom_err();
    out = like(self, b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R m_expandtabs(const CallArgs &a, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, "expandtabs", self, s))
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
    for (usize i = 0; i < s.size(); i++) {
        if (s[i] == '\t') {
            i64 n = tab > 0 ? tab - col % tab : 0;
            for (i64 k = 0; k < n; k++)
                if (!b.push(' '))
                    return oom_err();
            col += n;
        } else {
            if (!b.push(s[i]))
                return oom_err();
            col = (s[i] == '\n' || s[i] == '\r') ? 0 : col + 1;
        }
    }
    out = like(self, b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

// ---------------------------------------------------------- hex and decode

R m_hex(const CallArgs &a, Value &out)
{
    Value self;
    Str s;
    static const Str NAMES[] = { "sep", "bytes_per_sep" };
    Value got[2];
    if (!self_bytes(a, "hex", self, s) || !meth_take(a, "hex", NAMES, 0, got))
        return R::Err;
    i64 per = 1;
    if (!got[1].is_nil() && !as_index(got[1], per))
        return err_not_index(got[1]);
    String b;
    if (!hex_with_sep(s, got[0], per, false, b))
        return R::Err;
    out = str_of_bytes(b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

int hex_digit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// bytes.fromhex and bytearray.fromhex. They differ only in what they make.
R from_hex(const CallArgs &a, Str who, bool array, Value &out)
{
    if (a.nkw || a.nargs != 1)
        return err_set2("TypeError", "fromhex() takes exactly one argument", who);
    if (!is_str(a.args[0]))
        return err_set2("TypeError", "fromhex() argument must be a str", type_name(a.args[0]));
    Str s = str_of(a.args[0])->str();
    String b;
    for (usize i = 0; i < s.size();) {
        if (s[i] == ' ') {
            i++;
            continue;
        }
        int hi = i + 1 < s.size() ? hex_digit(s[i]) : -1;
        int lo = i + 1 < s.size() ? hex_digit(s[i + 1]) : -1;
        if (hi < 0 || lo < 0)
            return err_set("ValueError", "non-hexadecimal number found in fromhex() arg");
        if (!b.push(char(hi * 16 + lo)))
            return oom_err();
        i += 2;
    }
    out = array ? bytearray_new(b.str()) : bytes_new(b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R m_fromhex_bytes(const CallArgs &a, Value &out)
{
    return from_hex(a, "bytes", false, out);
}

R m_fromhex_array(const CallArgs &a, Value &out)
{
    return from_hex(a, "bytearray", true, out);
}

R m_decode(const CallArgs &a, Value &out)
{
    Value self;
    Str s;
    if (!self_bytes(a, "decode", self, s))
        return R::Err;
    static const Str NAMES[] = { "encoding", "errors" };
    Value got[2];
    if (!meth_take(a, "decode", NAMES, 0, got))
        return R::Err;
    (void)s;
    return text_decode(self, got[0], got[1], out);
}

// ------------------------------------------------------------ the mutations

ArrayObj *self_array(const CallArgs &a, Str who)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!is_bytearray(s)) {
        Buf<96> b;
        b.put(who).put("() requires a bytearray");
        return err_set2("TypeError", b.str(), type_name(s)), nullptr;
    }
    return array_of(s);
}

R m_append(const CallArgs &a, Value &out)
{
    ArrayObj *b = self_array(a, "append");
    if (!b || !meth_args(a, "append", 1, 1))
        return R::Err;
    i64 n = 0;
    if (!as_index(a.args[1], n))
        return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
    if (n < 0 || n > 255)
        return err_set("ValueError", "byte must be in range(0, 256)");
    if (!b->data.push(u8(n)))
        return oom_err();
    out = value_none();
    return R::Ok;
}

R m_extend(const CallArgs &a, Value &out)
{
    ArrayObj *b = self_array(a, "extend");
    if (!b || !meth_args(a, "extend", 1, 1))
        return R::Err;
    if (iter_needs_vm(a.args[1]))
        return iter_park(a, 1, m_extend, out);
    Root rb{ method_self(a.args[0]) };
    Str more;
    if (bytes_like(a.args[1], more)) {
        // Copy first: the argument may be self, and the Vec will move.
        String owned;
        if (!owned.append(more))
            return oom_err();
        for (usize i = 0; i < owned.size(); i++)
            if (!array_of(rb.v)->data.push(u8(owned[i])))
                return oom_err();
        out = value_none();
        return R::Ok;
    }
    Root it{ py_iter(a.args[1]) };
    if (it.v.is_nil())
        return R::Err;
    for (;;) {
        Root got;
        R r = py_next(it.v, got.v);
        if (r == R::Err)
            return R::Err;
        if (r == R::NotImpl)
            break;
        i64 n = 0;
        if (!as_index(got.v, n) || n < 0 || n > 255)
            return err_set("ValueError", "byte must be in range(0, 256)");
        if (!array_of(rb.v)->data.push(u8(n)))
            return oom_err();
    }
    out = value_none();
    return R::Ok;
}

R m_insert(const CallArgs &a, Value &out)
{
    ArrayObj *b = self_array(a, "insert");
    if (!b || !meth_args(a, "insert", 2, 2))
        return R::Err;
    i64 at = 0, n = 0;
    if (!as_index(a.args[1], at) || !as_index(a.args[2], n))
        return err_set("TypeError", "an integer is required");
    if (n < 0 || n > 255)
        return err_set("ValueError", "byte must be in range(0, 256)");
    i64 len = i64(b->data.size());
    if (at < 0)
        at += len;
    at = at < 0 ? 0 : at > len ? len : at;
    if (!b->data.insert(usize(at), u8(n)))
        return oom_err();
    out = value_none();
    return R::Ok;
}

R m_pop(const CallArgs &a, Value &out)
{
    ArrayObj *b = self_array(a, "pop");
    if (!b || !meth_args(a, "pop", 0, 1))
        return R::Err;
    if (b->data.empty())
        return err_set("IndexError", "pop from empty bytearray");
    i64 at = i64(b->data.size()) - 1;
    if (a.nargs > 1 && !as_index(a.args[1], at))
        return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
    if (at < 0)
        at += i64(b->data.size());
    if (at < 0 || at >= i64(b->data.size()))
        return err_set("IndexError", "index out of range");
    out = Value::of_int(b->data[usize(at)]);
    b->data.erase(usize(at), 1);
    return R::Ok;
}

R m_remove(const CallArgs &a, Value &out)
{
    ArrayObj *b = self_array(a, "remove");
    if (!b || !meth_args(a, "remove", 1, 1))
        return R::Err;
    i64 n = 0;
    if (!as_index(a.args[1], n))
        return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
    for (usize i = 0; i < b->data.size(); i++)
        if (b->data[i] == u8(n)) {
            b->data.erase(i, 1);
            out = value_none();
            return R::Ok;
        }
    return err_set("ValueError", "value not found in bytearray");
}

R m_clear(const CallArgs &a, Value &out)
{
    ArrayObj *b = self_array(a, "clear");
    if (!b || !meth_args(a, "clear", 0, 0))
        return R::Err;
    b->data.clear();
    out = value_none();
    return R::Ok;
}

// take_bytes(n=None): the first n bytes out, as bytes; a negative n counts
// from the end. 3.15's, which the library already uses.
R m_take_bytes(const CallArgs &a, Value &out)
{
    ArrayObj *b = self_array(a, "take_bytes");
    if (!b || !meth_args(a, "take_bytes", 0, 1))
        return R::Err;
    i64 size = i64(b->data.size()), n = size;
    if (a.nargs > 1 && !is_none(a.args[1])) {
        if (!as_index(a.args[1], n))
            return err_set("TypeError", "n must be an integer or None");
        if (n < 0)
            n += size;
    }
    if (n < 0 || n > size) {
        char t1[24], t2[24];
        Buf<96> m;
        m.put("can't take ").put(int_text(t1, sizeof t1, n)).put(" bytes outside size ");
        m.put(int_text(t2, sizeof t2, size));
        return err_set("IndexError", m.str());
    }
    Root self{ obj_value(b) };
    out = bytes_new(Str(reinterpret_cast<const char *>(b->data.data()), usize(n)));
    if (out.is_nil())
        return R::Err;
    b = array_of(self.v);
    if (n)
        b->data.erase(0, usize(n));
    return R::Ok;
}

// resize(size): cut, or grow with zeros.
R m_resize(const CallArgs &a, Value &out)
{
    ArrayObj *b = self_array(a, "resize");
    if (!b || !meth_args(a, "resize", 1, 1))
        return R::Err;
    i64 n = 0;
    if (!as_index(a.args[1], n))
        return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
    if (n < 0) {
        char t[24];
        Buf<96> m;
        m.put("Can only resize to positive sizes, got ").put(int_text(t, sizeof t, n));
        return err_set("ValueError", m.str());
    }
    usize old = b->data.size();
    if (!b->data.resize(usize(n)))
        return oom_err();
    for (usize i = old; i < usize(n); i++)
        b->data[i] = 0;
    out = value_none();
    return R::Ok;
}

R m_reverse(const CallArgs &a, Value &out)
{
    ArrayObj *b = self_array(a, "reverse");
    if (!b || !meth_args(a, "reverse", 0, 0))
        return R::Err;
    for (usize i = 0; i < b->data.size() / 2; i++) {
        u8 t                            = b->data[i];
        b->data[i]                      = b->data[b->data.size() - 1 - i];
        b->data[b->data.size() - 1 - i] = t;
    }
    out = value_none();
    return R::Ok;
}

R m_copy(const CallArgs &a, Value &out)
{
    ArrayObj *b = self_array(a, "copy");
    if (!b || !meth_args(a, "copy", 0, 0))
        return R::Err;
    out = bytearray_new(b->str());
    return out.is_nil() ? R::Err : R::Ok;
}

// ------------------------------------------------------------- memoryview

// A window, not a copy. `owner` is the bytes, bytearray or array the octets
// live in. A slice of a view is another view on the same owner.
//
// `width` and `code` are what array gives a buffer: the octets of a
// `memoryview(array('i', ...))` are read four at a time and answer integers,
// where a view of bytes reads one at a time. `step` is in items and may be
// negative, which is what makes a strided slice a view rather than a copy.
struct MemObj : Obj {
    Value owner;
    u32 at;  // the first item's byte offset into the owner
    u32 len; // items, not octets
    i32 step;
    u8 width;
    char code;
};

MemObj *mem_of(Value v)
{
    return static_cast<MemObj *>(v.obj());
}

void mem_trace(Obj *o)
{
    gc_mark(static_cast<MemObj *>(o)->owner);
}

R mem_len(Value v, usize &out)
{
    out = mem_of(v)->len;
    return R::Ok;
}

R mem_repr(Value v, String &out)
{
    char tmp[24];
    Buf<64> b;
    b.put("<memory at ").put(addr_text(tmp, sizeof tmp, v.obj())).put('>');
    return out.append(b.str()) ? R::Ok : oom_err();
}

// Every octet the owner holds, whatever this view shows of them.
bool mem_owner_bytes(Value v, Str &out, bool *writable)
{
    MemObj *m = mem_of(v);
    char code = 'B';
    if (is_bytes(m->owner))
        out = static_cast<BytesObj *>(m->owner.obj())->str();
    else if (is_bytearray(m->owner))
        out = array_of(m->owner)->str();
    else if (!array_bytes(m->owner, out, code))
        return err_set("BufferError", "the underlying object is gone"), false;
    if (writable)
        *writable = !is_bytes(m->owner);
    return true;
}

// Where item `i` of this view starts, in the owner's octets.
usize mem_offset(const MemObj *m, usize i)
{
    return usize(i64(m->at) + i64(i) * i64(m->step) * i64(m->width));
}

R mem_item(Value v, usize i, Value &out)
{
    Str all;
    if (!mem_owner_bytes(v, all, nullptr))
        return R::Err;
    const ItemKind *k = item_kind(mem_of(v)->code);
    if (!k)
        return err_set("NotImplementedError", "this memoryview format");
    out = item_get(all, mem_offset(mem_of(v), i), k);
    return out.is_nil() ? R::Err : R::Ok;
}

R mem_getitem(Value v, Value key, Value &out)
{
    Root rv{ v };
    usize n = mem_of(rv.v)->len;
    if (is_slice(key)) {
        i64 start = 0, stop = 0, step = 1;
        usize count = 0;
        if (!slice_resolve(key, n, start, stop, step, count))
            return R::Err;
        MemObj *m = mem_of(rv.v);
        Value w   = memview_new(rv.v);
        if (w.is_nil())
            return R::Err;
        m                = mem_of(rv.v);
        mem_of(w)->at    = u32(mem_offset(m, usize(start)));
        mem_of(w)->len   = u32(count);
        mem_of(w)->step  = i32(step * m->step);
        mem_of(w)->width = m->width;
        mem_of(w)->code  = m->code;
        out              = w;
        return R::Ok;
    }
    usize i = 0;
    if (index_of(key, n, i) != R::Ok)
        return R::Err;
    return mem_item(rv.v, i, out);
}

R mem_setitem(Value v, Value key, Value item)
{
    Str all;
    bool writable = false;
    if (!mem_owner_bytes(v, all, &writable))
        return R::Err;
    if (!writable)
        return err_set("TypeError", "cannot modify read-only memory");
    if (is_slice(key)) {
        // A slice takes octets of its own shape, copied first in case they
        // are this memory's.
        i64 start = 0, stop = 0, step = 1;
        usize count = 0;
        if (!slice_resolve(key, mem_of(v)->len, start, stop, step, count))
            return R::Err;
        Str src;
        if (!bytes_like(item, src))
            return err_not("a bytes-like object is required", item, true);
        MemObj *m = mem_of(v);
        if (src.size() != count * m->width)
            return err_set("ValueError",
                           "memoryview assignment: lvalue and rvalue have different structures");
        String copy;
        if (!copy.assign(src))
            return oom_err();
        u8 *base = is_bytearray(m->owner) ? array_of(m->owner)->data.data() : array_data(m->owner);
        if (!base)
            return err_set("TypeError", "cannot modify read-only memory");
        for (usize i = 0; i < count; i++) {
            usize at = mem_offset(m, usize(start + i64(i) * step));
            for (usize j = 0; j < m->width; j++)
                base[at + j] = u8(copy[i * m->width + j]);
        }
        return R::Ok;
    }
    usize i = 0;
    if (index_of(key, mem_of(v)->len, i) != R::Ok)
        return R::Err;
    const ItemKind *k = item_kind(mem_of(v)->code);
    if (!k)
        return err_set("NotImplementedError", "this memoryview format");
    MemObj *m = mem_of(v);
    u8 *base  = is_bytearray(m->owner) ? array_of(m->owner)->data.data() : array_data(m->owner);
    if (!base)
        return err_set("TypeError", "cannot modify read-only memory");
    return item_put(reinterpret_cast<char *>(base), mem_offset(m, i), k, item);
}

// The octets this view shows, gathered. A contiguous view is a substring of
// the owner's; a strided one has to be copied out.
R mem_gather(Value v, String &out)
{
    Str all;
    if (!mem_owner_bytes(v, all, nullptr))
        return R::Err;
    MemObj *m = mem_of(v);
    for (usize i = 0; i < m->len; i++) {
        usize at = mem_offset(m, i);
        for (usize j = 0; j < m->width; j++)
            if (!out.push(all[at + j]))
                return oom_err();
    }
    return R::Ok;
}

R m_tobytes(const CallArgs &a, Value &out)
{
    Value self = a.nargs ? method_self(a.args[0]) : Value();
    if (!is_memview(self))
        return err_set2("TypeError", "tobytes() requires a memoryview", type_name(self));
    if (!meth_args(a, "tobytes", 0, 0))
        return R::Err;
    Root rs{ self };
    String text;
    if (mem_gather(rs.v, text) != R::Ok)
        return R::Err;
    out = bytes_new(text.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R m_tolist(const CallArgs &a, Value &out)
{
    Value self = a.nargs ? method_self(a.args[0]) : Value();
    if (!is_memview(self))
        return err_set2("TypeError", "tolist() requires a memoryview", type_name(self));
    if (!meth_args(a, "tolist", 0, 0))
        return R::Err;
    Root rs{ self };
    ListObj *l = list_new();
    if (!l)
        return oom_err();
    Root rl{ obj_value(l) };
    for (usize i = 0; i < mem_of(rs.v)->len; i++) {
        Value one;
        if (mem_item(rs.v, i, one) != R::Ok)
            return R::Err;
        if (!list_push(list_of(rl.v), one))
            return oom_err();
    }
    out = rl.v;
    return R::Ok;
}

// cast(fmt): the same octets, read at a different width. Only a contiguous
// view can be recast, which is what CPython says too.
R m_cast(const CallArgs &a, Value &out)
{
    Value self = a.nargs ? method_self(a.args[0]) : Value();
    if (!is_memview(self))
        return err_set2("TypeError", "cast() requires a memoryview", type_name(self));
    if (!meth_args(a, "cast", 1, 2))
        return R::Err;
    if (!is_str(a.args[1]) || str_of(a.args[1])->len != 1)
        return err_set("TypeError", "cast() wants a single-character format");
    const ItemKind *k = item_kind(str_of(a.args[1])->bytes()[0]);
    if (!k)
        return err_set("ValueError", "cast() wants a native single-character format");
    Root rs{ self };
    if (mem_of(rs.v)->step != 1)
        return err_set("TypeError", "cast() wants a contiguous buffer");
    usize octets = usize(mem_of(rs.v)->len) * mem_of(rs.v)->width;
    if (octets % k->width)
        return err_set("TypeError", "the buffer length is not a multiple of the item size");
    Value w = memview_new(rs.v);
    if (w.is_nil())
        return R::Err;
    mem_of(w)->at    = mem_of(rs.v)->at;
    mem_of(w)->len   = u32(octets / k->width);
    mem_of(w)->step  = 1;
    mem_of(w)->width = k->width;
    mem_of(w)->code  = k->code;
    out              = w;
    return R::Ok;
}

R m_release(const CallArgs &a, Value &out)
{
    // Nothing is pinned here: the collector owns the buffer, so releasing a
    // view is only a promise not to use it again.
    if (!meth_args(a, "release", 0, 0))
        return R::Err;
    out = value_none();
    return R::Ok;
}

// A one-item tuple, which is what shape and strides are for a flat view.
Value one_tuple(i64 n)
{
    TupleObj *t = tuple_new(1);
    if (!t)
        return oom_err(), Value();
    Root rt{ obj_value(t) };
    Value v = int_from_i64(n);
    if (v.is_nil())
        return Value();
    static_cast<TupleObj *>(rt.v.obj())->items()[0] = v;
    return rt.v;
}

R mem_getattr(Value v, StrObj *name, Value &out)
{
    MemObj *m = mem_of(v);
    Str n     = name->str();
    if (n == "itemsize")
        out = Value::of_int(m->width);
    else if (n == "format")
        out = str_new(Str(&m->code, 1));
    else if (n == "nbytes")
        out = int_from_i64(i64(m->len) * m->width);
    else if (n == "ndim")
        out = Value::of_int(1);
    else if (n == "shape")
        out = one_tuple(i64(m->len));
    else if (n == "strides")
        out = one_tuple(i64(m->step) * m->width);
    else if (n == "readonly")
        out = value_bool(is_bytes(m->owner));
    else if (n == "obj")
        out = m->owner;
    else if (n == "c_contiguous" || n == "f_contiguous" || n == "contiguous")
        out = value_bool(m->step == 1);
    else if (n == "suboffsets")
        out = obj_value(tuple_new(0));
    else
        return R::NotImpl;
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method MEMVIEW[] = {
    { "tobytes", m_tobytes }, { "tolist", m_tolist },   { "hex", m_hex },
    { "cast", m_cast },       { "release", m_release },
};

// A view compares and searches as the octets it shows.
R mem_eq(Value a, Value b, bool &out)
{
    Str x, y;
    if (!bytes_like(a, x) || !bytes_like(b, y))
        return R::NotImpl;
    out = x == y;
    return R::Ok;
}

R mem_contains(Value v, Value item, bool &out)
{
    Str s, sub;
    if (!bytes_like(v, s))
        return R::Err;
    i64 n = 0;
    if (as_index(item, n)) {
        out = n >= 0 && n <= 255 && s.find(char(n)) != Str::npos;
        return R::Ok;
    }
    if (!bytes_like(item, sub))
        return err_set2("TypeError", "a bytes-like object is required", type_name(item));
    out = s.find(sub) != Str::npos;
    return R::Ok;
}

// ----------------------------------------------------------- the two tables

// Read-only, so both types get these.
constexpr Method COMMON[] = {
    { "capitalize", m_capitalize },
    { "center", m_center },
    { "count", m_count },
    { "decode", m_decode },
    { "endswith", m_endswith },
    { "expandtabs", m_expandtabs },
    { "find", m_find },
    { "hex", m_hex },
    { "index", m_index },
    { "isalnum", m_isalnum },
    { "isalpha", m_isalpha },
    { "isascii", m_isascii },
    { "isdigit", m_isdigit },
    { "islower", m_islower },
    { "isspace", m_isspace },
    { "istitle", m_istitle },
    { "isupper", m_isupper },
    { "join", m_join },
    { "ljust", m_ljust },
    { "lower", m_lower },
    { "lstrip", m_lstrip },
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
    { "maketrans", m_maketrans, true },
    { "upper", m_upper },
    { "zfill", m_zfill },
};

constexpr Method BYTES_ONLY[] = { { "fromhex", m_fromhex_bytes, true } };

constexpr Method ARRAY_ONLY[] = {
    { "fromhex", m_fromhex_array, true },
    { "append", m_append },
    { "extend", m_extend },
    { "insert", m_insert },
    { "pop", m_pop },
    { "remove", m_remove },
    { "clear", m_clear },
    { "reverse", m_reverse },
    { "copy", m_copy },
    { "take_bytes", m_take_bytes },
    { "resize", m_resize },
};

} // namespace

constexpr Type memview_type{ .name     = "memoryview",
                             .trace    = mem_trace,
                             .eq       = mem_eq,
                             .repr     = mem_repr,
                             .len      = mem_len,
                             .getitem  = mem_getitem,
                             .setitem  = mem_setitem,
                             .contains = mem_contains,
                             .iter     = seq_iter,
                             .getattr  = mem_getattr,
                             .patma    = PATMA_SEQ };

Value memview_new(Value owner)
{
    Str s;
    char code = 'B';
    u8 width  = 1;
    if (is_memview(owner)) {
        // A view of a view watches the same owner, at the same width.
        MemObj *src = static_cast<MemObj *>(owner.obj());
        code        = src->code;
        width       = src->width;
    } else if (array_bytes(owner, s, code)) {
        const ItemKind *k = item_kind(code);
        width             = k ? k->width : 1;
    } else if (!bytes_like(owner, s)) {
        return err_set2("TypeError", "memoryview: a bytes-like object is required",
                        type_name(owner)),
               Value();
    }
    Root ro{ is_memview(owner) ? static_cast<MemObj *>(owner.obj())->owner : owner };
    u32 base  = is_memview(owner) ? static_cast<MemObj *>(owner.obj())->at : 0;
    u32 len   = is_memview(owner) ? static_cast<MemObj *>(owner.obj())->len : u32(s.size() / width);
    MemObj *m = static_cast<MemObj *>(obj_alloc(&memview_type, sizeof(MemObj)));
    if (!m)
        return oom_err(), Value();
    m->owner = ro.v;
    m->at    = base;
    m->len   = len;
    m->step  = 1;
    m->width = width;
    m->code  = code;
    return obj_value(m);
}

// The octets this view shows, as a span of the owner's. A strided view is not
// one span, so it is not bytes-like; tobytes() gathers it instead.
bool memview_bytes(Value v, Str &out, bool *writable)
{
    MemObj *m = static_cast<MemObj *>(v.obj());
    Str all;
    if (!mem_owner_bytes(v, all, writable))
        return false;
    if (m->step != 1)
        return err_set("BufferError", "memoryview: underlying buffer is not C-contiguous"), false;
    usize at = m->at < all.size() ? m->at : all.size();
    usize n  = usize(m->len) * m->width;
    if (n > all.size() - at)
        n = all.size() - at;
    out = all.substr(at, n);
    return true;
}

bool bytes_methods()
{
    return method_install(&bytes_type, COMMON) && method_install(&bytes_type, BYTES_ONLY) &&
           method_install(&bytearray_type, COMMON) && method_install(&bytearray_type, ARRAY_ONLY) &&
           method_install(&memview_type, MEMVIEW);
}
