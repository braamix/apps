// The two grammars that stand on the format spec: str.format's replacement
// fields, and `%`.
//
// Both are built in two passes, which is what ground rule 2 asks for. The
// first is plain C++ and reads the template into a *plan* -- a flat list of
// steps, each either text to emit or a value to format. The second is a
// continuation that walks the plan: a step that needs Python asks for one
// call and comes back with the answer, and a step that does not is done on
// the spot. Nothing about the template is looked at twice, and no C++ frame
// holds anything across a call.
//
// The plan is a list of four values per step:
//
//   LIT      text      -- append text to the output
//   FMT      value spec conv -- append format(value, spec) to the output
//   SPECLIT  text      -- append text to the spec being built
//   SPECFMT  value     -- append format(value) to the spec being built
//   FMTSPEC  value conv     -- format by the spec built so far, then clear it
//   CALL     fn arg    -- call it and keep the answer; the FMT after this one
//                        has no value of its own and formats that instead.
//                        This is what `%d` of a class asking __int__, and
//                        format_map over a mapping written in Python, both
//                        need: a value the plan cannot have until it runs.
#include "bigint.h"
#include "builtin.h"
#include "call.h"
#include "format.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "math/math.h"
#include "method.h"
#include "ops.h"
#include "type.h"

namespace {

enum : i64 { LIT, FMT, SPECLIT, SPECFMT, FMTSPEC, CALL };

constexpr usize STEP = 5; // kind, value, spec, conv, min_digits

R oom()
{
    return err_set("MemoryError", "out of memory");
}

bool step_push(ListObj *plan, i64 kind, Value a, Value b, Value c, Value d = Value())
{
    Root ra{ a }, rb{ b }, rc{ c }, rd{ d };
    Value k = int_from_i64(kind);
    if (k.is_nil() || !list_push(plan, k))
        return false;
    return list_push(plan, ra.v) && list_push(plan, rb.v) && list_push(plan, rc.v) &&
           list_push(plan, rd.v);
}

bool step_text(ListObj *plan, i64 kind, Str text)
{
    Value s = str_new(text);
    if (s.is_nil())
        return false;
    return step_push(plan, kind, s, Value(), Value());
}

bool is_dig(char c)
{
    return c >= '0' && c <= '9';
}

// ------------------------------------------------- str.format's field syntax

// `name` as an argument: a number is positional, anything else a keyword.
// Auto-numbering fills in for an empty one, and mixing the two is refused.
R field_arg(ListObj *plan, Str name, Value args, Value kwargs, u32 &autonum, i32 &mode,
            bool &deferred, Value &out)
{
    if (!name.size()) {
        if (mode < 0)
            return err_set("ValueError",
                           "cannot switch from manual field specification to automatic "
                           "field numbering");
        mode      = 1;
        usize n   = static_cast<TupleObj *>(args.obj())->len;
        u32 index = autonum++;
        if (index >= n) {
            Buf<96> b;
            char t[24];
            b.put("Replacement index ").put(int_text(t, sizeof t, i64(index)));
            b.put(" out of range for positional args tuple");
            return err_set("IndexError", b.str());
        }
        out = static_cast<TupleObj *>(args.obj())->items()[index];
        return R::Ok;
    }

    bool numeric = true;
    for (usize i = 0; i < name.size(); i++)
        if (!is_dig(name[i]))
            numeric = false;

    if (numeric) {
        if (mode > 0)
            return err_set("ValueError",
                           "cannot switch from automatic field numbering to manual field "
                           "specification");
        mode  = -1;
        i64 v = 0;
        for (usize i = 0; i < name.size(); i++)
            v = v * 10 + (name[i] - '0');
        usize n = static_cast<TupleObj *>(args.obj())->len;
        if (v < 0 || usize(v) >= n) {
            Buf<96> b;
            char t[24];
            b.put("Replacement index ").put(int_text(t, sizeof t, v));
            b.put(" out of range for positional args tuple");
            return err_set("IndexError", b.str());
        }
        out = static_cast<TupleObj *>(args.obj())->items()[usize(v)];
        return R::Ok;
    }

    if (kwargs.is_nil())
        return err_set2("KeyError", "no keyword arguments", name);
    Root key{ str_new(name) };
    if (key.v.is_nil())
        return R::Err;
    // A mapping of one's own answers __getitem__ in Python, which no plan can
    // do while it is being built: the lookup becomes a step.
    if (is_inst(kwargs)) {
        Root m{ type_special(kwargs, "__getitem__") };
        if (!m.v.is_nil()) {
            if (!step_push(plan, CALL, m.v, key.v, Value()))
                return oom();
            deferred = true;
            return R::Ok;
        }
    }
    return py_getitem(kwargs, key.v, out);
}

// `.attr` and `[index]` after the argument name, applied left to right. An
// index of digits alone is an integer; anything else is a string key, and it
// is not quoted.
R field_trail(Str trail, Value base, Value &out)
{
    Root cur{ base };
    usize i = 0;
    while (i < trail.size()) {
        if (trail[i] == '.') {
            usize j = ++i;
            while (j < trail.size() && trail[j] != '.' && trail[j] != '[')
                j++;
            if (j == i)
                return err_set("ValueError", "Empty attribute in format string");
            StrObj *name = str_intern(trail.substr(i, j - i));
            if (!name)
                return oom();
            Value got;
            if (py_getattr(cur.v, name, got) != R::Ok)
                return R::Err;
            cur = got;
            i   = j;
            continue;
        }
        if (trail[i] != '[')
            return err_set("ValueError", "Only '.' or '[' may follow ']' in format string");
        usize j = ++i;
        while (j < trail.size() && trail[j] != ']')
            j++;
        if (j >= trail.size())
            return err_set("ValueError", "Missing ']' in format string");
        Str key   = trail.substr(i, j - i);
        bool nums = key.size() > 0;
        for (usize k = 0; k < key.size(); k++)
            if (!is_dig(key[k]))
                nums = false;
        Root kv{ nums ? Value() : str_new(key) };
        if (nums) {
            i64 v = 0;
            for (usize k = 0; k < key.size(); k++)
                v = v * 10 + (key[k] - '0');
            kv = int_from_i64(v);
        }
        if (kv.v.is_nil())
            return R::Err;
        Value got;
        if (py_getitem(cur.v, kv.v, got) != R::Ok)
            return R::Err;
        cur = got;
        i   = j + 1;
    }
    out = cur.v;
    return R::Ok;
}

// Where a `{` that starts at `at` is closed, counting nested braces.
usize brace_end(Str s, usize at)
{
    u32 depth = 0;
    for (usize k = at; k < s.size(); k++) {
        if (s[k] == '{')
            depth++;
        else if (s[k] == '}') {
            if (!depth)
                return k;
            depth--;
        }
    }
    return Str::npos;
}

// The top-level `!` or `:` of a field, or npos. A `[` hides both, since a key
// may hold anything.
usize field_mark(Str f, char want)
{
    u32 depth = 0;
    for (usize k = 0; k < f.size(); k++) {
        char c = f[k];
        if (c == '[')
            depth++;
        else if (c == ']') {
            if (depth)
                depth--;
        } else if (!depth && c == want) {
            if (want == '!' && k + 1 < f.size() && f[k + 1] == '=')
                continue;
            return k;
        }
    }
    return Str::npos;
}

// A format spec of str.format's, which may itself hold replacement fields --
// one level, which is all CPython allows.
R plan_spec(ListObj *plan, Str spec, Value args, Value kwargs, u32 &autonum, i32 &mode)
{
    usize at = 0, run = 0;
    for (;;) {
        bool end = at == spec.size();
        char c   = end ? 0 : spec[at];
        if (!end && c != '{' && c != '}') {
            at++;
            continue;
        }
        if (at > run && !step_text(plan, SPECLIT, spec.substr(run, at - run)))
            return oom();
        if (end)
            return R::Ok;
        if (at + 1 < spec.size() && spec[at + 1] == c) {
            if (!step_text(plan, SPECLIT, spec.substr(at, 1)))
                return oom();
            at += 2;
            run = at;
            continue;
        }
        if (c == '}')
            return err_set("ValueError", "Single '}' encountered in format string");

        usize stop = brace_end(spec, at + 1);
        if (stop == Str::npos)
            return err_set("ValueError", "unmatched '{' in format spec");
        Str inner = spec.substr(at + 1, stop - at - 1);
        if (field_mark(inner, ':') != Str::npos)
            return err_set("ValueError", "Max string recursion exceeded");

        Str name   = inner;
        usize bang = field_mark(inner, '!');
        if (bang != Str::npos)
            return err_set("ValueError", "conversions are not allowed in a nested spec");

        usize head = 0;
        while (head < name.size() && name[head] != '.' && name[head] != '[')
            head++;
        Value base;
        bool deferred = false;
        if (field_arg(plan, name.substr(0, head), args, kwargs, autonum, mode, deferred, base) !=
            R::Ok)
            return R::Err;
        if (deferred && head != name.size())
            return err_set("ValueError", "a mapping of one's own cannot be walked into");
        Root rb{ base };
        Value v;
        if (!deferred && field_trail(name.substr(head), rb.v, v) != R::Ok)
            return R::Err;
        if (!step_push(plan, SPECFMT, v, Value(), Value()))
            return oom();
        at  = stop + 1;
        run = at;
    }
}

// The whole template, read into a plan.
R plan_format(ListObj *plan, Str t, Value args, Value kwargs)
{
    u32 autonum = 0;
    i32 mode    = 0; // 1 automatic, -1 manual, 0 undecided
    usize at = 0, run = 0;
    for (;;) {
        bool end = at == t.size();
        char c   = end ? 0 : t[at];
        if (!end && c != '{' && c != '}') {
            at++;
            continue;
        }
        if (at > run && !step_text(plan, LIT, t.substr(run, at - run)))
            return oom();
        if (end)
            return R::Ok;
        if (at + 1 < t.size() && t[at + 1] == c) {
            if (!step_text(plan, LIT, t.substr(at, 1)))
                return oom();
            at += 2;
            run = at;
            continue;
        }
        if (c == '}')
            return err_set("ValueError", "Single '}' encountered in format string");

        usize stop = brace_end(t, at + 1);
        if (stop == Str::npos)
            return err_set("ValueError", "expected '}' before end of string");
        Str field = t.substr(at + 1, stop - at - 1);

        Str name = field;
        Str spec;
        bool has_spec = false;
        i64 conv      = CONV_NONE;

        usize colon = field_mark(field, ':');
        if (colon != Str::npos) {
            name     = field.substr(0, colon);
            spec     = field.substr(colon + 1);
            has_spec = true;
        }
        usize bang = field_mark(name, '!');
        if (bang != Str::npos) {
            Str w = name.substr(bang + 1);
            if (w.size() != 1 || (w[0] != 's' && w[0] != 'r' && w[0] != 'a')) {
                Buf<96> b;
                b.put("Unknown conversion specifier ").put(w);
                return err_set("ValueError", b.str());
            }
            conv = w[0];
            name = name.substr(0, bang);
        }

        usize head = 0;
        while (head < name.size() && name[head] != '.' && name[head] != '[')
            head++;
        Value base;
        bool deferred = false;
        if (field_arg(plan, name.substr(0, head), args, kwargs, autonum, mode, deferred, base) !=
            R::Ok)
            return R::Err;
        if (deferred && head != name.size())
            return err_set("ValueError", "a mapping of one's own cannot be walked into");
        Root rb{ base };
        Value v;
        if (!deferred && field_trail(name.substr(head), rb.v, v) != R::Ok)
            return R::Err;

        Root rv{ v }, rconv{ int_from_i64(conv) };
        if (rconv.v.is_nil())
            return R::Err;
        // A spec with no fields of its own is one step; one with fields is a
        // run of SPECLIT and SPECFMT closed by FMTSPEC.
        if (has_spec && spec.find('{') != Str::npos) {
            if (plan_spec(plan, spec, args, kwargs, autonum, mode) != R::Ok)
                return R::Err;
            if (!step_push(plan, FMTSPEC, rv.v, Value(), rconv.v))
                return oom();
        } else {
            Root rs{ str_new(has_spec ? spec : Str("")) };
            if (rs.v.is_nil())
                return R::Err;
            if (!step_push(plan, FMT, rv.v, rs.v, rconv.v))
                return oom();
        }
        at  = stop + 1;
        run = at;
    }
}

// -------------------------------------------------------------- the % syntax

// One % conversion turned into a format spec, which is where the two grammars
// meet. `%s` is right-aligned by default and str's own spec is not, so the
// alignment is always written out.
bool is_int_type(char c)
{
    return c == 'd' || c == 'o' || c == 'x' || c == 'X' || c == 'c';
}

struct Percent {
    char type      = 0;
    bool left      = false;
    bool zero      = false;
    bool alt       = false;
    char sign      = 0;
    i64 width      = -1;
    i64 prec       = -1;
    bool star_wide = false;
    bool star_prec = false;
    Str key; // %(name)s
    bool keyed = false;
};

// The spec this conversion means. A number is right-aligned by default and a
// str is not, so the alignment is written out for text and left implicit for a
// number -- where an explicit one would stop a bare `0` from meaning `0=`.
bool spec_text(const Percent &p, String &out)
{
    char body = p.type;
    bool text = body == 's' || body == 'r' || body == 'a' || body == 'b';
    if (body == 'i' || body == 'u')
        body = 'd';

    Buf<48> b;
    // An alignment is only worth writing where there is a width to fill, and
    // for text at that: a number is right-aligned already.
    if (p.width > 0) {
        if (p.left)
            b.put('<');
        else if (text)
            b.put('>');
    }
    if (p.sign && !text)
        b.put(p.sign);
    if (p.alt && !text)
        b.put('#');
    // '-' beats '0', as it does in printf.
    if (p.zero && !p.left && !text)
        b.put('0');
    if (p.width > 0) {
        char t[24];
        b.put(int_text(t, sizeof t, p.width));
    }
    // An integer's precision is a digit count and travels beside the spec.
    if (p.prec >= 0 && !is_int_type(body)) {
        char t[24];
        b.put('.').put(int_text(t, sizeof t, p.prec));
    }
    if (!text)
        b.put(body);
    return out.append(b.str());
}

// One conversion, read off the template at `at`. False leaves the error set.
bool percent_read(Str t, usize &at, bool keyed_ok, Percent &p)
{
    if (at < t.size() && t[at] == '(') {
        if (!keyed_ok)
            return err_set("TypeError", "format requires a mapping"), false;
        usize close = at;
        u32 depth   = 0;
        for (; close < t.size(); close++) {
            if (t[close] == '(')
                depth++;
            else if (t[close] == ')' && !--depth)
                break;
        }
        if (close >= t.size())
            return err_set("ValueError", "incomplete format key"), false;
        p.key   = t.substr(at + 1, close - at - 1);
        p.keyed = true;
        at      = close + 1;
    }
    for (; at < t.size(); at++) {
        char c = t[at];
        if (c == '-')
            p.left = true;
        else if (c == '0')
            p.zero = true;
        else if (c == '#')
            p.alt = true;
        else if (c == '+')
            p.sign = '+';
        else if (c == ' ') {
            if (p.sign != '+')
                p.sign = ' ';
        } else
            break;
    }
    if (at < t.size() && t[at] == '*') {
        p.star_wide = true;
        at++;
    } else {
        i64 w = -1;
        while (at < t.size() && is_dig(t[at])) {
            w = (w < 0 ? 0 : w) * 10 + (t[at++] - '0');
            if (w > 0x7fffff)
                return err_set("ValueError", "width too big"), false;
        }
        p.width = w;
    }
    if (at < t.size() && t[at] == '.') {
        at++;
        if (at < t.size() && t[at] == '*') {
            p.star_prec = true;
            at++;
        } else {
            i64 q = 0;
            while (at < t.size() && is_dig(t[at]))
                q = q * 10 + (t[at++] - '0');
            p.prec = q;
        }
    }
    // A length modifier printf takes and Python ignores.
    while (at < t.size() && (t[at] == 'h' || t[at] == 'l' || t[at] == 'L'))
        at++;
    if (at >= t.size())
        return err_set("ValueError", "incomplete format"), false;
    p.type = t[at++];
    return true;
}

// The next argument, from the tuple or -- with %(name) -- from the mapping.
R percent_arg(const Percent &p, Value values, Value mapping, bool bytes, usize &next, Value &out)
{
    if (p.keyed) {
        if (mapping.is_nil())
            return err_set("TypeError", "format requires a mapping");
        // A bytes template names its keys in bytes, which is what makes
        // b"%(k)s" % {"k": ...} a KeyError.
        Root key{ bytes ? bytes_new(p.key) : str_new(p.key) };
        if (key.v.is_nil())
            return R::Err;
        return py_getitem(mapping, key.v, out);
    }
    TupleObj *t = static_cast<TupleObj *>(values.obj());
    if (next >= t->len)
        return err_set("TypeError", "not enough arguments for format string");
    out = t->items()[next++];
    return R::Ok;
}

// `%` over a str or over octets. The plan it builds is the same one
// str.format builds, so both walk the same continuation.
R plan_percent(ListObj *plan, Str t, Value values, Value mapping, bool bytes, bool single,
               usize &used)
{
    usize at = 0, run = 0;
    used = 0;
    while (at <= t.size()) {
        if (at == t.size()) {
            if (at > run && !step_text(plan, LIT, t.substr(run, at - run)))
                return oom();
            return R::Ok;
        }
        if (t[at] != '%') {
            at++;
            continue;
        }
        if (at > run && !step_text(plan, LIT, t.substr(run, at - run)))
            return oom();
        at++;
        if (at < t.size() && t[at] == '%') {
            if (!step_text(plan, LIT, Str("%")))
                return oom();
            at++;
            run = at;
            continue;
        }

        Percent p;
        if (!percent_read(t, at, !mapping.is_nil(), p))
            return R::Err;
        run = at;

        // With a mapping on the right there is one positional value and it is
        // the mapping itself, so naming a key spends it: `"%(a)s %s"` has
        // nothing left for the `%s`, where `"%s %(a)s"` does.
        if (p.keyed && single)
            used = 1;

        if (p.star_wide) {
            Value w;
            if (percent_arg(p, values, mapping, bytes, used, w) != R::Ok)
                return R::Err;
            i64 n = 0;
            if (!as_index(w, n))
                return err_set("TypeError", "* wants an int");
            if (n < 0) {
                p.left = true;
                n      = -n;
            }
            p.width = n;
        }
        if (p.star_prec) {
            Value q;
            if (percent_arg(p, values, mapping, bytes, used, q) != R::Ok)
                return R::Err;
            i64 n = 0;
            if (!as_index(q, n))
                return err_set("TypeError", "* wants an int");
            p.prec = n < 0 ? -1 : n;
        }

        Value v;
        if (percent_arg(p, values, mapping, bytes, used, v) != R::Ok)
            return R::Err;
        Root rv{ v };

        i64 conv = CONV_NONE;
        switch (p.type) {
        case 's':
            conv = bytes ? CONV_NONE : CONV_STR;
            break;
        case 'b':
            if (!bytes)
                return err_set("ValueError", "unsupported format character 'b'");
            break;
        case 'r':
            conv = CONV_REPR;
            break;
        case 'a':
            conv = CONV_ASCII;
            break;
        case 'd':
        case 'i':
        case 'u':
            // printf truncates a float toward zero here, and Python kept it.
            if (is_float(rv.v)) {
                f64 x = float_of(rv.v);
                if (isnan(x) || isinf(x))
                    return err_set("OverflowError", "cannot convert float to integer");
                rv = int_from_f64(x);
                if (rv.v.is_nil())
                    return R::Err;
            }
            break;
        case 'o':
        case 'x':
        case 'X':
        case 'c':
        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
            break;
        default: {
            Buf<64> b;
            b.put("unsupported format character '").put(p.type).put("'");
            return err_set("ValueError", b.str());
        }
        }

        // A number is required here, and `%` says so with a TypeError where
        // the spec grammar would call the spec itself wrong.
        if (p.type != 's' && p.type != 'r' && p.type != 'a' && p.type != 'b') {
            i64 n   = 0;
            f64 x   = 0;
            bool ok = p.type == 'c' ? (as_index(rv.v, n) || is_str(rv.v))
                      : (p.type == 'e' || p.type == 'E' || p.type == 'f' || p.type == 'F' ||
                         p.type == 'g' || p.type == 'G')
                          ? (is_intval(rv.v) || as_number(rv.v, x))
                          : is_intval(rv.v);
            if (!ok && is_inst(rv.v))
                ok = !type_special(rv.v, "__index__").is_nil() ||
                     !type_special(rv.v, "__int__").is_nil() ||
                     !type_special(rv.v, "__float__").is_nil();
            if (!ok) {
                Buf<96> b;
                b.put("%").put(p.type).put(" format: a number is required");
                return err_set2("TypeError", b.str(), type_name(rv.v));
            }
        }

        // %s of a bytes wants its octets and not its repr; %c takes a
        // one-character string as well as an integer.
        if (p.type == 'c' && is_str(rv.v)) {
            if (str_of(rv.v)->chars != 1)
                return err_set("TypeError", "%c requires int or char");
            conv   = CONV_STR;
            p.type = 's';
        }
        if (bytes && (p.type == 's' || p.type == 'b')) {
            Str raw;
            if (!bytes_like(rv.v, raw))
                return err_set2("TypeError", "%b requires a bytes-like object", type_name(rv.v));
            rv = str_of_bytes(raw);
            if (rv.v.is_nil())
                return R::Err;
            p.type = 's';
        }

        char body = p.type == 'i' || p.type == 'u' ? 'd' : p.type;
        i64 least = p.prec >= 0 && is_int_type(body) ? p.prec : -1;

        // `%d` of a class asks it for an integer, which `{:d}` does not: the
        // two grammars part company here, so the call is a step of its own.
        bool via_int = false;
        if (is_int_type(body) && body != 'c' && is_inst(rv.v)) {
            Root m{ type_special(rv.v, "__index__") };
            if (m.v.is_nil())
                m = type_special(rv.v, "__int__");
            if (!m.v.is_nil()) {
                if (!step_push(plan, CALL, m.v, Value(), Value()))
                    return oom();
                via_int = true;
            }
        }

        String spec;
        if (!spec_text(p, spec))
            return oom();
        Root rs{ str_new(spec.str()) }, rc{ int_from_i64(conv) }, rd{ int_from_i64(least) };
        if (rs.v.is_nil() || rc.v.is_nil() || rd.v.is_nil())
            return R::Err;
        if (!step_push(plan, FMT, via_int ? Value() : rv.v, rs.v, rc.v, rd.v))
            return oom();
    }
    return R::Ok;
}

} // namespace

