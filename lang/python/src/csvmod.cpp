// `_csv`: what csv.py stands on. CPython's Modules/_csv.c, in its own shape:
// the dialect, the parser's state machine, the writer's two-pass join, and
// the registry.
//
// What differs is where it meets Python. A reader's lines come from an
// iterator that may be a file or a generator, and a writer's `write` is
// whatever the program handed it, so reading a row and writing one are
// continuations when they have to be -- and plain calls when the lines come
// out of a list and the fields are all str and numbers.
#include "bigint.h"
#include "builtin.h"
#include "call.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "posix.h"
#include "type.h"
#include "ustr.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

constexpr u32 NOT_SET = u32(-1);
constexpr u32 EOL     = u32(-2);

enum ParserState : u8 {
    START_RECORD,
    START_FIELD,
    ESCAPED_CHAR,
    IN_FIELD,
    IN_QUOTED_FIELD,
    ESCAPE_IN_QUOTED_FIELD,
    QUOTE_IN_QUOTED_FIELD,
    EAT_CRNL,
    AFTER_ESCAPED_CRNL,
};

enum QuoteStyle : i64 {
    QUOTE_MINIMAL,
    QUOTE_ALL,
    QUOTE_NONNUMERIC,
    QUOTE_NONE,
    QUOTE_STRINGS,
    QUOTE_NOTNULL,
};

struct StyleDesc {
    i64 style;
    Str name;
};

constexpr StyleDesc QUOTE_STYLES[] = {
    { QUOTE_MINIMAL, "QUOTE_MINIMAL" }, { QUOTE_ALL, "QUOTE_ALL" },
    { QUOTE_NONNUMERIC, "QUOTE_NONNUMERIC" }, { QUOTE_NONE, "QUOTE_NONE" },
    { QUOTE_STRINGS, "QUOTE_STRINGS" },   { QUOTE_NOTNULL, "QUOTE_NOTNULL" },
};

struct Home {
    Value error;    // the class _csv.Error
    Value dialects; // the registry, a dict
    i64 field_limit = 128 * 1024;
};

Home *home;

void home_mark()
{
    if (!home)
        return;
    gc_mark(home->error);
    gc_mark(home->dialects);
}

Home *here()
{
    if (!home) {
        home = heap_new<Home>();
        if (home)
            gc_root_hook(home_mark);
    }
    return home;
}

// _csv.Error(msg), pending.
R csv_error(Str msg)
{
    Home *h = here();
    if (!h || h->error.is_nil())
        return err_set("Exception", msg);
    Root m{ str_new(msg) };
    TupleObj *t = m.v.is_nil() ? nullptr : tuple_new(1);
    if (!t)
        return err_pending() ? R::Err : oom();
    t->items()[0] = m.v;
    Root rt{ obj_value(t) };
    Value e = exc_construct(here()->error, rt.v);
    return e.is_nil() ? R::Err : err_set_value(e);
}

Str put_char(Buf<8> &b, u32 c)
{
    char tmp[4];
    b.put(Str(tmp, cp_encode(c, tmp)));
    return b.str();
}

// ------------------------------------------------------------------ Dialect

struct DialectObj : Obj {
    Value lineterminator; // a str
    u32 delimiter, quotechar, escapechar;
    i64 quoting;
    bool doublequote, skipinitialspace, strict;
};

extern const Type dialect_type;

DialectObj *dialect_of(Value v)
{
    return static_cast<DialectObj *>(v.obj());
}

bool is_dialect(Value v)
{
    return v.is_obj() && v.obj()->type == &dialect_type;
}

void dialect_trace(Obj *o)
{
    gc_mark(static_cast<DialectObj *>(o)->lineterminator);
}

Value char_or_none(u32 c)
{
    if (c == NOT_SET)
        return value_none();
    char tmp[4];
    return str_new(Str(tmp, cp_encode(c, tmp)));
}

R dialect_getattr(Value v, StrObj *name, Value &out)
{
    DialectObj *d = dialect_of(v);
    Str n         = name->str();
    if (n == "delimiter")
        out = char_or_none(d->delimiter);
    else if (n == "quotechar")
        out = char_or_none(d->quotechar);
    else if (n == "escapechar")
        out = char_or_none(d->escapechar);
    else if (n == "lineterminator")
        out = d->lineterminator;
    else if (n == "quoting")
        out = int_from_i64(d->quoting);
    else if (n == "doublequote")
        out = value_bool(d->doublequote);
    else if (n == "skipinitialspace")
        out = value_bool(d->skipinitialspace);
    else if (n == "strict")
        out = value_bool(d->strict);
    else
        return R::NotImpl;
    return out.is_nil() ? R::Err : R::Ok;
}

bool str_contains_cp(Value s, u32 c)
{
    Str t    = str_of(s)->str();
    usize at = 0;
    while (at < t.size()) {
        u32 cp = 0;
        at += cp_decode(t, at, cp);
        if (cp == c)
            return true;
    }
    return false;
}

