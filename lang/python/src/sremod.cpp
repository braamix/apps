// `_sre`: the objects over sre.cpp's engine, as Modules/_sre/sre.c has them --
// Pattern, Match, Scanner and Template, and the dozen names re/ imports.
//
// A str is UTF-8 here, and the engine wants a character at a time. A str that
// is all ASCII is its own octets; any other is decoded once into codepoints,
// with the byte offset of each beside it so a slice costs nothing to find.
//
// Everything that matches may take long, so everything that matches is a job
// kept in a StateObj: the call runs it, and when the engine says its slice is
// spent, the call answers a continuation that parks and runs it on. `sub`
// with a function parks the same way, on a call instead of a sleep.
#include "bigint.h"
#include "binfmt.h"
#include "builtin.h"
#include "exc.h"
#include "gc.h"
#include "genalias.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "sre.h"
#include "type.h"
#include "ustr.h"
#include "vm.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// Dispatches between two looks at the clock. A slice is a few milliseconds.
constexpr u32 SLICE = 1u << 18;

constexpr Str COPYRIGHT = " SRE 2.2.2 Copyright (c) 1997-2002 by Secret Labs AB ";

extern const Type pattern_type;
extern const Type match_type;
extern const Type scanner_type;
extern const Type template_type;
extern const Type text_type;
extern const Type state_type;
extern const Type iter_type;

// ------------------------------------------------------------------- text

// A str with something past ASCII in it, as codepoints.
struct TextObj : Obj {
    Value str;     // the StrObj it came from
    Vec<u32> cps;  // one per character
    Vec<u32> offs; // the byte offset of each, and the length at the end
};

void text_trace(Obj *o)
{
    gc_mark(static_cast<TextObj *>(o)->str);
}

void text_fini(Obj *o)
{
    TextObj *t = static_cast<TextObj *>(o);
    t->cps.~Vec();
    t->offs.~Vec();
}

constexpr Type text_type{ .name = "_sre.text", .trace = text_trace, .fini = text_fini };

// The last text decoded, and the function sub() hands a template to.
struct Home {
    Value text;
    Value compile_template;
};

Home *home;

void home_mark()
{
    if (home) {
        gc_mark(home->text);
        gc_mark(home->compile_template);
    }
}

bool home_up()
{
    if (home)
        return true;
    home = heap_new<Home>();
    if (!home)
        return oom(), false;
    gc_root_hook(home_mark);
    return true;
}

TextObj *text_of(Value v)
{
    return static_cast<TextObj *>(v.obj());
}

// The codepoints of `s`. One is kept, so a loop of p.match(s, pos) over one
// string decodes it once.
Value text_new(Value s)
{
    if (!home_up())
        return Value();
    if (!home->text.is_nil() && text_of(home->text)->str == s)
        return home->text;
    Root rs{ s };
    gc_reserve(usize(str_of(s)->chars) * 8);
    TextObj *t = static_cast<TextObj *>(obj_alloc(&text_type, sizeof(TextObj)));
    if (!t)
        return oom(), Value();
    new (&t->cps) Vec<u32>();
    new (&t->offs) Vec<u32>();
    t->str = rs.v;
    Root rt{ obj_value(t) };
    StrObj *so = str_of(rs.v);
    Str all    = so->str();
    if (!t->cps.reserve(so->chars) || !t->offs.reserve(usize(so->chars) + 1))
        return oom(), Value();
    usize at = 0;
    u32 cp   = 0;
    while (at < all.size()) {
        usize w = cp_decode(all, at, cp);
        if (!t->cps.push(cp) || !t->offs.push(u32(at)))
            return oom(), Value();
        at += w;
    }
    if (!t->offs.push(u32(all.size())))
        return oom(), Value();
    home->text = rt.v;
    return rt.v;
}

// What the engine reads for one target, and how to cut a piece out of it.
struct Target {
    Value text; // StrObj, BytesObj or TextObj
    const void *data;
    isize length;
    bool isbytes;
    int charsize;
};

bool is_text(Value v)
{
    return v.is_obj() && v.obj()->type == &text_type;
}

// getstring(): a str or anything with octets, else TypeError. A bytearray or a
// memoryview is copied, since a callback could resize it under the engine.
bool target_of(Value string, Target &t)
{
    Value v = method_self(string);
    if (is_str(v)) {
        StrObj *s = str_of(v);
        t.isbytes = false;
        t.length  = isize(s->chars);
        if (s->flags & OBJ_ASCII) {
            t.text     = v;
            t.data     = s->bytes();
            t.charsize = 1;
            return true;
        }
        t.text = text_new(v);
        if (t.text.is_nil())
            return false;
        t.data     = text_of(t.text)->cps.data();
        t.charsize = 4;
        return true;
    }
    Str octets;
    char code = 'B';
    if (!bytes_like(v, octets) && !array_bytes(v, octets, code)) {
        Buf<160> m;
        m.put("expected string or bytes-like object, got '").put(type_name(string)).put('\'');
        return err_set("TypeError", m.str()), false;
    }
    t.text = is_bytes(v) ? v : bytes_new(octets);
    if (t.text.is_nil())
        return false;
    t.data     = static_cast<BytesObj *>(t.text.obj())->data();
    t.length   = isize(octets.size());
    t.isbytes  = true;
    t.charsize = 1;
    return true;
}

// getslice(): characters [i, j) of the text, as the target's kind. The whole
// of an exact str or bytes is the object itself.
Value slice_of(Value string, Value text, bool isbytes, isize i, isize j)
{
    if (isbytes) {
        BytesObj *b = static_cast<BytesObj *>(text.obj());
        if (is_bytes(string) && i == 0 && j == isize(b->len))
            return string;
        return bytes_new(Str(reinterpret_cast<const char *>(b->data()) + i, usize(j - i)));
    }
    if (is_text(text)) {
        TextObj *t = text_of(text);
        StrObj *s  = str_of(t->str);
        if (is_str(string) && i == 0 && j == isize(s->chars))
            return string;
        u32 b = t->offs[usize(i)], e = t->offs[usize(j)];
        return str_of_bytes(Str(s->bytes() + b, e - b));
    }
    StrObj *s = str_of(text);
    if (is_str(string) && i == 0 && j == isize(s->len))
        return string;
    return str_of_bytes(Str(s->bytes() + i, usize(j - i)));
}

// ---------------------------------------------------------------- pattern

struct PatternObj : Obj {
    Value pattern;    // str, bytes or None
    Value groupindex; // DictObj, or Nil
    Value indexgroup; // TupleObj, or Nil
    isize groups;
    i32 flags;
    i32 isbytes; // 1 bytes, 0 str, -1 None
    u32 codesize;

    SreCode *code() { return reinterpret_cast<SreCode *>(this + 1); }

    const SreCode *code() const { return reinterpret_cast<const SreCode *>(this + 1); }
};

PatternObj *pat_of(Value v)
{
    return static_cast<PatternObj *>(v.obj());
}

bool is_pattern(Value v)
{
    return v.is_obj() && v.obj()->type == &pattern_type;
}

void pattern_trace(Obj *o)
{
    PatternObj *p = static_cast<PatternObj *>(o);
    gc_mark(p->pattern);
    gc_mark(p->groupindex);
    gc_mark(p->indexgroup);
}

// %.<n>R: a repr cut to `n` characters.
bool put_repr_cut(String &out, Value v, usize n)
{
    String r;
    if (py_repr(v, r) != R::Ok)
        return false;
    Str s    = r.str();
    usize at = 0;
    for (usize k = 0; k < n && at < s.size(); k++)
        at += cp_width(u8(s[at]));
    if (at > s.size())
        at = s.size();
    return out.append(s.substr(0, at)) ? true : (oom(), false);
}

R pattern_repr(Value v, String &out)
{
    struct Flag {
        Str name;
        i32 value;
    };
    constexpr Flag FLAG_NAMES[] = {
        { "re.IGNORECASE", SRE_FLAG_IGNORECASE }, { "re.LOCALE", SRE_FLAG_LOCALE },
        { "re.MULTILINE", SRE_FLAG_MULTILINE },   { "re.DOTALL", SRE_FLAG_DOTALL },
        { "re.UNICODE", SRE_FLAG_UNICODE },       { "re.VERBOSE", SRE_FLAG_VERBOSE },
        { "re.DEBUG", SRE_FLAG_DEBUG },           { "re.ASCII", SRE_FLAG_ASCII },
    };
    PatternObj *p = pat_of(v);
    i32 flags     = p->flags;
    // Omit re.UNICODE for valid string patterns.
    if (p->isbytes == 0 &&
        (flags & (SRE_FLAG_LOCALE | SRE_FLAG_UNICODE | SRE_FLAG_ASCII)) == SRE_FLAG_UNICODE)
        flags &= ~SRE_FLAG_UNICODE;
    if (!out.append("re.compile(") || !put_repr_cut(out, p->pattern, 200))
        return err_pending() ? R::Err : oom();
    bool first = true;
    for (const Flag &f : FLAG_NAMES) {
        if (!(flags & f.value))
            continue;
        if (!out.append(first ? Str(", ") : Str("|")) || !out.append(f.name))
            return oom();
        first = false;
        flags &= ~f.value;
    }
    if (flags) {
        Buf<24> b;
        b.put("0x");
        put_hexw(b, u32(flags), 1);
        if (!out.append(first ? Str(", ") : Str("|")) || !out.append(b.str()))
            return oom();
    }
    return out.push(')') ? R::Ok : oom();
}

