// `_string`: str.format's grammar, as string.Formatter reads it.
//
// formatter_parser and formatter_field_name_split are CPython's
// MarkupIterator and FieldNameIterator, step for step, including where each
// reports an error: at the step that reaches it, not at the call.
#include "gc.h"
#include "kernel/fmt.h"
#include "module.h"
#include "ops.h"
#include "type.h"
#include "ucd.h"
#include "ustr.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// s the str, at the next byte to read.
struct ScanObj : Obj {
    Value s;
    usize at;
};

void scan_trace(Obj *o)
{
    gc_mark(static_cast<ScanObj *>(o)->s);
}

Value scan_self(Value v)
{
    return v;
}

Str text_of(Value v)
{
    return str_of(static_cast<ScanObj *>(v.obj())->s)->str();
}

Value tuple_of(Value *items, usize n)
{
    TupleObj *t = tuple_new(n);
    if (!t)
        return oom(), Value();
    for (usize i = 0; i < n; i++)
        t->items()[i] = items[i];
    return obj_value(t);
}

// The decimal a field name or key spells, -1 for none.
R get_integer(Str s, i64 &out)
{
    out = -1;
    if (!s.size())
        return R::Ok;
    i64 acc = 0;
    for (usize i = 0; i < s.size();) {
        u32 cp = 0;
        i += cp_decode(s, i, cp);
        int d = ucd_decimal(cp);
        if (d < 0)
            return R::Ok;
        if (acc > (i64(0x7fffffffffffffff) - d) / 10)
            return err_set("ValueError", "Too many decimal digits in format string");
        acc = acc * 10 + d;
    }
    out = acc;
    return R::Ok;
}

Value index_or_str(Str s)
{
    i64 n = 0;
    if (get_integer(s, n) != R::Ok)
        return Value();
    return n >= 0 ? int_from_i64(n) : str_new(s);
}

// ------------------------------------------------------------ the fields

struct Field {
    Str name, spec;
    u32 conv = 0;
};

// The field after a '{', up to its '}'.
R parse_field(Str t, usize &at, Field &f)
{
    char c      = 0;
    usize start = at;
    while (at < t.size()) {
        c = t[at++];
        if (c == '{')
            return err_set("ValueError", "unexpected '{' in field name");
        if (c == '[') {
            while (at < t.size() && t[at] != ']')
                at++;
            continue;
        }
        if (c == '}' || c == ':' || c == '!')
            break;
    }
    f.name = t.substr(start, at - 1 - start);
    if (c != '!' && c != ':') {
        if (c != '}')
            return err_set("ValueError", "expected '}' before end of string");
        return R::Ok;
    }
    if (c == '!') {
        if (at >= t.size())
            return err_set("ValueError", "end of string while looking for conversion specifier");
        at += cp_decode(t, at, f.conv);
        if (at < t.size()) {
            c = t[at++];
            if (c == '}')
                return R::Ok;
            if (c != ':')
                return err_set("ValueError", "expected ':' after conversion specifier");
        }
    }
    start     = at;
    u32 count = 1;
    while (at < t.size()) {
        c = t[at++];
        if (c == '{') {
            count++;
        } else if (c == '}' && --count == 0) {
            f.spec = t.substr(start, at - 1 - start);
            return R::Ok;
        }
    }
    return err_set("ValueError", "unmatched '{' in format spec");
}