bool set_char(Str name, u32 &target, Value src, u32 dflt, bool none_ok)
{
    if (src.is_nil()) {
        target = dflt;
        return true;
    }
    if (none_ok && is_none(src)) {
        target = NOT_SET;
        return true;
    }
    Buf<128> m;
    if (!is_str(src)) {
        m.put('"').put(name).put("\" must be a unicode character");
        m.put(none_ok ? Str(" or None") : Str()).put(", not ").put(type_name(src));
        return err_set("TypeError", m.str()) == R::Ok;
    }
    if (str_of(src)->chars != 1) {
        char tmp[24];
        m.put('"').put(name).put("\" must be a unicode character");
        m.put(none_ok ? Str(" or None") : Str()).put(", not a string of length ");
        m.put(int_text(tmp, sizeof tmp, str_of(src)->chars));
        return err_set("TypeError", m.str()) == R::Ok;
    }
    u32 cp = 0;
    cp_decode(str_of(src)->str(), 0, cp);
    target = cp;
    return true;
}

bool check_char(Str name, u32 c, DialectObj *d, bool allowspace)
{
    Buf<96> m;
    if (c == '\r' || c == '\n' || (c == ' ' && !allowspace)) {
        m.put("bad ").put(name).put(" value");
        return err_set("ValueError", m.str()) == R::Ok;
    }
    if (c != NOT_SET && str_contains_cp(d->lineterminator, c)) {
        m.put("bad ").put(name).put(" or lineterminator value");
        return err_set("ValueError", m.str()) == R::Ok;
    }
    return true;
}

bool check_chars(Str n1, Str n2, u32 c1, u32 c2)
{
    if (c1 == c2 && c1 != NOT_SET) {
        Buf<96> m;
        m.put("bad ").put(n1).put(" or ").put(n2).put(" value");
        return err_set("ValueError", m.str()) == R::Ok;
    }
    return true;
}

constexpr Str DIALECT_KWS[] = { "dialect",        "delimiter", "doublequote",
                                "escapechar",     "lineterminator", "quotechar",
                                "quoting",        "skipinitialspace", "strict" };

// Dialect(dialect=None, **options): a validated dialect. `v` holds the
// arguments as DIALECT_KWS names them.
R dialect_make(Value *v, Value &out)
{
    Roots pin{ v, 9 };
    Root dialect{ v[0] };
    if (!dialect.v.is_nil() && is_str(dialect.v)) {
        Home *h = here();
        Value got;
        R r = h ? dict_get(static_cast<DictObj *>(h->dialects.obj()), dialect.v, got) : R::Err;
        if (r == R::Err)
            return R::Err;
        if (r == R::NotImpl)
            return csv_error("unknown dialect");
        dialect = got;
    }
    bool bare = true;
    for (u32 i = 1; i < 9; i++)
        bare = bare && v[i].is_nil();
    if (!dialect.v.is_nil() && is_dialect(dialect.v) && bare) {
        out = dialect.v;
        return R::Ok;
    }
    if (!dialect.v.is_nil()) {
        for (u32 i = 1; i < 9; i++) {
            if (!v[i].is_nil())
                continue;
            StrObj *n = str_intern(DIALECT_KWS[i]);
            if (!n)
                return oom();
            Value got;
            Got g = py_attr(dialect.v, n, got);
            if (g == Got::Error)
                return R::Err;
            if (g == Got::Call)
                return err_set2("TypeError", "a dialect attribute that needs a call",
                                DIALECT_KWS[i]);
            if (g == Got::Ok)
                v[i] = got;
        }
    }
    DialectObj *d = static_cast<DialectObj *>(obj_alloc(&dialect_type, sizeof(DialectObj)));
    if (!d)
        return oom();
    d->lineterminator = Value();
    Root rd{ obj_value(d) };
    if (!set_char("delimiter", d->delimiter, v[1], ',', false))
        return R::Err;
    d->doublequote = v[2].is_nil() ? true : py_truth(v[2]);
    if (!set_char("escapechar", d->escapechar, v[3], NOT_SET, true))
        return R::Err;
    if (v[4].is_nil()) {
        Value lt = str_new("\r\n");
        if (lt.is_nil())
            return R::Err;
        dialect_of(rd.v)->lineterminator = lt;
    } else if (!is_str(v[4])) {
        Buf<96> m;
        m.put("\"lineterminator\" must be a string, not ").put(type_name(v[4]));
        return err_set("TypeError", m.str());
    } else {
        d->lineterminator = v[4];
    }
    d = dialect_of(rd.v);
    if (!set_char("quotechar", d->quotechar, v[5], '"', true))
        return R::Err;
    if (v[6].is_nil()) {
        d->quoting = QUOTE_MINIMAL;
    } else {
        i64 q = 0;
        if (!(v[6].is_int() || (v[6].is_obj() && v[6].obj()->type == &int_type)) ||
            !as_index(v[6], q)) {
            Buf<96> m;
            m.put("\"quoting\" must be an integer, not ").put(type_name(v[6]));
            return err_set("TypeError", m.str());
        }
        d->quoting = q;
    }
    d->skipinitialspace = !v[7].is_nil() && py_truth(v[7]);
    d->strict           = !v[8].is_nil() && py_truth(v[8]);

    bool known = false;
    for (const StyleDesc &s : QUOTE_STYLES)
        known = known || s.style == d->quoting;
    if (!known)
        return err_set("TypeError", "bad \"quoting\" value");
    if (!v[5].is_nil() && is_none(v[5]) && v[6].is_nil())
        d->quoting = QUOTE_NONE;
    if (d->quoting != QUOTE_NONE && d->quotechar == NOT_SET)
        return err_set("TypeError", "quotechar must be set if quoting enabled");
    if (!check_char("delimiter", d->delimiter, d, true) ||
        !check_char("escapechar", d->escapechar, d, !d->skipinitialspace) ||
        !check_char("quotechar", d->quotechar, d, !d->skipinitialspace) ||
        !check_chars("delimiter", "escapechar", d->delimiter, d->escapechar) ||
        !check_chars("delimiter", "quotechar", d->delimiter, d->quotechar) ||
        !check_chars("escapechar", "quotechar", d->escapechar, d->quotechar))
        return R::Err;
    out = rd.v;
    return R::Ok;
}