R pattern_hash(Value v, u32 &out)
{
    PatternObj *p = pat_of(v);
    u32 h         = 0;
    if (py_hash(p->pattern, h) != R::Ok)
        return R::Err;
    u32 hc = 2166136261u;
    for (u32 i = 0; i < p->codesize; i++) {
        hc ^= p->code()[i];
        hc *= 16777619u;
    }
    h ^= hc;
    h ^= u32(p->flags);
    h ^= u32(p->isbytes);
    h ^= p->codesize;
    out = h;
    return R::Ok;
}

R pattern_eq(Value a, Value b, bool &out)
{
    if (!is_pattern(a) || !is_pattern(b))
        return R::NotImpl;
    if (a == b) {
        out = true;
        return R::Ok;
    }
    PatternObj *l = pat_of(a), *r = pat_of(b);
    bool same = l->flags == r->flags && l->isbytes == r->isbytes && l->codesize == r->codesize;
    for (u32 i = 0; same && i < l->codesize; i++)
        same = l->code()[i] == r->code()[i];
    if (same && py_eq(l->pattern, r->pattern, same) != R::Ok)
        return R::Err;
    out = same;
    return R::Ok;
}

R pattern_getattr(Value v, StrObj *name, Value &out)
{
    PatternObj *p = pat_of(v);
    Str n         = name->str();
    if (n == "pattern") {
        out = p->pattern;
    } else if (n == "flags") {
        out = Value::of_int(p->flags);
    } else if (n == "groups") {
        out = int_from_i64(p->groups);
    } else if (n == "groupindex") {
        // A proxy, so the dict the pattern keeps cannot be changed through it.
        if (p->groupindex.is_nil()) {
            DictObj *d = dict_new();
            if (!d)
                return oom();
            out = obj_value(d);
        } else {
            out = mappingproxy_new(p->groupindex);
        }
    } else {
        return R::NotImpl;
    }
    return out.is_nil() ? R::Err : R::Ok;
}

void pattern_error(isize status)
{
    if (status == SRE_ERROR_MEMORY)
        oom();
    else
        err_set("RuntimeError", "internal error in regular expression engine");
}

// ------------------------------------------------------------------ match

struct MatchObj : Obj {
    Value string;  // the target as given
    Value text;    // what was matched: StrObj, BytesObj or TextObj
    Value pattern; // PatternObj
    Value regs;    // the tuple `regs` made, or Nil
    isize pos, endpos;
    isize lastindex;
    isize groups; // the pattern's, plus one
    bool isbytes;

    isize *mark() { return reinterpret_cast<isize *>(this + 1); }
};

MatchObj *match_of(Value v)
{
    return static_cast<MatchObj *>(v.obj());
}

bool is_match(Value v)
{
    return v.is_obj() && v.obj()->type == &match_type;
}

void match_trace(Obj *o)
{
    MatchObj *m = static_cast<MatchObj *>(o);
    gc_mark(m->string);
    gc_mark(m->text);
    gc_mark(m->pattern);
    gc_mark(m->regs);
}

isize text_length(Value text, bool isbytes)
{
    if (isbytes)
        return isize(static_cast<BytesObj *>(text.obj())->len);
    if (is_text(text))
        return isize(text_of(text)->cps.size());
    return isize(str_of(text)->len);
}

// The octets a match over a buffer reads now: the buffer may have changed
// since, and a group is cut out of what it holds today.
bool live_octets(Value string, Str &out)
{
    Value v   = method_self(string);
    char code = 'B';
    return bytes_like(v, out) || array_bytes(v, out, code);
}

Value match_getslice_by_index(MatchObj *m, isize index, Value def)
{
    index *= 2;
    if (m->mark()[index] < 0)
        return def;
    if (m->isbytes && !is_bytes(method_self(m->string))) {
        Str now;
        if (!live_octets(m->string, now))
            return err_set("TypeError", "the matched buffer is gone"), Value();
        isize n = isize(now.size());
        isize i = m->mark()[index] < n ? m->mark()[index] : n;
        isize j = m->mark()[index + 1] < n ? m->mark()[index + 1] : n;
        return bytes_new(now.substr(usize(i), usize(j - i)));
    }
    isize length = text_length(m->text, m->isbytes);
    isize i      = m->mark()[index];
    isize j      = m->mark()[index + 1];
    i            = i < length ? i : length;
    j            = j < length ? j : length;
    return slice_of(m->string, m->text, m->isbytes, i, j);
}

// A group by number or by name; IndexError when there is none.
bool match_getindex(MatchObj *m, Value index, isize &out)
{
    isize i = -1;
    if (index.is_nil()) {
        out = 0;
        return true;
    }
    if (is_intval(index)) {
        i64 n = 0;
        if (!as_index(index, n))
            n = -1;
        i = n < 0 || n > 0x7fffffff ? -1 : isize(n);
    } else if (!pat_of(m->pattern)->groupindex.is_nil()) {
        Value got;
        R r = dict_get(static_cast<DictObj *>(pat_of(m->pattern)->groupindex.obj()), index, got);
        if (r == R::Err)
            return false; // an unhashable name, as CPython reports it
        if (r == R::Ok && is_intval(got)) {
            i64 n = 0;
            if (as_index(got, n) && n >= 0 && n <= 0x7fffffff)
                i = isize(n);
        }
    }
    if (i < 0 || i >= m->groups)
        return err_set("IndexError", "no such group"), false;
    out = i;
    return true;
}

Value match_getslice(MatchObj *m, Value index, Value def)
{
    isize i = 0;
    if (!match_getindex(m, index, i))
        return Value();
    return match_getslice_by_index(m, i, def);
}

Value pair(isize a, isize b)
{
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom(), Value();
    t->items()[0] = Value::of_int(i32(a));
    t->items()[1] = Value::of_int(i32(b));
    return obj_value(t);
}

R match_repr(Value v, String &out)
{
    MatchObj *m = match_of(v);
    Root g0{ match_getslice_by_index(m, 0, value_none()) };
    if (g0.v.is_nil())
        return R::Err;
    char n[24];
    if (!out.append("<re.Match object; span=(") ||
        !out.append(int_text(n, sizeof n, i64(m->mark()[0]))) || !out.append(", ") ||
        !out.append(int_text(n, sizeof n, i64(m->mark()[1]))) || !out.append("), match="))
        return oom();
    if (!put_repr_cut(out, g0.v, 50))
        return R::Err;
    return out.push('>') ? R::Ok : oom();
}

R match_getitem(Value v, Value key, Value &out)
{
    out = match_getslice(match_of(v), key, value_none());
    return out.is_nil() ? R::Err : R::Ok;
}

Value match_regs(MatchObj *m)
{
    if (!m->regs.is_nil())
        return m->regs;
    TupleObj *t = tuple_new(usize(m->groups));
    if (!t)
        return oom(), Value();
    Root rt{ obj_value(t) };
    for (isize i = 0; i < m->groups; i++) {
        Value p = pair(m->mark()[i * 2], m->mark()[i * 2 + 1]);
        if (p.is_nil())
            return Value();
        static_cast<TupleObj *>(rt.v.obj())->items()[i] = p;
    }
    m->regs = rt.v;
    return rt.v;
}

R match_getattr(Value v, StrObj *name, Value &out)
{
    MatchObj *m = match_of(v);
    Str n       = name->str();
    if (n == "string") {
        out = m->string;
    } else if (n == "re") {
        out = m->pattern;
    } else if (n == "pos") {
        out = int_from_i64(m->pos);
    } else if (n == "endpos") {
        out = int_from_i64(m->endpos);
    } else if (n == "lastindex") {
        out = m->lastindex >= 0 ? int_from_i64(m->lastindex) : value_none();
    } else if (n == "lastgroup") {
        PatternObj *p = pat_of(m->pattern);
        out           = value_none();
        if (!p->indexgroup.is_nil() && m->lastindex >= 0 &&
            m->lastindex < isize(static_cast<TupleObj *>(p->indexgroup.obj())->len))
            out = static_cast<TupleObj *>(p->indexgroup.obj())->items()[m->lastindex];
    } else if (n == "regs") {
        out = match_regs(m);
    } else {
        return R::NotImpl;
    }
    return out.is_nil() ? R::Err : R::Ok;
}

// ---------------------------------------------------------------- template

struct TemplateItem {
    isize index;
    Value literal; // Nil for an empty one
};

struct TemplateObj : Obj {
    Value literal;
    isize n;      // group references
    isize chunks; // group references and non-empty literals, plus one

    TemplateItem *items() { return reinterpret_cast<TemplateItem *>(this + 1); }
};

TemplateObj *tpl_of(Value v)
{
    return static_cast<TemplateObj *>(v.obj());
}

bool is_template(Value v)
{
    return v.is_obj() && v.obj()->type == &template_type;
}