// ------------------------------------------------------------ the two passes

namespace {

// s[0] the plan, s[1] the output parts, s[2] the spec being built, s[3] the
// native that formats one value; i is the step, and the step before it says
// where its answer goes.
R plan_step(ContObj *k, Value in)
{
    ListObj *plan = list_of(k->s[0]);
    if (k->i && !in.is_nil()) {
        i64 was = 0;
        as_index(plan->items[(k->i - 1) * STEP], was);
        if (was == CALL) {
            k->s[4] = in;
        } else {
            if (!is_str(in))
                return err_set2("TypeError", "__format__ must return a str", type_name(in));
            if (!list_push(list_of(was == SPECFMT ? k->s[2] : k->s[1]), in))
                return oom();
        }
    }

    while (k->i * STEP < plan->items.size()) {
        usize base = k->i * STEP;
        i64 kind   = 0;
        as_index(plan->items[base], kind);
        Value a = plan->items[base + 1];
        Value b = plan->items[base + 2];
        Value c = plan->items[base + 3];
        Value d = plan->items[base + 4];
        k->i++;

        if (kind == LIT) {
            if (!list_push(list_of(k->s[1]), a))
                return oom();
            continue;
        }
        if (kind == SPECLIT) {
            if (!list_push(list_of(k->s[2]), a))
                return oom();
            continue;
        }
        if (kind == CALL)
            return cont_call(k, a, b, b.is_nil() ? 0 : 1);
        // A step with no value of its own formats what TOINT just produced.
        if (a.is_nil())
            a = k->s[4];

        Root spec;
        if (kind == FMTSPEC) {
            // The pieces collected since the last one are this field's spec.
            String text;
            ListObj *parts = list_of(k->s[2]);
            for (usize x = 0; x < parts->items.size(); x++)
                if (!text.append(str_of(parts->items[x])->str()))
                    return oom();
            parts->items.clear();
            spec = str_new(text.str());
            if (spec.v.is_nil())
                return R::Err;
        } else if (kind == SPECFMT) {
            spec = str_new(Str(""));
            if (spec.v.is_nil())
                return R::Err;
            c = Value();
            d = Value();
        } else {
            spec = b;
        }

        Root argv{ obj_value(tuple_new(4)) };
        if (argv.v.is_nil())
            return oom();
        TupleObj *t   = static_cast<TupleObj *>(argv.v.obj());
        t->items()[0] = a;
        t->items()[1] = spec.v;
        t->items()[2] = c.is_nil() ? int_from_i64(0) : c;
        t->items()[3] = d.is_nil() ? int_from_i64(-1) : d;
        if (t->items()[2].is_nil() || t->items()[3].is_nil())
            return R::Err;
        return cont_call_v(k, k->s[3], argv.v);
    }

    String text;
    ListObj *parts = list_of(k->s[1]);
    for (usize x = 0; x < parts->items.size(); x++)
        if (!text.append(str_of(parts->items[x])->str()))
            return oom();
    Value made = k->j ? bytes_new(text.str()) : str_new(text.str());
    return made.is_nil() ? R::Err : cont_done(k, made);
}

// format(value, spec, conv) as a callable, so plan_step can ask for it and
// run_cont chains whatever it parks on.
R fmt_one(const CallArgs &a, Value &out)
{
    i64 conv = 0, least = -1;
    as_index(a.args[2], conv);
    as_index(a.args[3], least);
    return format_field(a.args[0], str_of(a.args[1])->str(), u32(conv), i32(least), out);
}

Value plan_run(ListObj *plan, bool bytes)
{
    Root rp{ obj_value(plan) };
    Root out{ obj_value(list_new()) }, spec{ obj_value(list_new()) };
    Root fn{ native_new("__format_one", fmt_one) };
    if (out.v.is_nil() || spec.v.is_nil() || fn.v.is_nil())
        return oom(), Value();
    Value kv = cont_new(plan_step);
    if (kv.is_nil())
        return Value();
    cont_of(kv)->s[0] = rp.v;
    cont_of(kv)->s[1] = out.v;
    cont_of(kv)->s[2] = spec.v;
    cont_of(kv)->s[3] = fn.v;
    cont_of(kv)->j    = bytes ? 1 : 0;
    return kv;
}

} // namespace