// The dialect a call names: its optional positional one and its keywords.
R dialect_from_call(const CallArgs &a, u32 at, Str who, Value &out)
{
    Value v[9];
    if (a.nargs > at + 1) {
        Buf<96> m;
        m.put(who).put(" expected at most ").put(u64(at + 1)).put(" arguments");
        return err_set("TypeError", m.str());
    }
    if (a.nargs > at)
        v[0] = a.args[at];
    for (u32 k = 0; k < a.nkw; k++) {
        Str nm = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        u32 i  = 0;
        while (i < 9 && DIALECT_KWS[i] != nm)
            i++;
        if (i == 9) {
            Buf<128> m;
            m.put("'").put(nm).put("' is an invalid keyword argument for ").put(who);
            return err_set("TypeError", m.str());
        }
        if (!v[i].is_nil()) {
            Buf<96> m;
            m.put("argument for ").put(who).put(" given by name ('").put(nm).put("') and position");
            return err_set("TypeError", m.str());
        }
        v[i] = a.kwvals[k];
    }
    return dialect_make(v, out);
}

R b_dialect(const CallArgs &a, Value &out)
{
    return dialect_from_call(a, 0, "Dialect()", out);
}

R dm_reduce(const CallArgs &a, Value &out)
{
    (void)out;
    Buf<96> m;
    m.put("cannot pickle '").put(a.nargs ? type_name(a.args[0]) : Str("Dialect"));
    m.put("' instances");
    return err_set("TypeError", m.str());
}

R dm_replace(const CallArgs &a, Value &out)
{
    if (a.nargs != 1)
        return err_set("TypeError", "__replace__() takes no positional arguments");
    return dialect_from_call(a, 0, "__replace__()", out);
}

constexpr Method DIALECT_METHODS[] = {
    { "__reduce__", dm_reduce },
    { "__reduce_ex__", dm_reduce },
    { "__replace__", dm_replace },
};

R dialect_repr(Value v, String &out)
{
    char tmp[24];
    Buf<96> b;
    b.put("<_csv.Dialect object at ").put(addr_text(tmp, sizeof tmp, v.obj())).put('>');
    return out.append(b.str()) ? R::Ok : oom();
}

constexpr Type dialect_type{ .name    = "_csv.Dialect",
                             .trace   = dialect_trace,
                             .repr    = dialect_repr,
                             .getattr = dialect_getattr };

// ------------------------------------------------------------------ reader

struct ReaderObj : Obj {
    Value input;   // the iterator
    Value dialect;
    Value fields;  // ListObj, or Nil when the row is handed out
    Vec<u32> field;
    u32 line_num;
    ParserState state;
    bool unquoted;
};

extern const Type reader_type;

ReaderObj *reader_of(Value v)
{
    return static_cast<ReaderObj *>(v.obj());
}

void reader_trace(Obj *o)
{
    ReaderObj *r = static_cast<ReaderObj *>(o);
    gc_mark(r->input);
    gc_mark(r->dialect);
    gc_mark(r->fields);
}

void reader_fini(Obj *o)
{
    static_cast<ReaderObj *>(o)->field.~Vec();
}

R parse_save_field(Value rv)
{
    ReaderObj *r  = reader_of(rv);
    DialectObj *d = dialect_of(r->dialect);
    Root field;
    if (r->unquoted && r->field.empty() && (d->quoting == QUOTE_NOTNULL ||
                                            d->quoting == QUOTE_STRINGS)) {
        field = value_none();
    } else {
        String s;
        for (u32 c : r->field)
            if (!cp_append(s, c))
                return oom();
        field = str_new(s.str());
        if (field.v.is_nil())
            return R::Err;
        r = reader_of(rv);
        d = dialect_of(r->dialect);
        if (r->unquoted && !r->field.empty() &&
            (d->quoting == QUOTE_NONNUMERIC || d->quoting == QUOTE_STRINGS)) {
            Value f;
            if (py_float_of(field.v, f) != R::Ok)
                return R::Err;
            field = f;
        }
        reader_of(rv)->field.clear();
    }
    return list_push(list_of(reader_of(rv)->fields), field.v) ? R::Ok : oom();
}

R parse_add_char(ReaderObj *r, u32 c)
{
    i64 limit = here()->field_limit;
    if (i64(r->field.size()) >= limit) {
        char tmp[24];
        Buf<96> m;
        m.put("field larger than field limit (").put(int_text(tmp, sizeof tmp, limit)).put(')');
        return csv_error(m.str());
    }
    return r->field.push(c) ? R::Ok : oom();
}