void template_trace(Obj *o)
{
    TemplateObj *t = static_cast<TemplateObj *>(o);
    gc_mark(t->literal);
    for (isize i = 0; i < t->n; i++)
        gc_mark(t->items()[i].literal);
}

// One piece of a result, joined onto `acc`. The literal is always the right
// kind; a group is the match's.
bool put_piece(String &acc, Value v)
{
    Str s;
    Value x = method_self(v);
    if (is_str(x))
        s = str_of(x)->str();
    else if (!bytes_like(x, s))
        return err_set2("TypeError", "expected str or bytes", type_name(v)), false;
    return acc.append(s) ? true : (oom(), false);
}

Value expand_template(TemplateObj *self, MatchObj *m)
{
    if (self->n == 0)
        return self->literal;
    Root rs{ obj_value(self) }, rm{ obj_value(m) };
    String acc;
    Root only;
    usize count = 0;
    auto take   = [&](Value v) {
        if (count++ == 0)
            only = v;
        return put_piece(acc, v);
    };
    if (!take(self->literal))
        return Value();
    for (isize i = 0; i < tpl_of(rs.v)->n; i++) {
        TemplateItem it = tpl_of(rs.v)->items()[i];
        if (it.index >= match_of(rm.v)->groups)
            return err_set("IndexError", "no such group"), Value();
        Root item{ match_getslice_by_index(match_of(rm.v), it.index, value_none()) };
        if (item.v.is_nil())
            return Value();
        if (!is_none(item.v) && !take(item.v))
            return Value();
        if (!it.literal.is_nil() && !take(it.literal))
            return Value();
    }
    bool str = is_str(method_self(tpl_of(rs.v)->literal));
    // One piece of the exact kind is the result as it is.
    if (count == 1 && (str ? is_str(only.v) : is_bytes(only.v)))
        return only.v;
    return str ? str_of_bytes(acc.str()) : bytes_new(acc.str());
}

// ------------------------------------------------------------------ state

enum : u8 {
    JOB_MATCH,
    JOB_FULLMATCH,
    JOB_SEARCH,
    JOB_FINDALL,
    JOB_SPLIT,
    JOB_SUB,
    JOB_SCAN_MATCH,
    JOB_SCAN_SEARCH,
    JOB_ITER, // a scanner's search, where None is the end of an iteration
};

enum : u8 { FILTER_LITERAL, FILTER_TEMPLATE, FILTER_CALLABLE };

// SRE_STATE with the Python half, and the loop of whatever job it is doing.
struct StateObj : Obj {
    SreState st;
    Value string;  // the target as given
    Value text;    // what the engine reads
    Value pattern; // PatternObj
    Value list;    // findall's and split's result
    Value filter;  // sub's replacement
    Value match;   // the match sub() is waiting on the filter for
    Value first;   // sub's only piece, while there is only one
    Value bad;     // the first piece of the wrong kind, reported at the end
    String acc;    // sub's result, as it is built
    isize n, count, last;
    isize b, e; // sub: the match waiting on the filter
    u32 items;  // pieces in the result so far
    u32 bad_at;
    u8 kind;
    u8 filter_type;
    bool isbytes;
    bool subn;
    bool waiting;   // sub: the filter's answer is due
    bool executing; // a scanner in the middle of a call
    bool exhausted; // a scanner with nothing left: state->start was NULL
};

StateObj *state_of(Value v)
{
    return static_cast<StateObj *>(v.obj());
}

void state_trace(Obj *o)
{
    StateObj *s = static_cast<StateObj *>(o);
    gc_mark(s->string);
    gc_mark(s->text);
    gc_mark(s->pattern);
    gc_mark(s->list);
    gc_mark(s->filter);
    gc_mark(s->match);
    gc_mark(s->first);
    gc_mark(s->bad);
}

void state_fini(Obj *o)
{
    StateObj *s = static_cast<StateObj *>(o);
    sre_state_fini(&s->st);
    s->acc.~String();
}

constexpr Type state_type{ .name = "_sre.state", .trace = state_trace, .fini = state_fini };

// state_init(): the target, clamped bounds, and marks for the groups.
Value state_new(Value pattern, Value string, isize start, isize end)
{
    Root rp{ pattern }, rs{ string };
    StateObj *so = static_cast<StateObj *>(obj_alloc(&state_type, sizeof(StateObj)));
    if (!so)
        return oom(), Value();
    sre_state_clear(&so->st);
    new (&so->acc) String();
    so->string = rs.v;
    so->text = so->pattern = so->list = so->filter = so->match = so->first = so->bad = Value();
    so->n = so->count = so->last = so->b = so->e = 0;
    so->items = so->bad_at = 0;
    so->kind = so->filter_type = 0;
    so->isbytes = so->subn = so->waiting = so->executing = so->exhausted = false;
    Root ro{ obj_value(so) };
    so->pattern   = rp.v;
    PatternObj *p = pat_of(rp.v);
    if (p->groups) {
        so->st.mark = static_cast<const void **>(heap_alloc(usize(p->groups) * 2 * sizeof(void *)));
        if (!so->st.mark)
            return oom(), Value();
    }

    Target t;
    if (!target_of(rs.v, t))
        return Value();
    so = state_of(ro.v);
    p  = pat_of(rp.v);
    if (t.isbytes && p->isbytes == 0)
        return err_set("TypeError", "cannot use a string pattern on a bytes-like object"), Value();
    if (!t.isbytes && p->isbytes > 0)
        return err_set("TypeError", "cannot use a bytes pattern on a string-like object"), Value();

    // adjust boundaries
    if (start < 0)
        start = 0;
    else if (start > t.length)
        start = t.length;
    if (end < 0)
        end = 0;
    else if (end > t.length)
        end = t.length;

    so->text         = t.text;
    so->isbytes      = t.isbytes;
    so->st.charsize  = t.charsize;
    so->st.beginning = t.data;
    so->st.start     = static_cast<const char *>(t.data) + start * t.charsize;
    so->st.end       = static_cast<const char *>(t.data) + end * t.charsize;
    so->st.ptr       = so->st.start;
    so->st.pos       = start;
    so->st.endpos    = end;
    so->st.slice     = SLICE;
    return ro.v;
}

isize offset_of(const SreState *s, const void *p)
{
    return (static_cast<const char *>(p) - static_cast<const char *>(s->beginning)) / s->charsize;
}

Value state_getslice(StateObj *so, isize index, bool empty)
{
    const SreState *s = &so->st;
    isize i, j;
    index = (index - 1) * 2;
    if (index >= s->lastmark || !s->mark[index] || !s->mark[index + 1]) {
        if (!empty)
            return value_none();
        i = j = 0;
    } else {
        i = offset_of(s, s->mark[index]);
        j = offset_of(s, s->mark[index + 1]);
        if (i > j)
            return err_set("SystemError",
                           "The span of capturing group is wrong, please report "
                           "a bug for the re module."),
                   Value();
    }
    return slice_of(so->string, so->text, so->isbytes, i, j);
}

// pattern_new_match(): the Match of a state that matched.
Value match_new(StateObj *so)
{
    Root ro{ obj_value(so) };
    PatternObj *p = pat_of(so->pattern);
    isize groups  = p->groups + 1;
    MatchObj *m   = static_cast<MatchObj *>(
        obj_alloc(&match_type, sizeof(MatchObj) + usize(groups) * 2 * sizeof(isize)));
    if (!m)
        return oom(), Value();
    so                = state_of(ro.v);
    p                 = pat_of(so->pattern);
    const SreState *s = &so->st;
    m->string         = so->string;
    m->text           = so->text;
    m->pattern        = so->pattern;
    m->regs           = Value();
    m->groups         = groups;
    m->isbytes        = so->isbytes;
    m->mark()[0]      = offset_of(s, s->start);
    m->mark()[1]      = offset_of(s, s->ptr);
    for (isize i = 0, j = 0; i < p->groups; i++, j += 2) {
        if (j + 1 <= s->lastmark && s->mark[j] && s->mark[j + 1]) {
            m->mark()[j + 2] = offset_of(s, s->mark[j]);
            m->mark()[j + 3] = offset_of(s, s->mark[j + 1]);
            if (m->mark()[j + 2] > m->mark()[j + 3])
                return err_set("SystemError",
                               "The span of capturing group is wrong, please "
                               "report a bug for the re module."),
                       Value();
        } else {
            m->mark()[j + 2] = m->mark()[j + 3] = -1; // undefined
        }
    }
    m->pos       = s->pos;
    m->endpos    = s->endpos;
    m->lastindex = s->lastindex;
    return obj_value(m);
}

// ------------------------------------------------------------------- jobs

enum class Job : u8 { Done, Suspend, Call, Err };

// One search or match over the state's window, from where it stopped.
isize engine(StateObj *so, bool search)
{
    if (!so->st.suspended) {
        sre_state_reset(&so->st);
        so->st.ptr = so->st.start;
    }
    const SreCode *code = pat_of(so->pattern)->code();
    return search ? sre_search(&so->st, code) : sre_match(&so->st, code);
}