Value str_format_call(Value self, Value args, Value kwargs, Value mapping)
{
    Root rs{ self }, ra{ args }, rk{ kwargs }, rm{ mapping };
    Root plan{ obj_value(list_new()) };
    if (plan.v.is_nil())
        return oom(), Value();
    Value names = rm.v.is_nil() ? rk.v : rm.v;
    if (plan_format(list_of(plan.v), str_of(rs.v)->str(), ra.v, names) != R::Ok)
        return Value();
    return plan_run(list_of(plan.v), false);
}

// `fmt % right`. A tuple on the right is the argument list; a mapping is one
// only when the template names keys, since `"%s" % {}` prints the dict.
static Value mod_common(Value fmt, Value right, bool bytes)
{
    Root rf{ fmt }, rr{ right };
    Str t = bytes ? Str() : str_of(rf.v)->str();
    Str raw;
    if (bytes) {
        if (!bytes_like(rf.v, raw))
            return err_set("TypeError", "the left operand must be bytes-like"), Value();
        t = raw;
    }

    // A mapping on the right is both: it answers %(name) and it is also the
    // one positional value, which is why `'%s %(foo)s' % {'foo': 1}` works.
    // Nothing is left over in that case, so the count is not checked either.
    bool map = !is_tuple(rr.v) &&
               (is_dict(rr.v) || (is_inst(rr.v) && !type_special(rr.v, "__getitem__").is_nil()));

    Root mapping;
    Root values;
    if (map)
        mapping = rr.v;
    if (is_tuple(rr.v)) {
        values = rr.v;
    } else {
        TupleObj *one = tuple_new(1);
        if (!one)
            return oom(), Value();
        one->items()[0] = rr.v;
        values          = obj_value(one);
    }
    if (values.v.is_nil())
        return oom(), Value();

    Root plan{ obj_value(list_new()) };
    if (plan.v.is_nil())
        return oom(), Value();
    usize used = 0;
    if (plan_percent(list_of(plan.v), t, values.v, mapping.v, bytes, !is_tuple(rr.v), used) !=
        R::Ok)
        return Value();
    if (!map && used < usize(static_cast<TupleObj *>(values.v.obj())->len))
        return err_set("TypeError", "not all arguments converted during string formatting"),
               Value();
    return plan_run(list_of(plan.v), bytes);
}

Value str_mod(Value fmt, Value right)
{
    return mod_common(fmt, right, false);
}

Value bytes_mod(Value fmt, Value right)
{
    return mod_common(fmt, right, true);
}