R parse_process_char(Value rv, u32 c)
{
    ReaderObj *r  = reader_of(rv);
    DialectObj *d = dialect_of(r->dialect);
    switch (r->state) {
    case START_RECORD:
        if (c == EOL)
            break;
        if (c == '\n' || c == '\r') {
            r->state = EAT_CRNL;
            break;
        }
        r->state = START_FIELD;
        [[fallthrough]];
    case START_FIELD:
        r->unquoted = true;
        if (c == '\n' || c == '\r' || c == EOL) {
            if (parse_save_field(rv) != R::Ok)
                return R::Err;
            reader_of(rv)->state = c == EOL ? START_RECORD : EAT_CRNL;
        } else if (c == d->quotechar && d->quoting != QUOTE_NONE) {
            r->unquoted = false;
            r->state    = IN_QUOTED_FIELD;
        } else if (c == d->escapechar) {
            r->state = ESCAPED_CHAR;
        } else if (c == ' ' && d->skipinitialspace) {
            // spaces at the start of a field are skipped
        } else if (c == d->delimiter) {
            if (parse_save_field(rv) != R::Ok)
                return R::Err;
        } else {
            if (parse_add_char(r, c) != R::Ok)
                return R::Err;
            r->state = IN_FIELD;
        }
        break;

    case ESCAPED_CHAR:
        if (c == '\n' || c == '\r') {
            if (parse_add_char(r, c) != R::Ok)
                return R::Err;
            r->state = AFTER_ESCAPED_CRNL;
            break;
        }
        if (c == EOL)
            c = '\n';
        if (parse_add_char(r, c) != R::Ok)
            return R::Err;
        r->state = IN_FIELD;
        break;

    case AFTER_ESCAPED_CRNL:
        if (c == EOL)
            break;
        [[fallthrough]];
    case IN_FIELD:
        if (c == '\n' || c == '\r' || c == EOL) {
            if (parse_save_field(rv) != R::Ok)
                return R::Err;
            reader_of(rv)->state = c == EOL ? START_RECORD : EAT_CRNL;
        } else if (c == d->escapechar) {
            r->state = ESCAPED_CHAR;
        } else if (c == d->delimiter) {
            if (parse_save_field(rv) != R::Ok)
                return R::Err;
            reader_of(rv)->state = START_FIELD;
        } else if (parse_add_char(r, c) != R::Ok) {
            return R::Err;
        }
        break;

    case IN_QUOTED_FIELD:
        if (c == EOL) {
            // a quoted field spans lines
        } else if (c == d->escapechar) {
            r->state = ESCAPE_IN_QUOTED_FIELD;
        } else if (c == d->quotechar && d->quoting != QUOTE_NONE) {
            r->state = d->doublequote ? QUOTE_IN_QUOTED_FIELD : IN_FIELD;
        } else if (parse_add_char(r, c) != R::Ok) {
            return R::Err;
        }
        break;

    case ESCAPE_IN_QUOTED_FIELD:
        if (c == EOL)
            c = '\n';
        if (parse_add_char(r, c) != R::Ok)
            return R::Err;
        r->state = IN_QUOTED_FIELD;
        break;

    case QUOTE_IN_QUOTED_FIELD:
        if (d->quoting != QUOTE_NONE && c == d->quotechar) {
            if (parse_add_char(r, c) != R::Ok)
                return R::Err;
            r->state = IN_QUOTED_FIELD;
        } else if (c == d->delimiter) {
            if (parse_save_field(rv) != R::Ok)
                return R::Err;
            reader_of(rv)->state = START_FIELD;
        } else if (c == '\n' || c == '\r' || c == EOL) {
            if (parse_save_field(rv) != R::Ok)
                return R::Err;
            reader_of(rv)->state = c == EOL ? START_RECORD : EAT_CRNL;
        } else if (!d->strict) {
            if (parse_add_char(r, c) != R::Ok)
                return R::Err;
            r->state = IN_FIELD;
        } else {
            Buf<8> a, b;
            Buf<64> m;
            m.put('\'').put(put_char(a, d->delimiter)).put("' expected after '");
            m.put(put_char(b, d->quotechar)).put('\'');
            return csv_error(m.str());
        }
        break;

    case EAT_CRNL:
        if (c == '\n' || c == '\r') {
            // part of the line ending
        } else if (c == EOL) {
            r->state = START_RECORD;
        } else {
            return csv_error("new-line character seen in unquoted field - do you need to open "
                             "the file with newline=''?");
        }
        break;
    }
    return R::Ok;
}

R parse_reset(Value rv)
{
    ListObj *l = list_new();
    if (!l)
        return oom();
    ReaderObj *r = reader_of(rv);
    r->fields    = obj_value(l);
    r->field.clear();
    r->state    = START_RECORD;
    r->unquoted = false;
    return R::Ok;
}

// One line of input into the parser. Done: the row is complete.
enum class Row : u8 { More, Done, Error };