// A piece of sub()'s result, checked as the join at the end would check it.
bool sub_put(StateObj *so, Value v)
{
    Value x = method_self(v);
    Str s;
    bool fits = so->isbytes ? bytes_like(x, s) : is_str(x);
    if (!fits) {
        if (so->bad.is_nil()) {
            so->bad    = v;
            so->bad_at = so->items;
        }
        so->items++;
        return true;
    }
    if (!so->isbytes)
        s = str_of(x)->str();
    so->first = so->items == 0 ? v : Value();
    so->items++;
    return so->acc.append(s) ? true : (oom(), false);
}

// The sub() result: the pieces joined, and the count beside them for subn().
Value sub_result(StateObj *so)
{
    Root ro{ obj_value(so) };
    if (!so->bad.is_nil()) {
        char n[24];
        Buf<200> m;
        m.put("sequence item ").put(int_text(n, sizeof n, i64(so->bad_at)));
        m.put(so->isbytes ? ": expected a bytes-like object, " : ": expected str instance, ");
        m.put(type_name(so->bad)).put(" found");
        return err_set("TypeError", m.str()), Value();
    }
    Root item;
    if (so->items == 0)
        item = slice_of(so->string, so->text, so->isbytes, 0, 0);
    else if (so->items == 1 && (so->isbytes ? is_bytes(so->first) : is_str(so->first)))
        item = so->first;
    else
        item = so->isbytes ? bytes_new(so->acc.str()) : str_of_bytes(so->acc.str());
    if (item.v.is_nil())
        return Value();
    so = state_of(ro.v);
    if (!so->subn)
        return item.v;
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom(), Value();
    t->items()[0] = item.v;
    t->items()[1] = int_from_i64(state_of(ro.v)->n);
    return obj_value(t);
}

// Run the job until it is done, the slice is spent, or sub() needs its filter
// called; `out` is the answer once done.
Job job_run(Value sv, Value &out)
{
    Root rs{ sv };
    StateObj *so = state_of(sv);
    switch (so->kind) {
    case JOB_MATCH:
    case JOB_FULLMATCH:
    case JOB_SEARCH: {
        so->st.match_all = so->kind == JOB_FULLMATCH;
        isize status     = engine(so, so->kind == JOB_SEARCH);
        if (status == SRE_SUSPEND)
            return Job::Suspend;
        if (status < 0)
            return pattern_error(status), Job::Err;
        out = status ? match_new(state_of(rs.v)) : value_none();
        return out.is_nil() ? Job::Err : Job::Done;
    }

    case JOB_SCAN_MATCH:
    case JOB_SCAN_SEARCH:
    case JOB_ITER: {
        if (so->exhausted) {
            out = value_none();
            return Job::Done;
        }
        isize status = engine(so, so->kind != JOB_SCAN_MATCH);
        if (status == SRE_SUSPEND)
            return Job::Suspend;
        if (status < 0)
            return pattern_error(status), Job::Err;
        out = status ? match_new(state_of(rs.v)) : value_none();
        if (out.is_nil())
            return Job::Err;
        so = state_of(rs.v);
        if (status == 0) {
            so->exhausted = true;
        } else {
            so->st.must_advance = so->st.ptr == so->st.start;
            so->st.start        = so->st.ptr;
        }
        return Job::Done;
    }

    case JOB_FINDALL:
        while (so->st.start <= so->st.end) {
            isize status = engine(so, true);
            if (status == SRE_SUSPEND)
                return Job::Suspend;
            if (status <= 0) {
                if (status == 0)
                    break;
                return pattern_error(status), Job::Err;
            }
            // don't bother to build a match object
            Root item;
            isize groups = pat_of(so->pattern)->groups;
            if (groups == 0) {
                isize b = offset_of(&so->st, so->st.start);
                isize e = offset_of(&so->st, so->st.ptr);
                item    = slice_of(so->string, so->text, so->isbytes, b, e);
            } else if (groups == 1) {
                item = state_getslice(so, 1, true);
            } else {
                TupleObj *t = tuple_new(usize(groups));
                if (!t)
                    return oom(), Job::Err;
                item = obj_value(t);
                for (isize i = 0; i < groups; i++) {
                    Value o = state_getslice(state_of(rs.v), i + 1, true);
                    if (o.is_nil())
                        return Job::Err;
                    static_cast<TupleObj *>(item.v.obj())->items()[i] = o;
                }
            }
            if (item.v.is_nil())
                return Job::Err;
            so = state_of(rs.v);
            if (!list_push(list_of(so->list), item.v))
                return oom(), Job::Err;
            so->st.must_advance = so->st.ptr == so->st.start;
            so->st.start        = so->st.ptr;
        }
        out = so->list;
        return Job::Done;

    case JOB_SPLIT:
        while (!so->count || so->n < so->count) {
            isize status = engine(so, true);
            if (status == SRE_SUSPEND)
                return Job::Suspend;
            if (status <= 0) {
                if (status == 0)
                    break;
                return pattern_error(status), Job::Err;
            }
            // the segment before this match
            Value item = slice_of(so->string, so->text, so->isbytes, so->last,
                                  offset_of(&so->st, so->st.start));
            if (item.is_nil() || !list_push(list_of(state_of(rs.v)->list), item))
                return err_pending() ? Job::Err : (oom(), Job::Err);
            // the groups, if any
            so = state_of(rs.v);
            for (isize i = 0; i < pat_of(so->pattern)->groups; i++) {
                item = state_getslice(state_of(rs.v), i + 1, false);
                if (item.is_nil() || !list_push(list_of(state_of(rs.v)->list), item))
                    return err_pending() ? Job::Err : (oom(), Job::Err);
            }
            so = state_of(rs.v);
            so->n++;
            so->st.must_advance = so->st.ptr == so->st.start;
            so->st.start        = so->st.ptr;
            so->last            = offset_of(&so->st, so->st.start);
        }
        {
            // the segment following the last match (even if empty)
            Value item = slice_of(so->string, so->text, so->isbytes, so->last, so->st.endpos);
            if (item.is_nil() || !list_push(list_of(state_of(rs.v)->list), item))
                return err_pending() ? Job::Err : (oom(), Job::Err);
        }
        out = state_of(rs.v)->list;
        return Job::Done;

    case JOB_SUB:
        if (so->waiting) {
            // The filter has answered, and `out` is what it said.
            so->waiting = false;
            so->match   = Value();
            if (!is_none(out) && !sub_put(so, out))
                return Job::Err;
            goto next_match;
        }
        while (!so->count || so->n < so->count) {
            {
                isize status = engine(so, true);
                if (status == SRE_SUSPEND)
                    return Job::Suspend;
                if (status <= 0) {
                    if (status == 0)
                        break;
                    return pattern_error(status), Job::Err;
                }
                so->b = offset_of(&so->st, so->st.start);
                so->e = offset_of(&so->st, so->st.ptr);
                if (so->last < so->b) {
                    // the segment before this match
                    Root item{ slice_of(so->string, so->text, so->isbytes, so->last, so->b) };
                    if (item.v.is_nil() || !sub_put(state_of(rs.v), item.v))
                        return Job::Err;
                    so = state_of(rs.v);
                }
                if (so->filter_type == FILTER_LITERAL) {
                    if (!sub_put(so, so->filter))
                        return Job::Err;
                } else {
                    // pass the match through the filter
                    Root m{ match_new(so) };
                    if (m.v.is_nil())
                        return Job::Err;
                    so = state_of(rs.v);
                    if (so->filter_type == FILTER_CALLABLE) {
                        so->match   = m.v;
                        so->waiting = true;
                        return Job::Call;
                    }
                    Root item{ expand_template(tpl_of(so->filter), match_of(m.v)) };
                    if (item.v.is_nil())
                        return Job::Err;
                    so = state_of(rs.v);
                    if (!is_none(item.v) && !sub_put(so, item.v))
                        return Job::Err;
                }
            }
        next_match:
            so       = state_of(rs.v);
            so->last = so->e;
            so->n++;
            so->st.must_advance = so->st.ptr == so->st.start;
            so->st.start        = so->st.ptr;
        }
        // the segment following the last match
        if (so->last < so->st.endpos) {
            Root item{ slice_of(so->string, so->text, so->isbytes, so->last, so->st.endpos) };
            if (item.v.is_nil() || !sub_put(state_of(rs.v), item.v))
                return Job::Err;
        }
        out = sub_result(state_of(rs.v));
        return out.is_nil() ? Job::Err : Job::Done;
    }
    return err_set("SystemError", "unknown regular expression job"), Job::Err;
}

R stop_iteration()
{
    Value e = exc_new(exc_find("StopIteration"), Value());
    return e.is_nil() ? R::Err : err_set_value(e);
}

// A job's answer: an iteration that found nothing is over.
R job_answer(StateObj *so, Value got, Value &out)
{
    so->executing = false;
    if (so->kind == JOB_ITER && is_none(got))
        return stop_iteration();
    out = got;
    return R::Ok;
}

// A job given up on, by an exception or a ^C: a scanner can be called again.
void job_drop(StateObj *so)
{
    sre_abandon(&so->st);
    so->waiting   = false;
    so->match     = Value();
    so->executing = false;
}