R markup_next(Value v, Value &out)
{
    ScanObj *o = static_cast<ScanObj *>(v.obj());
    Str t      = text_of(v);
    if (o->at >= t.size())
        return R::NotImpl;
    usize start = o->at;
    usize at    = o->at;
    char c      = 0;
    bool markup = false;
    while (at < t.size()) {
        c = t[at++];
        if (c == '{' || c == '}') {
            markup = true;
            break;
        }
    }
    bool at_end = at >= t.size();
    usize len   = at - start;
    if (c == '}' && (at_end || t[at] != c))
        return err_set("ValueError", "Single '}' encountered in format string");
    if (at_end && c == '{')
        return err_set("ValueError", "Single '{' encountered in format string");
    if (!at_end) {
        if (t[at] == c) {
            at++;
            markup = false;
        } else {
            len--;
        }
    }
    Str literal = t.substr(start, len);
    Field f;
    if (markup && parse_field(t, at, f) != R::Ok)
        return R::Err;
    o->at = at;

    Root items[4];
    items[0] = str_new(literal);
    if (items[0].v.is_nil())
        return R::Err;
    if (!markup) {
        for (usize i = 1; i < 4; i++)
            items[i] = value_none();
    } else {
        items[1] = str_new(f.name);
        items[2] = str_new(f.spec);
        if (f.conv) {
            char b[4];
            items[3] = str_new(Str(b, cp_encode(f.conv, b)));
        } else {
            items[3] = value_none();
        }
        if (items[1].v.is_nil() || items[2].v.is_nil() || items[3].v.is_nil())
            return R::Err;
    }
    Value vals[4] = { items[0].v, items[1].v, items[2].v, items[3].v };
    out           = tuple_of(vals, 4);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Type markup_type{ .name  = "formatteriterator",
                            .trace = scan_trace,
                            .iter  = scan_self,
                            .next  = markup_next,
                            .final = true };

// ------------------------------------------------------- the name's parts

R fieldname_next(Value v, Value &out)
{
    ScanObj *o = static_cast<ScanObj *>(v.obj());
    Str t      = text_of(v);
    if (o->at >= t.size())
        return R::NotImpl;
    usize at    = o->at;
    char c      = t[at++];
    bool attr   = c == '.';
    usize start = at;
    Str name;
    if (attr) {
        while (at < t.size() && t[at] != '.' && t[at] != '[')
            at++;
        name = t.substr(start, at - start);
    } else if (c == '[') {
        while (at < t.size() && t[at] != ']')
            at++;
        if (at >= t.size())
            return err_set("ValueError", "Missing ']' in format string");
        name = t.substr(start, at - start);
        at++;
    } else {
        return err_set("ValueError", "Only '.' or '[' may follow ']' in format field specifier");
    }
    Root key{ attr ? str_new(name) : index_or_str(name) };
    if (key.v.is_nil())
        return R::Err;
    if (!name.size())
        return err_set("ValueError", "Empty attribute in format string");
    o->at         = at;
    Value vals[2] = { value_bool(attr), key.v };
    out           = tuple_of(vals, 2);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Type fieldname_type{ .name  = "fieldnameiterator",
                               .trace = scan_trace,
                               .iter  = scan_self,
                               .next  = fieldname_next,
                               .final = true };

Value scan_new(const Type *t, Value s, usize at)
{
    Root rs{ s };
    ScanObj *o = static_cast<ScanObj *>(obj_alloc(t, sizeof(ScanObj)));
    if (!o)
        return oom(), Value();
    o->s  = rs.v;
    o->at = at;
    return obj_value(o);
}

bool want_str(const CallArgs &a, Str who)
{
    if (!args_only(a, who, 1, 1))
        return false;
    if (is_str(a.args[0]))
        return true;
    Buf<96> b;
    b.put("expected str, got ").put(type_name(a.args[0]));
    return err_set("TypeError", b.str()) == R::Ok;
}

R b_formatter_parser(const CallArgs &a, Value &out)
{
    if (!want_str(a, "formatter_parser"))
        return R::Err;
    out = scan_new(&markup_type, a.args[0], 0);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_formatter_field_name_split(const CallArgs &a, Value &out)
{
    if (!want_str(a, "formatter_field_name_split"))
        return R::Err;
    Str t   = str_of(a.args[0])->str();
    usize i = 0;
    while (i < t.size() && t[i] != '.' && t[i] != '[')
        i++;
    Root first{ index_or_str(t.substr(0, i)) };
    if (first.v.is_nil())
        return R::Err;
    Root rest{ scan_new(&fieldname_type, a.args[0], i) };
    if (rest.v.is_nil())
        return R::Err;
    Value vals[2] = { first.v, rest.v };
    out           = tuple_of(vals, 2);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr ModDef STRING_DEFS[] = {
    { "formatter_parser", b_formatter_parser },
    { "formatter_field_name_split", b_formatter_field_name_split },
};

} // namespace

bool string_install(DictObj *into)
{
    return mod_defs(into, STRING_DEFS);
}