Row feed_line(Value rv, Value line)
{
    Root rl{ line };
    if (!is_str(rl.v)) {
        Buf<160> m;
        m.put("iterator should return strings, not ").put(type_name(rl.v));
        m.put(" (the file should be opened in text mode)");
        csv_error(m.str());
        return Row::Error;
    }
    ReaderObj *r = reader_of(rv);
    if (r->fields.is_nil()) {
        csv_error("iterator has already advanced the reader");
        return Row::Error;
    }
    r->line_num++;
    Str s    = str_of(rl.v)->str();
    usize at = 0;
    while (at < s.size()) {
        u32 cp = 0;
        at += cp_decode(s, at, cp);
        if (parse_process_char(rv, cp) != R::Ok)
            return Row::Error;
    }
    if (parse_process_char(rv, EOL) != R::Ok)
        return Row::Error;
    return reader_of(rv)->state == START_RECORD ? Row::Done : Row::More;
}

// The input is over: the row it ends, an error, or NotImpl for the end.
R input_ended(Value rv, Value &out)
{
    ReaderObj *r = reader_of(rv);
    if (!r->field.empty() || r->state == IN_QUOTED_FIELD) {
        if (dialect_of(r->dialect)->strict)
            return csv_error("unexpected end of data");
        if (parse_save_field(rv) != R::Ok)
            return R::Err;
        out                   = reader_of(rv)->fields;
        reader_of(rv)->fields = Value();
        return R::Ok;
    }
    return R::NotImpl;
}

R stop_iteration()
{
    Value e = exc_new(exc_find("StopIteration"), Value());
    return e.is_nil() ? R::Err : err_set_value(e);
}

R take_row(Value rv, Value &out)
{
    out                   = reader_of(rv)->fields;
    reader_of(rv)->fields = Value();
    return R::Ok;
}

// s[0] the reader, s[1] the input's __next__.
R reader_step(ContObj *k, Value in)
{
    for (;;) {
        if (k->i == 0) {
            k->i        = 1;
            k->catching = CATCH_STOP;
            return cont_call(k, k->s[1], Value(), 0);
        }
        k->catching = CATCH_NONE;
        if (in.is_nil()) {
            k->caught = Value();
            Value row;
            R r = input_ended(k->s[0], row);
            if (r == R::NotImpl)
                return stop_iteration();
            if (r != R::Ok)
                return R::Err;
            return cont_done(k, row);
        }
        switch (feed_line(k->s[0], in)) {
        case Row::Error:
            return R::Err;
        case Row::Done: {
            Value row;
            take_row(k->s[0], row);
            return cont_done(k, row);
        }
        case Row::More:
            k->i = 0;
            in   = Value();
            continue;
        }
    }
}

R reader_next_any(Value rv, Value &out)
{
    Root r{ rv };
    if (parse_reset(r.v) != R::Ok)
        return R::Err;
    Value input = reader_of(r.v)->input;
    if (iter_needs_vm(input)) {
        Root m{ next_special(input) };
        if (m.v.is_nil())
            return err_pending() ? R::Err : err_set("TypeError", "reader input cannot be stepped");
        Root kv{ cont_new(reader_step) };
        if (kv.v.is_nil())
            return R::Err;
        cont_of(kv.v)->s[0] = r.v;
        cont_of(kv.v)->s[1] = m.v;
        out                 = kv.v;
        return R::Ok;
    }
    for (;;) {
        Root line;
        R got = py_next(reader_of(r.v)->input, line.v);
        if (got == R::Err)
            return R::Err;
        if (got == R::NotImpl)
            return input_ended(r.v, out);
        switch (feed_line(r.v, line.v)) {
        case Row::Error:
            return R::Err;
        case Row::Done:
            return take_row(r.v, out);
        case Row::More:
            break;
        }
    }
}

R reader_next(Value v, Value &out)
{
    return reader_next_any(v, out);
}

Value reader_iter(Value v)
{
    return v;
}

R rm_next(const CallArgs &a, Value &out)
{
    if (!a.nargs || !a.args[0].is_obj() || a.args[0].obj()->type != &reader_type)
        return err_set("TypeError", "a _csv.reader is required");
    R r = reader_next_any(a.args[0], out);
    return r == R::NotImpl ? stop_iteration() : r;
}

constexpr Method READER_METHODS[] = {
    { "__next__", rm_next },
};

R reader_getattr(Value v, StrObj *name, Value &out)
{
    ReaderObj *r = reader_of(v);
    if (name->str() == "dialect") {
        out = r->dialect;
        return R::Ok;
    }
    if (name->str() == "line_num") {
        out = int_from_i64(r->line_num);
        return R::Ok;
    }
    return R::NotImpl;
}

constexpr Type reader_type{ .name    = "_csv.reader",
                            .trace   = reader_trace,
                            .fini    = reader_fini,
                            .iter    = reader_iter,
                            .next    = reader_next,
                            .getattr = reader_getattr,
                            .vmnext  = true };

R b_reader(const CallArgs &a, Value &out)
{
    if (!a.nargs)
        return err_set("TypeError", "reader expected at least 1 argument, got 0");
    if (iter_needs_vm(a.args[0]) && type_has_py_special(a.args[0], "__iter__"))
        return iter_park(a, 0, b_reader, out);
    Root it{ py_iter(a.args[0]) };
    if (it.v.is_nil())
        return R::Err;
    Root dialect;
    if (dialect_from_call(a, 1, "reader", dialect.v) != R::Ok)
        return R::Err;
    ReaderObj *r = static_cast<ReaderObj *>(obj_alloc(&reader_type, sizeof(ReaderObj)));
    if (!r)
        return oom();
    new (&r->field) Vec<u32>();
    r->input    = it.v;
    r->dialect  = dialect.v;
    r->fields   = Value();
    r->line_num = 0;
    r->state    = START_RECORD;
    r->unquoted = false;
    Root rr{ obj_value(r) };
    if (parse_reset(rr.v) != R::Ok)
        return R::Err;
    out = rr.v;
    return R::Ok;
}