// The continuation of a job that could not finish in one call. s[0] the
// state; i is what it is waiting on.
enum : u32 { WAIT_START, WAIT_SLEEP, WAIT_CALL };

void job_fail(ContObj *k)
{
    job_drop(state_of(k->s[0]));
}

R job_step(ContObj *k, Value in)
{
    Root rs{ k->s[0] };
    Value out = in;
    switch (k->i) {
    case WAIT_START:
        // The call left a request behind: make it before running on.
        if (k->j == u32(Job::Call)) {
            k->i = WAIT_CALL;
            return cont_call(k, state_of(rs.v)->filter, state_of(rs.v)->match);
        }
        k->i = WAIT_SLEEP;
        return cont_sleep(k, 0);
    case WAIT_SLEEP:
        if (vm_take_interrupt()) {
            job_drop(state_of(rs.v));
            return err_set("KeyboardInterrupt", "");
        }
        break;
    default:
        break;
    }
    switch (job_run(rs.v, out)) {
    case Job::Done:
        if (job_answer(state_of(rs.v), out, out) != R::Ok)
            return R::Err;
        return cont_done(k, out);
    case Job::Suspend:
        k->i = WAIT_SLEEP;
        return cont_sleep(k, 0);
    case Job::Call:
        k->i = WAIT_CALL;
        return cont_call(k, state_of(rs.v)->filter, state_of(rs.v)->match);
    case Job::Err:
        break;
    }
    job_drop(state_of(rs.v));
    return R::Err;
}

// Run a job from a call: its answer, or the continuation that finishes it.
R job_start(Value sv, Value &out)
{
    Root rs{ sv };
    Value got;
    Job r = job_run(rs.v, got);
    if (r == Job::Done)
        return job_answer(state_of(rs.v), got, out);
    if (r == Job::Err) {
        job_drop(state_of(rs.v));
        return R::Err;
    }
    Root kv{ cont_new(job_step) };
    if (kv.v.is_nil()) {
        job_drop(state_of(rs.v));
        return R::Err;
    }
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rs.v;
    k->i       = WAIT_START;
    k->j       = u32(r);
    k->fail    = job_fail;
    out        = kv.v;
    return R::Ok;
}

// ------------------------------------------------------------- arguments

// A Py_ssize_t argument: an int in range, or the error CPython gives.
bool ssize_arg(Value v, isize &out)
{
    Value x = method_self(v);
    if (!is_intval(x)) {
        Buf<128> m;
        m.put('\'').put(type_name(v)).put("' object cannot be interpreted as an integer");
        return err_set("TypeError", m.str()), false;
    }
    i64 n = 0;
    if (!as_index(x, n) || n > 0x7fffffff || n < -0x7fffffff - 1)
        return err_set("OverflowError", "Python int too large to convert to C ssize_t"), false;
    out = isize(n);
    return true;
}

bool int_arg(Value v, i32 &out)
{
    isize n = 0;
    if (!ssize_arg(v, n)) {
        if (err_kind() == Str("OverflowError")) {
            err_clear();
            err_set("OverflowError", "Python int too large to convert to C int");
        }
        return false;
    }
    out = i32(n);
    return true;
}

// Argument Clinic's parser and its messages: `names` after self, `least` of
// them required. `out` takes one value each, Nil where the call left it out.
bool take(const CallArgs &a, Str who, const Str *names, u32 n, u32 least, Value *out)
{
    char num[24];
    u32 given = a.nargs ? a.nargs - 1 : 0;
    for (u32 i = 0; i < n; i++)
        out[i] = Value();
    if (given > n) {
        Buf<160> b;
        b.put(who).put("() takes at most ").put(int_text(num, sizeof num, i64(n)));
        b.put(n == 1 ? " argument (" : " arguments (");
        b.put(int_text(num, sizeof num, i64(given))).put(" given)");
        return err_set("TypeError", b.str()), false;
    }
    for (u32 i = 0; i < given; i++)
        out[i] = a.args[i + 1];
    for (u32 k = 0; k < a.nkw; k++) {
        Str nm = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        u32 i  = 0;
        while (i < n && !(names[i] == nm))
            i++;
        Buf<160> b;
        if (i == n) {
            b.put(who).put("() got an unexpected keyword argument '").put(nm).put('\'');
            return err_set("TypeError", b.str()), false;
        }
        if (!out[i].is_nil()) {
            b.put("argument for ").put(who).put("() given by name ('").put(nm);
            b.put("') and position (").put(int_text(num, sizeof num, i64(i + 1))).put(')');
            return err_set("TypeError", b.str()), false;
        }
        out[i] = a.kwvals[k];
    }
    for (u32 i = 0; i < least; i++)
        if (out[i].is_nil()) {
            Buf<160> b;
            b.put(who).put("() missing required argument '").put(names[i]).put("' (pos ");
            b.put(int_text(num, sizeof num, i64(i + 1))).put(')');
            return err_set("TypeError", b.str()), false;
        }
    return true;
}

template <usize N>
bool take(const CallArgs &a, Str who, const Str (&names)[N], u32 least, Value (&out)[N])
{
    return take(a, who, names, N, least, out);
}

// METH_NOARGS and METH_O, as CPython words them.
bool no_args(const CallArgs &a, Str who)
{
    if (a.nkw || a.nargs > 1) {
        char num[24];
        Buf<160> b;
        b.put(who).put("() takes no arguments (");
        b.put(int_text(num, sizeof num, i64(a.nargs - 1 + a.nkw))).put(" given)");
        return err_set("TypeError", b.str()), false;
    }
    return true;
}

bool one_arg(const CallArgs &a, Str who)
{
    if (a.nkw) {
        Buf<160> b;
        b.put(who).put("() takes no keyword arguments");
        return err_set("TypeError", b.str()), false;
    }
    if (a.nargs != 2) {
        char num[24];
        Buf<160> b;
        b.put(who).put("() takes exactly one argument (");
        b.put(int_text(num, sizeof num, i64(a.nargs ? a.nargs - 1 : 0))).put(" given)");
        return err_set("TypeError", b.str()), false;
    }
    return true;
}

PatternObj *self_pattern(const CallArgs &a, Str who)
{
    if (!a.nargs || !is_pattern(a.args[0])) {
        Buf<128> m;
        m.put("descriptor '").put(who).put("' requires a 're.Pattern' object");
        return err_set("TypeError", m.str()), nullptr;
    }
    return pat_of(a.args[0]);
}

MatchObj *self_match(const CallArgs &a, Str who)
{
    if (!a.nargs || !is_match(a.args[0])) {
        Buf<128> m;
        m.put("descriptor '").put(who).put("' requires a 're.Match' object");
        return err_set("TypeError", m.str()), nullptr;
    }
    return match_of(a.args[0]);
}

// (string, pos=0, endpos=sys.maxsize), after self.
bool take_window(const CallArgs &a, Str who, Value &string, isize &pos, isize &endpos)
{
    constexpr Str NAMES[] = { "string", "pos", "endpos" };
    Value got[3];
    if (!take(a, who, NAMES, 1, got))
        return false;
    string = got[0];
    pos    = 0;
    endpos = 0x7fffffff;
    return (got[1].is_nil() || ssize_arg(got[1], pos)) &&
           (got[2].is_nil() || ssize_arg(got[2], endpos));
}

// ---------------------------------------------------------- pattern calls

R pattern_job(const CallArgs &a, Str who, u8 kind, Value &out)
{
    if (!self_pattern(a, who))
        return R::Err;
    Value string;
    isize pos, endpos;
    if (!take_window(a, who, string, pos, endpos))
        return R::Err;
    Root sv{ state_new(a.args[0], string, pos, endpos) };
    if (sv.v.is_nil())
        return R::Err;
    StateObj *so = state_of(sv.v);
    so->kind     = kind;
    if (kind == JOB_FINDALL) {
        ListObj *l = list_new();
        if (!l)
            return oom();
        state_of(sv.v)->list = obj_value(l);
    }
    return job_start(sv.v, out);
}

// match is prefixmatch under another name, and answers as it.
R p_match(const CallArgs &a, Value &out)
{
    return pattern_job(a, "prefixmatch", JOB_MATCH, out);
}

R p_prefixmatch(const CallArgs &a, Value &out)
{
    return pattern_job(a, "prefixmatch", JOB_MATCH, out);
}

R p_fullmatch(const CallArgs &a, Value &out)
{
    return pattern_job(a, "fullmatch", JOB_FULLMATCH, out);
}

R p_search(const CallArgs &a, Value &out)
{
    return pattern_job(a, "search", JOB_SEARCH, out);
}

R p_findall(const CallArgs &a, Value &out)
{
    return pattern_job(a, "findall", JOB_FINDALL, out);
}

struct ScannerObj : Obj {
    Value pattern;
    Value state; // StateObj
};

ScannerObj *scanner_of(Value v)
{
    return static_cast<ScannerObj *>(v.obj());
}

void scanner_trace(Obj *o)
{
    gc_mark(static_cast<ScannerObj *>(o)->pattern);
    gc_mark(static_cast<ScannerObj *>(o)->state);
}