// ------------------------------------------------------------------ writer

struct WriterObj : Obj {
    Value write;
    Value dialect;
};

extern const Type writer_type;

WriterObj *writer_of(Value v)
{
    return static_cast<WriterObj *>(v.obj());
}

void writer_trace(Obj *o)
{
    gc_mark(static_cast<WriterObj *>(o)->write);
    gc_mark(static_cast<WriterObj *>(o)->dialect);
}

bool is_special(const DialectObj *d, u32 c)
{
    return c == d->delimiter || c == d->quotechar || c == d->escapechar || c == '\r' ||
           c == '\n' || str_contains_cp(d->lineterminator, c);
}

// One field onto the record: `field` a str, or Nil for None.
R join_append(const DialectObj *d, String &rec, u32 &nfields, Value field, bool quoted)
{
    Str text = field.is_nil() ? Str() : str_of(field)->str();
    if (text.empty() && d->delimiter == ' ' && d->skipinitialspace) {
        if (d->quoting == QUOTE_NONE ||
            (field.is_nil() && (d->quoting == QUOTE_STRINGS || d->quoting == QUOTE_NOTNULL)))
            return csv_error("empty field must be quoted if delimiter is a space and "
                             "skipinitialspace is true");
        quoted = true;
    }
    String body;
    usize at = 0;
    while (at < text.size()) {
        u32 c = 0;
        at += cp_decode(text, at, c);
        bool escape = false;
        if (is_special(d, c)) {
            if (d->quoting == QUOTE_NONE) {
                escape = true;
            } else {
                if (c == d->quotechar) {
                    if (d->doublequote) {
                        if (!cp_append(body, d->quotechar))
                            return oom();
                    } else {
                        escape = true;
                    }
                } else if (c == d->escapechar) {
                    escape = true;
                }
                if (!escape)
                    quoted = true;
            }
            if (escape) {
                if (d->escapechar == NOT_SET)
                    return csv_error("need to escape, but no escapechar set");
                if (!cp_append(body, d->escapechar))
                    return oom();
            }
        }
        if (!cp_append(body, c))
            return oom();
    }
    if (nfields > 0 && !cp_append(rec, d->delimiter))
        return oom();
    if (quoted && !cp_append(rec, d->quotechar))
        return oom();
    if (!rec.append(body.str()))
        return oom();
    if (quoted && !cp_append(rec, d->quotechar))
        return oom();
    nfields++;
    return R::Ok;
}

bool is_number_like(Value v)
{
    if (v.is_int() || is_bool(v) || is_float(v) || is_intval(v))
        return true;
    if (v.is_obj() && type_name(v) == "complex")
        return true;
    return type_has_special(v, "__index__") || type_has_special(v, "__int__") ||
           type_has_special(v, "__float__");
}

// The record for a row whose fields are already in `fields`: each a str,
// None, or what str() made of it. `kinds` says which were str and None.
R build_line(Value dv, ListObj *fields, const Vec<u8> &kinds, Value &out)
{
    const DialectObj *d = dialect_of(dv);
    String rec;
    u32 n        = 0;
    bool wasnull = false;
    for (usize i = 0; i < fields->items.size(); i++) {
        u8 kind = kinds[i]; // 0 str, 1 None, 2 other, 3 number
        bool quoted;
        switch (d->quoting) {
        case QUOTE_NONNUMERIC:
            quoted = kind != 3;
            break;
        case QUOTE_ALL:
            quoted = true;
            break;
        case QUOTE_STRINGS:
            quoted = kind == 0;
            break;
        case QUOTE_NOTNULL:
            quoted = kind != 1;
            break;
        default:
            quoted = false;
        }
        wasnull = kind == 1;
        if (join_append(d, rec, n, kind == 1 ? Value() : fields->items[i], quoted) != R::Ok)
            return R::Err;
    }
    if (n > 0 && rec.empty()) {
        if (d->quoting == QUOTE_NONE ||
            (wasnull && (d->quoting == QUOTE_STRINGS || d->quoting == QUOTE_NOTNULL)))
            return csv_error("single empty field record must be quoted");
        n--;
        if (join_append(d, rec, n, Value(), true) != R::Ok)
            return R::Err;
    }
    if (!rec.append(str_of(d->lineterminator)->str()))
        return oom();
    out = str_new(rec.str());
    return out.is_nil() ? R::Err : R::Ok;
}