R scanner_getattr(Value v, StrObj *name, Value &out)
{
    if (name->str() != Str("pattern"))
        return R::NotImpl;
    out = scanner_of(v)->pattern;
    return R::Ok;
}

Value scanner_new(Value pattern, Value string, isize pos, isize endpos)
{
    Root rp{ pattern };
    Root sv{ state_new(pattern, string, pos, endpos) };
    if (sv.v.is_nil())
        return Value();
    ScannerObj *s = static_cast<ScannerObj *>(obj_alloc(&scanner_type, sizeof(ScannerObj)));
    if (!s)
        return oom(), Value();
    s->pattern = rp.v;
    s->state   = sv.v;
    return obj_value(s);
}

R p_scanner(const CallArgs &a, Value &out)
{
    if (!self_pattern(a, "scanner"))
        return R::Err;
    Value string;
    isize pos, endpos;
    if (!take_window(a, "scanner", string, pos, endpos))
        return R::Err;
    out = scanner_new(a.args[0], string, pos, endpos);
    return out.is_nil() ? R::Err : R::Ok;
}

// finditer's iterator: scanner.search until it answers None. What
// iter(scanner.search, None) is, without a callable to call.
struct IterObj : Obj {
    Value scanner; // Nil once it is over
};

void iter_trace(Obj *o)
{
    gc_mark(static_cast<IterObj *>(o)->scanner);
}

R p_finditer(const CallArgs &a, Value &out)
{
    if (!self_pattern(a, "finditer"))
        return R::Err;
    Value string;
    isize pos, endpos;
    if (!take_window(a, "finditer", string, pos, endpos))
        return R::Err;
    Root sc{ scanner_new(a.args[0], string, pos, endpos) };
    if (sc.v.is_nil())
        return R::Err;
    IterObj *it = static_cast<IterObj *>(obj_alloc(&iter_type, sizeof(IterObj)));
    if (!it)
        return oom();
    it->scanner = sc.v;
    out         = obj_value(it);
    return R::Ok;
}

R p_split(const CallArgs &a, Value &out)
{
    if (!self_pattern(a, "split"))
        return R::Err;
    constexpr Str NAMES[] = { "string", "maxsplit" };
    Value got[2];
    if (!take(a, "split", NAMES, 1, got))
        return R::Err;
    isize maxsplit = 0;
    if (!got[1].is_nil() && !ssize_arg(got[1], maxsplit))
        return R::Err;
    Root sv{ state_new(a.args[0], got[0], 0, 0x7fffffff) };
    if (sv.v.is_nil())
        return R::Err;
    ListObj *l = list_new();
    if (!l)
        return oom();
    StateObj *so = state_of(sv.v);
    so->list     = obj_value(l);
    so->kind     = JOB_SPLIT;
    so->count    = maxsplit;
    so->last     = so->st.pos;
    return job_start(sv.v, out);
}

// Call re._compile_template(pattern, repl), and check what it made. s[0] the
// pattern, s[1] the replacement, s[2] what to do with the template once made:
// the state of a sub() to start, or the match to expand.
R template_step(ContObj *k, Value in);

Value compile_template_fn()
{
    return home ? home->compile_template : Value();
}

// A str subclass or a bytes-like that is not bytes, as the retry takes it.
Value plain_template(Value v)
{
    Value x = method_self(v);
    if (is_str(x) && !is_str(v))
        return str_of_bytes(str_of(x)->str());
    Str s;
    if (bytes_like(x, s) && !is_bytes(v))
        return bytes_new(s);
    return Value();
}

// What to do with a template: finish a sub(), or expand for a match.
R template_ready(ContObj *k, Value tpl)
{
    if (!is_template(tpl)) {
        Buf<160> m;
        m.put("the result of compiling a replacement string is ").put(type_name(tpl));
        return err_set("RuntimeError", m.str());
    }
    Root rt{ tpl };
    Value then = k->s[2];
    if (is_match(then)) {
        Value got = expand_template(tpl_of(rt.v), match_of(then));
        return got.is_nil() ? R::Err : cont_done(k, got);
    }
    Root rs{ then };
    StateObj *so = state_of(rs.v);
    if (tpl_of(rt.v)->n == 0) {
        so->filter      = tpl_of(rt.v)->literal;
        so->filter_type = FILTER_LITERAL;
    } else {
        so->filter      = rt.v;
        so->filter_type = FILTER_TEMPLATE;
    }
    Value out;
    if (job_start(rs.v, out) != R::Ok)
        return R::Err;
    return cont_done(k, out);
}

enum : u32 { TPL_IMPORT, TPL_CALL, TPL_RETRY, TPL_DONE };

R template_step(ContObj *k, Value in)
{
    switch (k->i) {
    case TPL_IMPORT: {
        Value fn = compile_template_fn();
        if (fn.is_nil()) {
            StrObj *imp = str_intern("__import__");
            Value f;
            if (!imp || dict_get(builtins_dict(), obj_value(imp), f) != R::Ok)
                return err_pending() ? R::Err : oom();
            Root rf{ f };
            Value mod = str_new("re");
            if (mod.is_nil())
                return R::Err;
            k->i = TPL_CALL;
            return cont_call(k, rf.v, mod);
        }
        in = Value();
        [[fallthrough]];
    }
    case TPL_CALL: {
        if (!in.is_nil()) {
            // `re` is imported: take its compiler, and keep it.
            StrObj *n = str_intern("_compile_template");
            Value fn;
            if (!n || py_getattr(in, n, fn) != R::Ok)
                return err_pending() ? R::Err : oom();
            if (!home_up())
                return R::Err;
            home->compile_template = fn;
        }
        k->i        = TPL_RETRY;
        k->catching = CATCH_ANY;
        return cont_call(k, compile_template_fn(), k->s[0], 2, k->s[1]);
    }
    case TPL_RETRY: {
        k->catching = CATCH_NONE;
        if (!in.is_nil())
            return template_ready(k, in);
        // An unhashable replacement: its plain kind, once.
        Root e{ k->caught };
        k->caught = Value();
        if (exc_type_of(e.v) != exc_find("TypeError") || !k->s[3].is_nil())
            return err_set_value(e.v);
        Root plain{ plain_template(k->s[1]) };
        if (plain.v.is_nil())
            return err_pending() ? R::Err : err_set_value(e.v);
        k->s[3] = plain.v;
        k->i    = TPL_DONE;
        return cont_call(k, compile_template_fn(), k->s[0], 2, plain.v);
    }
    default:
        return template_ready(k, in);
    }
}