// A row's fields as the writer wants them: s[1] the row as a list, s[2] the
// converted fields, s[3] their kinds as a bytearray; j the next.
R writerow_step(ContObj *k, Value in)
{
    ListObj *row = list_of(k->s[1]);
    if (k->i == 1) {
        // str() of a field, written in Python, has answered.
        if (!is_str(in))
            return err_set2("TypeError", "__str__ returned non-string", type_name(in));
        if (!list_push(list_of(k->s[2]), in))
            return oom();
        k->i = 0;
    } else if (k->i == 2) {
        return cont_done(k, in);
    }
    while (k->j < row->items.size()) {
        Value f   = row->items[k->j++];
        ArrayObj *kinds = array_of(k->s[3]);
        if (is_str(f)) {
            if (!kinds->data.push(0) || !list_push(list_of(k->s[2]), f))
                return oom();
            continue;
        }
        if (is_none(f)) {
            if (!kinds->data.push(1) || !list_push(list_of(k->s[2]), f))
                return oom();
            continue;
        }
        if (!kinds->data.push(is_number_like(f) ? 3 : 2))
            return oom();
        Root rf{ f };
        Root m{ show_special(rf.v, true) };
        if (!m.v.is_nil()) {
            k->i = 1;
            return cont_await(k, m.v);
        }
        if (err_pending())
            return R::Err;
        String s;
        if (py_str(rf.v, s) != R::Ok)
            return R::Err;
        Value sv = str_new(s.str());
        if (sv.is_nil() || !list_push(list_of(k->s[2]), sv))
            return err_pending() ? R::Err : oom();
    }
    Root line;
    Vec<u8> kinds;
    for (u8 b : array_of(k->s[3])->data)
        if (!kinds.push(b))
            return oom();
    if (build_line(writer_of(k->s[0])->dialect, list_of(k->s[2]), kinds, line.v) != R::Ok)
        return R::Err;
    k->i = 2;
    return cont_call(k, writer_of(k->s[0])->write, line.v);
}

R writerow_start(Value w, Value row, Value &out)
{
    Root rw{ w }, rr{ row };
    Root lst{ obj_value(py_list_of(rr.v)) };
    if (lst.v.is_nil()) {
        if (err_kind() == "TypeError") {
            err_clear();
            Buf<128> m;
            m.put("iterable expected, not ").put(type_name(rr.v));
            return csv_error(m.str());
        }
        return R::Err;
    }
    Root conv{ obj_value(list_new()) };
    Root kinds{ bytearray_new(Str()) };
    if (conv.v.is_nil() || kinds.v.is_nil())
        return err_pending() ? R::Err : oom();
    Root kv{ cont_new(writerow_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rw.v;
    k->s[1]    = lst.v;
    k->s[2]    = conv.v;
    k->s[3]    = kinds.v;
    out        = kv.v;
    return R::Ok;
}

WriterObj *self_writer(const CallArgs &a, Str who)
{
    if (!a.nargs || !a.args[0].is_obj() || a.args[0].obj()->type != &writer_type) {
        err_set2("TypeError", "a _csv.writer is required", who);
        return nullptr;
    }
    return writer_of(a.args[0]);
}

R wm_writerow(const CallArgs &a, Value &out)
{
    if (!self_writer(a, "writerow") || !meth_args(a, "writerow", 1, 1))
        return R::Err;
    if (iter_needs_vm(a.args[1]))
        return iter_park(a, 1, wm_writerow, out);
    return writerow_start(a.args[0], a.args[1], out);
}

// s[0] the writer, s[1] the rows, a list; j the next.
R writerows_step(ContObj *k, Value)
{
    ListObj *rows = list_of(k->s[1]);
    if (k->j >= rows->items.size())
        return cont_done(k, value_none());
    Value row = rows->items[k->j++];
    Root rr{ row };
    if (iter_needs_vm(rr.v)) {
        // A row that is itself a generator is drained first.
        Value args[2] = { k->s[0], rr.v };
        CallArgs ca;
        ca.args  = args;
        ca.nargs = 2;
        Value w;
        if (wm_writerow(ca, w) != R::Ok)
            return R::Err;
        return cont_await(k, w);
    }
    Value w;
    if (writerow_start(k->s[0], rr.v, w) != R::Ok)
        return R::Err;
    return cont_await(k, w);
}

R wm_writerows(const CallArgs &a, Value &out)
{
    if (!self_writer(a, "writerows") || !meth_args(a, "writerows", 1, 1))
        return R::Err;
    if (iter_needs_vm(a.args[1]))
        return iter_park(a, 1, wm_writerows, out);
    Root rows{ obj_value(py_list_of(a.args[1])) };
    if (rows.v.is_nil())
        return R::Err;
    Root kv{ cont_new(writerows_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = a.args[0];
    cont_of(kv.v)->s[1] = rows.v;
    out                 = kv.v;
    return R::Ok;
}

constexpr Method WRITER_METHODS[] = {
    { "writerow", wm_writerow },
    { "writerows", wm_writerows },
};

R writer_getattr(Value v, StrObj *name, Value &out)
{
    if (name->str() == "dialect") {
        out = writer_of(v)->dialect;
        return R::Ok;
    }
    return R::NotImpl;
}

constexpr Type writer_type{ .name    = "_csv.writer",
                            .trace   = writer_trace,
                            .getattr = writer_getattr };

R b_writer(const CallArgs &a, Value &out)
{
    if (!a.nargs)
        return err_set("TypeError", "writer expected at least 1 argument, got 0");
    StrObj *wn = str_intern("write");
    if (!wn)
        return oom();
    Root write;
    Got g = py_attr(a.args[0], wn, write.v);
    if (g == Got::Error)
        return R::Err;
    if (g != Got::Ok || !py_callable(write.v))
        return err_set("TypeError", "argument 1 must have a \"write\" method");
    Root dialect;
    if (dialect_from_call(a, 1, "writer", dialect.v) != R::Ok)
        return R::Err;
    WriterObj *w = static_cast<WriterObj *>(obj_alloc(&writer_type, sizeof(WriterObj)));
    if (!w)
        return oom();
    w->write   = write.v;
    w->dialect = dialect.v;
    out        = obj_value(w);
    return R::Ok;
}

// ------------------------------------------------------------ the registry

DictObj *registry()
{
    Home *h = here();
    return h ? static_cast<DictObj *>(h->dialects.obj()) : nullptr;
}

R b_register_dialect(const CallArgs &a, Value &out)
{
    if (!a.nargs)
        return err_set("TypeError", "register_dialect expected at least 1 argument, got 0");
    if (!is_str(a.args[0]))
        return err_set("TypeError", "dialect name must be a string");
    Root name{ a.args[0] };
    Root d;
    if (dialect_from_call(a, 1, "register_dialect", d.v) != R::Ok)
        return R::Err;
    if (dict_set(registry(), name.v, d.v) != R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

R b_unregister_dialect(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "name" };
    Value v[1];
    if (!fn_take(a, "unregister_dialect", NAMES, 1, v))
        return R::Err;
    R r = dict_del(registry(), v[0]);
    if (r == R::Err)
        return R::Err;
    if (r == R::NotImpl)
        return csv_error("unknown dialect");
    out = value_none();
    return R::Ok;
}

R b_get_dialect(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "name" };
    Value v[1];
    if (!fn_take(a, "get_dialect", NAMES, 1, v))
        return R::Err;
    R r = dict_get(registry(), v[0], out);
    if (r == R::NotImpl)
        return csv_error("unknown dialect");
    return r;
}

R b_list_dialects(const CallArgs &a, Value &out)
{
    if (!args_only(a, "list_dialects", 0, 0))
        return R::Err;
    ListObj *l = list_new();
    if (!l)
        return oom();
    Root rl{ obj_value(l) };
    usize at = 0;
    Value k, x;
    while (table_next(registry()->t, at, k, x))
        if (!list_push(list_of(rl.v), k))
            return oom();
    out = rl.v;
    return R::Ok;
}

R b_field_size_limit(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "new_limit" };
    Value v[1];
    if (!fn_take(a, "field_size_limit", NAMES, 0, v))
        return R::Err;
    Home *h = here();
    i64 old = h->field_limit;
    if (!v[0].is_nil()) {
        i64 n = 0;
        if (!(v[0].is_int() || (v[0].is_obj() && v[0].obj()->type == &int_type)) ||
            !as_index(v[0], n))
            return err_set("TypeError", "limit must be an integer");
        h->field_limit = n;
    }
    out = int_from_i64(old);
    return R::Ok;
}

constexpr ModDef DEFS[] = {
    { "reader", b_reader },
    { "writer", b_writer },
    { "register_dialect", b_register_dialect },
    { "unregister_dialect", b_unregister_dialect },
    { "get_dialect", b_get_dialect },
    { "list_dialects", b_list_dialects },
    { "field_size_limit", b_field_size_limit },
};

Value error_class()
{
    Root base{ exc_type_value(exc_find("Exception")) };
    TupleObj *bases = base.v.is_nil() ? nullptr : tuple_new(1);
    if (!bases)
        return err_pending() ? Value() : (oom(), Value());
    bases->items()[0] = base.v;
    Root rb{ obj_value(bases) };
    DictObj *d = dict_new();
    if (!d)
        return oom(), Value();
    Root rd{ obj_value(d) };
    Root mod{ str_new("_csv") };
    Root name{ str_new("Error") };
    StrObj *key = str_intern("__module__");
    if (mod.v.is_nil() || name.v.is_nil() || !key ||
        dict_set(static_cast<DictObj *>(rd.v.obj()), obj_value(key), mod.v) != R::Ok)
        return err_pending() ? Value() : (oom(), Value());
    return type_new(name.v, rb.v, rd.v);
}

} // namespace

bool csv_install(DictObj *into)
{
    Home *h = here();
    if (!h)
        return oom() == R::Ok;
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (h->error.is_nil()) {
        h->error = error_class();
        if (h->error.is_nil())
            return false;
    }
    if (h->dialects.is_nil()) {
        DictObj *reg = dict_new();
        if (!reg)
            return oom() == R::Ok;
        h->dialects = obj_value(reg);
    }
    if (!method_install(&dialect_type, DIALECT_METHODS) ||
        !method_install(&reader_type, READER_METHODS) ||
        !method_install(&writer_type, WRITER_METHODS))
        return false;
    if (!mod_defs(d, DEFS) || !mod_type(d, &dialect_type, b_dialect) ||
        !mod_put(d, "Error", h->error) || !mod_put(d, "_dialects", h->dialects))
        return false;
    Root rt{ type_wrap(&reader_type) };
    Root wt{ type_wrap(&writer_type) };
    if (rt.v.is_nil() || wt.v.is_nil() || !mod_put(d, "Reader", rt.v) ||
        !mod_put(d, "Writer", wt.v))
        return false;
    for (const StyleDesc &s : QUOTE_STYLES)
        if (!mod_int(d, s.name, s.style))
            return false;
    return true;
}