R compile_template(Value pattern, Value repl, Value then, Value &out)
{
    Root rp{ pattern }, rr{ repl }, rt{ then };
    Root kv{ cont_new(template_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rp.v;
    k->s[1]    = rr.v;
    k->s[2]    = rt.v;
    k->i       = TPL_IMPORT;
    out        = kv.v;
    return R::Ok;
}

// The one no backslash is in, octet for octet.
bool is_literal(Value repl)
{
    Value x = method_self(repl);
    Str s;
    if (is_str(x))
        s = str_of(x)->str();
    else if (!bytes_like(x, s))
        return false;
    for (usize i = 0; i < s.size(); i++)
        if (s[i] == '\\')
            return false;
    return true;
}

R pattern_subx(const CallArgs &a, Str who, bool subn, Value &out)
{
    if (!self_pattern(a, who))
        return R::Err;
    constexpr Str NAMES[] = { "repl", "string", "count" };
    Value got[3];
    if (!take(a, who, NAMES, 2, got))
        return R::Err;
    isize count = 0;
    if (!got[2].is_nil() && !ssize_arg(got[2], count))
        return R::Err;
    Root repl{ got[0] };
    Root sv{ state_new(a.args[0], got[1], 0, 0x7fffffff) };
    if (sv.v.is_nil())
        return R::Err;
    StateObj *so = state_of(sv.v);
    so->kind     = JOB_SUB;
    so->count    = count;
    so->subn     = subn;
    if (py_callable(repl.v)) {
        so->filter      = repl.v;
        so->filter_type = FILTER_CALLABLE;
        return job_start(sv.v, out);
    }
    if (is_literal(repl.v)) {
        so->filter      = repl.v;
        so->filter_type = FILTER_LITERAL;
        return job_start(sv.v, out);
    }
    // not a literal; hand it over to the template compiler
    return compile_template(a.args[0], repl.v, sv.v, out);
}

R p_sub(const CallArgs &a, Value &out)
{
    return pattern_subx(a, "sub", false, out);
}

R p_subn(const CallArgs &a, Value &out)
{
    return pattern_subx(a, "subn", true, out);
}

R p_copy(const CallArgs &a, Value &out)
{
    if (!self_pattern(a, "__copy__") || !no_args(a, "Pattern.__copy__"))
        return R::Err;
    out = a.args[0];
    return R::Ok;
}

R p_deepcopy(const CallArgs &a, Value &out)
{
    if (!self_pattern(a, "__deepcopy__") || !one_arg(a, "Pattern.__deepcopy__"))
        return R::Err;
    out = a.args[0];
    return R::Ok;
}

constexpr Method PATTERN_METHODS[] = {
    { "prefixmatch", p_prefixmatch }, { "match", p_match },   { "fullmatch", p_fullmatch },
    { "search", p_search },           { "sub", p_sub },       { "subn", p_subn },
    { "findall", p_findall },         { "split", p_split },   { "finditer", p_finditer },
    { "scanner", p_scanner },         { "__copy__", p_copy }, { "__deepcopy__", p_deepcopy },
};

constexpr Type pattern_type{ .name    = "re.Pattern",
                             .trace   = pattern_trace,
                             .hash    = pattern_hash,
                             .eq      = pattern_eq,
                             .repr    = pattern_repr,
                             .getattr = pattern_getattr,
                             .final   = true };

// ------------------------------------------------------------ match calls

R m_group(const CallArgs &a, Value &out)
{
    MatchObj *m = self_match(a, "group");
    if (!m)
        return R::Err;
    if (a.nkw)
        return err_set("TypeError", "Match.group() takes no keyword arguments");
    // A group number may be anything with an __index__, written in Python.
    for (u32 i = 1; i < a.nargs; i++) {
        R r = R::Ok;
        if (redo_converted(a, i, "__index__", m_group, out, r))
            return r;
    }
    Root rm{ a.args[0] };
    u32 size = a.nargs - 1;
    if (size <= 1) {
        out = match_getslice(m, size ? a.args[1] : Value::of_int(0), value_none());
        return out.is_nil() ? R::Err : R::Ok;
    }
    // fetch multiple items
    TupleObj *t = tuple_new(size);
    if (!t)
        return oom();
    Root rt{ obj_value(t) };
    for (u32 i = 0; i < size; i++) {
        Value item = match_getslice(match_of(rm.v), a.args[i + 1], value_none());
        if (item.is_nil())
            return R::Err;
        static_cast<TupleObj *>(rt.v.obj())->items()[i] = item;
    }
    out = rt.v;
    return R::Ok;
}

R m_start(const CallArgs &a, Value &out);
R m_end(const CallArgs &a, Value &out);
R m_span(const CallArgs &a, Value &out);

R m_bound(const CallArgs &a, Str who, int which, Value &out)
{
    MatchObj *m = self_match(a, who);
    if (!m)
        return R::Err;
    if (a.nkw) {
        Buf<96> b;
        b.put(who).put("() takes no keyword arguments");
        return err_set("TypeError", b.str());
    }
    if (a.nargs > 2) {
        char num[24];
        Buf<96> b;
        b.put(who).put(" expected at most 1 argument, got ");
        b.put(int_text(num, sizeof num, i64(a.nargs - 1)));
        return err_set("TypeError", b.str());
    }
    R r = R::Ok;
    if (redo_converted(a, 1, "__index__",
                       which == 0   ? m_start
                       : which == 1 ? m_end
                                    : m_span,
                       out, r))
        return r;
    isize index = 0;
    if (!match_getindex(m, a.nargs > 1 ? a.args[1] : Value(), index))
        return R::Err;
    // marks are -1 if the group is undefined
    if (which == 0)
        out = int_from_i64(m->mark()[index * 2]);
    else if (which == 1)
        out = int_from_i64(m->mark()[index * 2 + 1]);
    else
        out = pair(m->mark()[index * 2], m->mark()[index * 2 + 1]);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_start(const CallArgs &a, Value &out)
{
    return m_bound(a, "start", 0, out);
}

R m_end(const CallArgs &a, Value &out)
{
    return m_bound(a, "end", 1, out);
}

R m_span(const CallArgs &a, Value &out)
{
    return m_bound(a, "span", 2, out);
}

R m_groups(const CallArgs &a, Value &out)
{
    MatchObj *m = self_match(a, "groups");
    if (!m)
        return R::Err;
    constexpr Str NAMES[] = { "default" };
    Value got[1];
    if (!take(a, "groups", NAMES, 0, got))
        return R::Err;
    Root def{ got[0].is_nil() ? value_none() : got[0] };
    Root rm{ a.args[0] };
    TupleObj *t = tuple_new(usize(m->groups - 1));
    if (!t)
        return oom();
    Root rt{ obj_value(t) };
    for (isize index = 1; index < match_of(rm.v)->groups; index++) {
        Value item = match_getslice_by_index(match_of(rm.v), index, def.v);
        if (item.is_nil())
            return R::Err;
        static_cast<TupleObj *>(rt.v.obj())->items()[index - 1] = item;
    }
    out = rt.v;
    return R::Ok;
}

R m_groupdict(const CallArgs &a, Value &out)
{
    MatchObj *m = self_match(a, "groupdict");
    if (!m)
        return R::Err;
    constexpr Str NAMES[] = { "default" };
    Value got[1];
    if (!take(a, "groupdict", NAMES, 0, got))
        return R::Err;
    Root def{ got[0].is_nil() ? value_none() : got[0] };
    Root rm{ a.args[0] };
    DictObj *d = dict_new();
    if (!d)
        return oom();
    Root rd{ obj_value(d) };
    Value gi = pat_of(m->pattern)->groupindex;
    if (!gi.is_nil()) {
        Root rg{ gi };
        usize at = 0;
        Value key, val;
        while (table_next(static_cast<DictObj *>(rg.v.obj())->t, at, key, val)) {
            Root rk{ key };
            Root item{ match_getslice(match_of(rm.v), key, def.v) };
            if (item.v.is_nil())
                return R::Err;
            if (dict_set(static_cast<DictObj *>(rd.v.obj()), rk.v, item.v) != R::Ok)
                return R::Err;
        }
    }
    out = rd.v;
    return R::Ok;
}

R m_expand(const CallArgs &a, Value &out)
{
    MatchObj *m = self_match(a, "expand");
    if (!m)
        return R::Err;
    constexpr Str NAMES[] = { "template" };
    Value got[1];
    if (!take(a, "expand", NAMES, 1, got))
        return R::Err;
    return compile_template(m->pattern, got[0], a.args[0], out);
}

R m_copy(const CallArgs &a, Value &out)
{
    if (!self_match(a, "__copy__") || !no_args(a, "Match.__copy__"))
        return R::Err;
    out = a.args[0];
    return R::Ok;
}

R m_deepcopy(const CallArgs &a, Value &out)
{
    if (!self_match(a, "__deepcopy__") || !one_arg(a, "Match.__deepcopy__"))
        return R::Err;
    out = a.args[0];
    return R::Ok;
}

constexpr Method MATCH_METHODS[] = {
    { "group", m_group },   { "start", m_start },   { "end", m_end },
    { "span", m_span },     { "groups", m_groups }, { "groupdict", m_groupdict },
    { "expand", m_expand }, { "__copy__", m_copy }, { "__deepcopy__", m_deepcopy },
};

constexpr Type match_type{ .name    = "re.Match",
                           .trace   = match_trace,
                           .repr    = match_repr,
                           .getitem = match_getitem,
                           .getattr = match_getattr,
                           .final   = true };

// ---------------------------------------------------------- scanner calls

// scanner_begin(): one call at a time.
R scanner_call(const CallArgs &a, Str who, u8 kind, Value &out)
{
    if (!a.nargs || !a.args[0].is_obj() || a.args[0].obj()->type != &scanner_type)
        return err_set2("TypeError", "descriptor requires a '_sre.SRE_Scanner' object", who);
    if (a.nkw || a.nargs > 1) {
        Buf<96> b;
        b.put(who).put("() takes no arguments");
        return err_set("TypeError", b.str());
    }
    StateObj *so = state_of(scanner_of(a.args[0])->state);
    if (so->executing)
        return err_set("ValueError", "regular expression scanner already executing");
    so->executing = true;
    so->kind      = kind;
    return job_start(obj_value(so), out);
}

R s_match(const CallArgs &a, Value &out)
{
    return scanner_call(a, "prefixmatch", JOB_SCAN_MATCH, out);
}

R s_prefixmatch(const CallArgs &a, Value &out)
{
    return scanner_call(a, "prefixmatch", JOB_SCAN_MATCH, out);
}

R s_search(const CallArgs &a, Value &out)
{
    return scanner_call(a, "search", JOB_SCAN_SEARCH, out);
}

constexpr Method SCANNER_METHODS[] = {
    { "prefixmatch", s_prefixmatch },
    { "match", s_match },
    { "search", s_search },
};

constexpr Type scanner_type{ .name    = "_sre.SRE_Scanner",
                             .trace   = scanner_trace,
                             .getattr = scanner_getattr,
                             .final   = true };

constexpr Type template_type{ .name = "_sre.SRE_Template", .trace = template_trace, .final = true };

// ---------------------------------------------------------- the iterator

Value iter_self(Value v)
{
    return v;
}

R it_next(const CallArgs &a, Value &out)
{
    if (!a.nargs || !a.args[0].is_obj() || a.args[0].obj()->type != &iter_type ||
        !meth_args(a, "__next__", 0, 0))
        return err_pending() ? R::Err : err_set("TypeError", "__next__() requires an iterator");
    IterObj *it = static_cast<IterObj *>(a.args[0].obj());
    if (it->scanner.is_nil())
        return stop_iteration();
    CallArgs sa;
    Value argv[1] = { it->scanner };
    sa.args       = argv;
    sa.nargs      = 1;
    return scanner_call(sa, "__next__", JOB_ITER, out);
}

// The slot: the same search, but never parking, for whatever steps an
// iterator without the VM.
R iter_next_slot(Value v, Value &out)
{
    IterObj *it = static_cast<IterObj *>(v.obj());
    if (it->scanner.is_nil())
        return R::NotImpl;
    Root rv{ v };
    StateObj *so = state_of(scanner_of(it->scanner)->state);
    if (so->executing)
        return err_set("ValueError", "regular expression scanner already executing");
    u32 slice    = so->st.slice;
    so->st.slice = 0;
    so->kind     = JOB_SCAN_SEARCH;
    Value got;
    Job r        = job_run(obj_value(so), got);
    so->st.slice = slice;
    if (r != Job::Done) {
        job_drop(so);
        return R::Err;
    }
    if (is_none(got)) {
        static_cast<IterObj *>(rv.v.obj())->scanner = Value();
        return R::NotImpl;
    }
    out = got;
    return R::Ok;
}

constexpr Method ITER_METHODS[] = {
    { "__next__", it_next },
};

constexpr Type iter_type{ .name   = "callable_iterator",
                          .trace  = iter_trace,
                          .iter   = iter_self,
                          .next   = iter_next_slot,
                          .final  = true,
                          .vmnext = true };

// ---------------------------------------------------------- the functions

R f_getcodesize(const CallArgs &a, Value &out)
{
    if (!args_only(a, "getcodesize", 0, 0))
        return R::Err;
    out = Value::of_int(SRE_CODESIZE);
    return R::Ok;
}

R char_fn(const CallArgs &a, Str who, u32 (*fn)(u32), bool (*test)(u32), Value &out)
{
    if (!args_only(a, who, 1, 1))
        return R::Err;
    i32 ch = 0;
    if (!int_arg(a.args[0], ch))
        return R::Err;
    if (test)
        out = value_bool(test(u32(ch)));
    else
        out = int_from_i64(i64(i32(fn(u32(ch)))));
    return out.is_nil() ? R::Err : R::Ok;
}

R f_ascii_iscased(const CallArgs &a, Value &out)
{
    return char_fn(a, "ascii_iscased", nullptr, sre_ascii_iscased, out);
}

R f_unicode_iscased(const CallArgs &a, Value &out)
{
    return char_fn(a, "unicode_iscased", nullptr, sre_unicode_iscased, out);
}

R f_ascii_tolower(const CallArgs &a, Value &out)
{
    return char_fn(a, "ascii_tolower", sre_lower_ascii, nullptr, out);
}

R f_unicode_tolower(const CallArgs &a, Value &out)
{
    return char_fn(a, "unicode_tolower", sre_lower_unicode, nullptr, out);
}

// A clinic `subclass_of` argument: the type or a subclass of it.
bool kind_arg(Value v, bool ok, Str who, Str what, Str want)
{
    if (ok)
        return true;
    Buf<160> m;
    m.put(who).put("() argument '").put(what).put("' must be ").put(want).put(", not ");
    m.put(type_name(v));
    return err_set("TypeError", m.str()), false;
}

R f_compile(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "pattern", "flags", "code", "groups", "groupindex", "indexgroup" };
    Value got[6];
    CallArgs shifted = a;
    // take counts a self; these are all arguments.
    Value argv[8];
    if (a.nargs > 6)
        return err_set("TypeError", "compile() takes at most 6 arguments");
    argv[0] = value_none();
    for (u32 i = 0; i < a.nargs; i++)
        argv[i + 1] = a.args[i];
    shifted.args  = argv;
    shifted.nargs = a.nargs + 1;
    if (!take(shifted, "compile", NAMES, 6, got))
        return R::Err;
    i32 flags    = 0;
    isize groups = 0;
    Value code   = method_self(got[2]);
    Value gi     = method_self(got[4]);
    Value ig     = method_self(got[5]);
    if (!int_arg(got[1], flags) || !kind_arg(got[2], is_list(code), "compile", "code", "list") ||
        !ssize_arg(got[3], groups) ||
        !kind_arg(got[4], is_dict(gi), "compile", "groupindex", "dict") ||
        !kind_arg(got[5], is_tuple(ig), "compile", "indexgroup", "tuple"))
        return R::Err;
    Root rpat{ got[0] }, rcode{ code }, rgi{ gi }, rig{ ig };
    usize n       = list_of(code)->items.size();
    PatternObj *p = static_cast<PatternObj *>(
        obj_alloc(&pattern_type, sizeof(PatternObj) + n * sizeof(SreCode)));
    if (!p)
        return oom();
    p->pattern    = rpat.v;
    p->groupindex = Value();
    p->indexgroup = Value();
    p->codesize   = u32(n);
    p->flags      = flags;
    p->groups     = groups;
    p->isbytes    = -1;
    Root rp{ obj_value(p) };
    for (usize i = 0; i < n; i++) {
        Value o = list_of(rcode.v)->items[i];
        i64 v   = 0;
        if (!is_intval(method_self(o))) {
            Buf<128> m;
            m.put('\'').put(type_name(o)).put("' object cannot be interpreted as an integer");
            return err_set("TypeError", m.str());
        }
        if (!as_index(method_self(o), v) || v < 0) {
            if (v < 0 && as_index(method_self(o), v))
                return err_set("OverflowError", "can't convert negative int to unsigned");
            return err_set("OverflowError", "Python int too large to convert to C unsigned long");
        }
        if (v > 0xffffffffll)
            return err_set("OverflowError", "regular expression code size limit exceeded");
        pat_of(rp.v)->code()[i] = SreCode(v);
    }
    if (!is_none(rpat.v)) {
        Target t;
        if (!target_of(rpat.v, t))
            return R::Err;
        pat_of(rp.v)->isbytes = t.isbytes ? 1 : 0;
    }
    if (dict_len(static_cast<DictObj *>(rgi.v.obj())) > 0) {
        pat_of(rp.v)->groupindex = rgi.v;
        if (static_cast<TupleObj *>(rig.v.obj())->len > 0)
            pat_of(rp.v)->indexgroup = rig.v;
    }
    if (!sre_validate(pat_of(rp.v)->code(), n, groups))
        return err_set("RuntimeError", "invalid SRE code");
    out = rp.v;
    return R::Ok;
}

R f_template(const CallArgs &a, Value &out)
{
    if (!args_only(a, "template", 2, 2))
        return R::Err;
    Value tl = method_self(a.args[1]);
    if (!kind_arg(a.args[1], is_list(tl), "template", "template", "list"))
        return R::Err;
    Root rl{ tl };
    usize n = list_of(tl)->items.size();
    if ((n & 1) == 0 || n < 1)
        return err_set("TypeError", "invalid template");
    n /= 2;
    TemplateObj *t = static_cast<TemplateObj *>(
        obj_alloc(&template_type, sizeof(TemplateObj) + n * sizeof(TemplateItem)));
    if (!t)
        return oom();
    t->n       = 0;
    t->chunks  = isize(1 + 2 * n);
    t->literal = list_of(rl.v)->items[0];
    Root rt{ obj_value(t) };
    for (usize i = 0; i < n; i++) {
        Value iv    = list_of(rl.v)->items[2 * i + 1];
        isize index = 0;
        if (!ssize_arg(iv, index))
            return R::Err;
        if (index < 0)
            return err_set("TypeError", "invalid template");
        Value literal = list_of(rl.v)->items[2 * i + 2];
        // Skip empty literals.
        if ((is_str(literal) && str_of(literal)->len == 0) ||
            (is_bytes(literal) && static_cast<BytesObj *>(literal.obj())->len == 0)) {
            literal = Value();
            tpl_of(rt.v)->chunks--;
        }
        tpl_of(rt.v)->items()[i] = TemplateItem{ index, literal };
        tpl_of(rt.v)->n          = isize(i + 1);
    }
    out = rt.v;
    return R::Ok;
}

constexpr ModDef DEFS[] = {
    { "compile", f_compile },
    { "template", f_template },
    { "getcodesize", f_getcodesize },
    { "ascii_iscased", f_ascii_iscased },
    { "unicode_iscased", f_unicode_iscased },
    { "ascii_tolower", f_ascii_tolower },
    { "unicode_tolower", f_unicode_tolower },
};

} // namespace

bool sre_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!method_install(&pattern_type, PATTERN_METHODS) ||
        !method_install(&match_type, MATCH_METHODS) ||
        !method_install(&scanner_type, SCANNER_METHODS) ||
        !method_install(&iter_type, ITER_METHODS))
        return false;
    // Patterns and matches are generic over the kind of string.
    constexpr const Type *GENERIC[] = { &pattern_type, &match_type };
    if (!genalias_install(GENERIC, 2))
        return false;
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    return mod_defs(d, DEFS) && mod_int(d, "MAGIC", SRE_MAGIC) &&
           mod_int(d, "CODESIZE", SRE_CODESIZE) && mod_int(d, "MAXREPEAT", SRE_MAXREPEAT) &&
           mod_int(d, "MAXGROUPS", SRE_MAXGROUPS) && mod_str(d, "copyright", COPYRIGHT);
}
