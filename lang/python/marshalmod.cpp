// `marshal`: Python/marshal.c's format, for everything but code objects.
//
// A code object here is this interpreter's own bytecode, which no .pyc
// describes, so dumps() refuses one as unmarshallable and loads() refuses
// TYPE_CODE; nothing writes a .pyc (sys.dont_write_bytecode). The rest is the
// format byte for byte, with two differences. Both walks are loops over a
// stack of their own rather than recursion, which a 128 KiB stack cannot
// afford at CPython's depth of 2000. And dumps() marks nothing for reuse but
// an interned string: CPython decides by reference count, which there is none
// of here, so a shared tuple is written twice where CPython writes a TYPE_REF.
// loads() reads references either way.
#include "bigint.h"
#include "complex.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "iter.h"
#include "kernel/fmt.h"
#include "kernel/text.h"
#include "math/ftoa.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "posix.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

constexpr i32 VERSION   = 6;
constexpr u32 MAX_DEPTH = 2000;
constexpr i64 SIZE32    = 0x7fffffff;

enum : u8 {
    T_NULL                 = '0',
    T_NONE                 = 'N',
    T_FALSE                = 'F',
    T_TRUE                 = 'T',
    T_STOPITER             = 'S',
    T_ELLIPSIS             = '.',
    T_BINARY_FLOAT         = 'g',
    T_BINARY_COMPLEX       = 'y',
    T_LONG                 = 'l',
    T_STRING               = 's',
    T_TUPLE                = '(',
    T_LIST                 = '[',
    T_DICT                 = '{',
    T_FROZENDICT           = '}',
    T_CODE                 = 'c',
    T_UNICODE              = 'u',
    T_UNKNOWN              = '?',
    T_SET                  = '<',
    T_FROZENSET            = '>',
    T_SLICE                = ':',
    T_INTERNED             = 't',
    T_ASCII                = 'a',
    T_ASCII_INTERNED       = 'A',
    T_SHORT_ASCII          = 'z',
    T_SHORT_ASCII_INTERNED = 'Z',
    T_INT                  = 'i',
    T_SMALL_TUPLE          = ')',
    T_COMPLEX              = 'x',
    T_FLOAT                = 'f',
    T_INT64                = 'I',
    T_REF                  = 'r',
    FLAG_REF               = 0x80,
};

enum WErr : u8 { W_OK, W_UNMARSHALLABLE, W_DEEP, W_NOMEM, W_CODE, W_SET };

// ---------------------------------------------------------------- writing

struct Writer {
    String out;
    i32 version;
    bool allow_code;
    WErr error = W_OK;
    u32 depth  = 0;
    // Every object written so far, by identity, with its slot; a slot's
    // complement marks one still being written, which a reference may not name.
    Root refs{ obj_value(dict_new()) };

    void byte(u8 c)
    {
        if (!out.push(char(c)) && error == W_OK)
            error = W_NOMEM;
    }

    void bytes(Str s)
    {
        if (!out.append(s) && error == W_OK)
            error = W_NOMEM;
    }

    void w_short(i32 x)
    {
        byte(u8(x & 0xff));
        byte(u8((x >> 8) & 0xff));
    }

    void w_long(i64 x)
    {
        for (u32 i = 0; i < 4; i++)
            byte(u8((x >> (8 * i)) & 0xff));
    }

    bool w_size(usize n)
    {
        if (n > usize(SIZE32)) {
            error = W_UNMARSHALLABLE;
            return false;
        }
        w_long(i64(n));
        return true;
    }

    void pstring(Str s)
    {
        if (w_size(s.size()))
            bytes(s);
    }

    void short_pstring(Str s)
    {
        byte(u8(s.size()));
        bytes(s);
    }

    void float_bin(f64 v)
    {
        char b[8];
        __builtin_memcpy(b, &v, 8);
        bytes(Str(b, 8));
    }

    void float_str(f64 v)
    {
        char t[64];
        short_pstring(fmt_f64(t, sizeof t, v, 17, 'g'));
    }

    // A long in base 2**15, sign on the count.
    void w_pylong(Value v, u8 flag)
    {
        byte(T_LONG | flag);
        Root b{ big_of_value(v) };
        if (b.v.is_nil()) {
            error = W_NOMEM;
            return;
        }
        BigObj *bo = big_of(b.v);
        // The magnitude as 15-bit digits.
        Vec<u16> digits;
        u64 acc  = 0;
        u32 have = 0;
        for (u32 i = 0; i < bo->len; i++) {
            acc |= u64(bo->limbs()[i]) << have;
            have += 32;
            while (have >= 15) {
                digits.push(u16(acc & 0x7fff));
                acc >>= 15;
                have -= 15;
            }
        }
        while (have > 0) {
            digits.push(u16(acc & 0x7fff));
            acc >>= 15;
            have = have > 15 ? have - 15 : 0;
        }
        while (!digits.empty() && digits[digits.size() - 1] == 0)
            digits.pop();
        if (digits.size() > usize(SIZE32)) {
            error = W_UNMARSHALLABLE;
            return;
        }
        i64 n = i64(digits.size());
        w_long(bo->neg ? -n : n);
        for (u16 d : digits)
            w_short(d);
    }

    // Whether a str is kept in the intern table, which is where CPython's
    // interned strings are; those are always written for reuse.
    static bool interned(Value v) { return str_is_interned(str_of(v)); }

    // A reference to `v` if it was written before, else a slot for it and
    // FLAG_REF in `flag`. True when nothing more is to be written for it.
    bool w_ref(Value v, u8 &flag, bool incomplete)
    {
        if (version < 3)
            return false;
        DictObj *t = static_cast<DictObj *>(refs.v.obj());
        Root key{ int_from_i64(i64(v.w)) };
        Value at;
        if (key.v.is_nil())
            return error = W_NOMEM, true;
        R r = dict_get(t, key.v, at);
        if (r == R::Ok) {
            i64 n = 0;
            as_index(at, n);
            if (n < 0) {
                error = W_UNMARSHALLABLE;
                return true;
            }
            byte(T_REF);
            w_long(n);
            return true;
        }
        if (r != R::NotImpl)
            return error = W_NOMEM, true;
        i64 n = i64(dict_len(t));
        if (dict_set(t, key.v, Value::of_int(incomplete ? ~n : n)) != R::Ok)
            return error = W_NOMEM, true;
        flag |= FLAG_REF;
        return false;
    }

    void complete(Value v)
    {
        DictObj *t = static_cast<DictObj *>(refs.v.obj());
        Root key{ int_from_i64(i64(v.w)) };
        Value at;
        i64 n = 0;
        if (!key.v.is_nil() && dict_get(t, key.v, at) == R::Ok && as_index(at, n) && n < 0)
            dict_set(t, key.v, Value::of_int(~n));
    }

    void simple(Value v, u8 flag);
};

// dumps(v) of one object, whole: what a set's members are sorted by.
bool dump_one(Value v, i32 version, bool allow_code, u32 depth, String &out, WErr &err);

void Writer::simple(Value v, u8 flag)
{
    // Containers are handled by the walk; this is one leaf.
    if (is_intval(v) && !is_bool(v)) {
        i64 x = 0;
        if (int_to_i64(v, x) && x >= -2147483648ll && x <= 2147483647ll) {
            byte(T_INT | flag);
            w_long(x);
        } else {
            w_pylong(v, flag);
        }
    } else if (is_float(v)) {
        if (version > 1) {
            byte(T_BINARY_FLOAT | flag);
            float_bin(float_of(v));
        } else {
            byte(T_FLOAT | flag);
            float_str(float_of(v));
        }
    } else if (is_complex(v)) {
        ComplexObj *c = complex_of(v);
        if (version > 1) {
            byte(T_BINARY_COMPLEX | flag);
            float_bin(c->re);
            float_bin(c->im);
        } else {
            byte(T_COMPLEX | flag);
            float_str(c->re);
            float_str(c->im);
        }
    } else if (is_bytes(v)) {
        byte(T_STRING | flag);
        pstring(static_cast<BytesObj *>(v.obj())->str());
    } else if (is_str(v)) {
        StrObj *s   = str_of(v);
        bool intern = version >= 3 && interned(v);
        if (intern)
            flag = FLAG_REF;
        if (version >= 4 && (v.obj()->flags & OBJ_ASCII)) {
            bool shrt = s->chars < 256;
            if (shrt) {
                byte((intern ? T_SHORT_ASCII_INTERNED : T_SHORT_ASCII) | flag);
                short_pstring(s->str());
            } else {
                byte((intern ? T_ASCII_INTERNED : T_ASCII) | flag);
                pstring(s->str());
            }
        } else {
            // Our bytes already spell a surrogate the way surrogatepass does.
            byte((intern ? T_INTERNED : T_UNICODE) | flag);
            pstring(s->str());
        }
    } else {
        Str raw;
        if (!is_memview(v) && buffer_like(v, raw)) {
            byte(T_STRING | flag);
            pstring(raw);
        } else if (is_memview(v) && buffer_like(v, raw)) {
            byte(T_STRING | flag);
            pstring(raw);
        } else {
            byte(T_UNKNOWN);
            error = W_UNMARSHALLABLE;
        }
    }
}

enum : u8 { WK_OBJ, WK_NULL, WK_POP, WK_DONE };

// The walk: a stack of what is still to be written, pre-order. A container
// writes its header and pushes its members, last first; a dict pushes a NULL
// behind its pairs; every container pushes a marker that ends its depth.
bool write_object(Writer &w, Value root)
{
    Root stack{ obj_value(list_new()) };
    if (stack.v.is_nil())
        return oom() == R::Ok;
    Vec<u8> tags;
    auto push = [&](Value v, u8 tag) {
        if (!list_push(list_of(stack.v), v.is_nil() ? value_none() : v) || !tags.push(tag)) {
            w.error = W_NOMEM;
            return false;
        }
        return true;
    };
    if (!push(root, WK_OBJ))
        return false;
    while (!tags.empty() && w.error == W_OK) {
        u8 tag = tags[tags.size() - 1];
        Root v{ list_of(stack.v)->items[list_of(stack.v)->items.size() - 1] };
        tags.pop();
        list_of(stack.v)->items.pop();
        if (tag == WK_POP) {
            w.depth--;
            continue;
        }
        if (tag == WK_DONE) {
            w.complete(v.v);
            w.depth--;
            continue;
        }
        if (tag == WK_NULL) {
            w.byte(T_NULL);
            continue;
        }
        if (w.depth + 1 > MAX_DEPTH) {
            w.error = W_DEEP;
            break;
        }
        Value x = v.v;
        u8 flag = 0;
        if (is_none(x)) {
            w.byte(T_NONE);
        } else if (x == exc_type_value(exc_find("StopIteration"))) {
            w.byte(T_STOPITER);
        } else if (x == value_ellipsis()) {
            w.byte(T_ELLIPSIS);
        } else if (is_true(x)) {
            w.byte(T_TRUE);
        } else if (is_bool(x)) {
            w.byte(T_FALSE);
        } else if (w.w_ref(x, flag, is_tuple(x) || is_frozendict(x) || is_slice(x))) {
            continue;
        } else if (is_tuple(x)) {
            TupleObj *t = static_cast<TupleObj *>(x.obj());
            if (w.version >= 4 && t->len < 256) {
                w.byte(T_SMALL_TUPLE | flag);
                w.byte(u8(t->len));
            } else {
                w.byte(T_TUPLE | flag);
                if (!w.w_size(t->len))
                    break;
            }
            w.depth++;
            push(v.v, WK_DONE);
            t = static_cast<TupleObj *>(v.v.obj());
            for (u32 i = t->len; i-- > 0;)
                if (!push(static_cast<TupleObj *>(v.v.obj())->items()[i], WK_OBJ))
                    break;
        } else if (is_list(x)) {
            w.byte(T_LIST | flag);
            usize n = list_of(x)->items.size();
            if (!w.w_size(n))
                break;
            w.depth++;
            push(v.v, WK_DONE);
            for (usize i = list_of(v.v)->items.size(); i-- > 0;)
                if (!push(list_of(v.v)->items[i], WK_OBJ))
                    break;
        } else if (is_anydict(x)) {
            if (is_frozendict(x)) {
                if (w.version < 6) {
                    w.byte(T_UNKNOWN);
                    w.error = W_UNMARSHALLABLE;
                    break;
                }
                w.byte(T_FROZENDICT | flag);
            } else {
                w.byte(T_DICT | flag);
            }
            w.depth++;
            push(v.v, WK_DONE);
            push(Value(), WK_NULL);
            // The pairs, in order: pushed last first.
            Vec<Value> kv;
            usize at = 0;
            Value k, val;
            while (table_next(static_cast<DictObj *>(v.v.obj())->t, at, k, val))
                if (!kv.push(k) || !kv.push(val)) {
                    w.error = W_NOMEM;
                    break;
                }
            Roots pin{ kv.data(), kv.size() };
            for (usize i = kv.size(); i-- > 0;)
                if (!push(kv[i], WK_OBJ))
                    break;
        } else if (is_anyset(x)) {
            w.byte((is_frozenset(x) ? T_FROZENSET : T_SET) | flag);
            usize n = set_len(set_at(x));
            if (!w.w_size(n))
                break;
            // Members in the order of their own dumps, for a stable file.
            ListObj *pairs = list_new();
            if (!pairs) {
                w.error = W_NOMEM;
                break;
            }
            Root rp{ obj_value(pairs) };
            usize at = 0;
            Value k, val;
            while (w.error == W_OK && table_next(set_at(v.v)->t, at, k, val)) {
                Root rk{ k };
                String dump;
                WErr err = W_OK;
                if (!dump_one(rk.v, w.version, w.allow_code, w.depth + 1, dump, err)) {
                    w.error = err == W_OK ? W_SET : err;
                    break;
                }
                Root rd{ bytes_new(dump.str()) };
                TupleObj *pr = rd.v.is_nil() ? nullptr : tuple_new(2);
                if (!pr) {
                    w.error = W_SET;
                    break;
                }
                pr->items()[0] = rd.v;
                pr->items()[1] = rk.v;
                if (!list_push(list_of(rp.v), obj_value(pr))) {
                    w.error = W_NOMEM;
                    break;
                }
            }
            if (w.error != W_OK)
                break;
            // Sorted by the dumps alone, which are unique for distinct members.
            Vec<Value> keys;
            for (Value p : list_of(rp.v)->items)
                if (!keys.push(static_cast<TupleObj *>(p.obj())->items()[0])) {
                    w.error = W_NOMEM;
                    break;
                }
            Vec<u32> idx;
            for (usize i = 0; i < keys.size(); i++)
                if (!idx.push(u32(i))) {
                    w.error = W_NOMEM;
                    break;
                }
            Roots pin{ keys.data(), keys.size() };
            if (w.error != W_OK || sort_idx(keys, idx, false) != R::Ok) {
                w.error = W_SET;
                break;
            }
            w.depth++;
            push(v.v, WK_DONE);
            for (usize i = idx.size(); i-- > 0;)
                if (!push(static_cast<TupleObj *>(list_of(rp.v)->items[idx[i]].obj())->items()[1],
                          WK_OBJ))
                    break;
        } else if (is_slice(x)) {
            if (w.version < 5) {
                w.byte(T_UNKNOWN);
                w.error = W_UNMARSHALLABLE;
                break;
            }
            w.byte(T_SLICE | flag);
            SliceObj *s = static_cast<SliceObj *>(x.obj());
            w.depth++;
            push(v.v, WK_DONE);
            push(s->step.is_nil() ? value_none() : s->step, WK_OBJ);
            push(s->stop.is_nil() ? value_none() : s->stop, WK_OBJ);
            push(s->start.is_nil() ? value_none() : s->start, WK_OBJ);
        } else if (is_code(x)) {
            if (!w.allow_code) {
                w.error = W_CODE;
                break;
            }
            w.byte(T_UNKNOWN);
            w.error = W_UNMARSHALLABLE;
        } else {
            w.simple(x, flag);
        }
    }
    return w.error == W_OK;
}

bool dump_one(Value v, i32 version, bool allow_code, u32 depth, String &out, WErr &err)
{
    Writer w;
    w.version    = version;
    w.allow_code = allow_code;
    w.depth      = depth;
    if (w.refs.v.is_nil()) {
        err = W_NOMEM;
        return false;
    }
    bool ok = write_object(w, v);
    err     = w.error;
    if (ok)
        out.assign(w.out.str());
    return ok;
}

R set_error(WErr e)
{
    switch (e) {
    case W_NOMEM:
        return oom();
    case W_DEEP:
        return err_set("ValueError", "object too deeply nested to marshal");
    case W_CODE:
        return err_set("ValueError", "marshalling code objects is disallowed");
    case W_SET:
        return err_pending() ? R::Err : err_set("ValueError", "unmarshallable object");
    default:
        return err_set("ValueError", "unmarshallable object");
    }
}

R dumps_to(Value v, i32 version, bool allow_code, Value &out)
{
    String s;
    WErr err = W_OK;
    if (!dump_one(v, version, allow_code, 0, s, err))
        return set_error(err);
    out = bytes_new(s.str());
    return out.is_nil() ? R::Err : R::Ok;
}

// ---------------------------------------------------------------- reading

struct Reader {
    Str data;
    usize at = 0;
    bool allow_code;

    const char *take(usize n)
    {
        if (data.size() - at < n) {
            err_set("EOFError", "marshal data too short");
            return nullptr;
        }
        const char *p = data.data() + at;
        at += n;
        return p;
    }

    i32 r_byte()
    {
        if (at < data.size())
            return u8(data[at++]);
        err_set("EOFError", "EOF read where not expected");
        return -1;
    }

    bool r_short(i32 &x)
    {
        const char *b = take(2);
        if (!b)
            return false;
        i32 v = u8(b[0]) | (u8(b[1]) << 8);
        x     = v & 0x8000 ? v - 0x10000 : v;
        return true;
    }

    bool r_long(i64 &x)
    {
        const char *b = take(4);
        if (!b)
            return false;
        u32 v = u32(u8(b[0])) | u32(u8(b[1])) << 8 | u32(u8(b[2])) << 16 | u32(u8(b[3])) << 24;
        x     = i64(i32(v));
        return true;
    }

    bool r_float_bin(f64 &x)
    {
        const char *b = take(8);
        if (!b)
            return false;
        __builtin_memcpy(&x, b, 8);
        return true;
    }

    bool r_float_str(f64 &x)
    {
        i32 n = r_byte();
        if (n < 0)
            return false;
        const char *b = take(usize(n));
        if (!b)
            return false;
        Option<f64> got = parse_f64(Str(b, usize(n)));
        if (!got.has_value()) {
            Buf<96> m;
            m.put("could not convert string to float: '").put(Str(b, usize(n))).put('\'');
            return err_set("ValueError", m.str()) == R::Ok;
        }
        x = got.value();
        return true;
    }

    Value r_pylong()
    {
        i64 n = 0;
        if (!r_long(n))
            return Value();
        if (n < -SIZE32 || n > SIZE32)
            return err_set("ValueError", "bad marshal data (long size out of range)"), Value();
        i64 count = n < 0 ? -n : n;
        if (count == 0)
            return Value::of_int(0);
        Vec<u32> limbs;
        u64 acc  = 0;
        u32 have = 0;
        for (i64 i = 0; i < count; i++) {
            i32 d = 0;
            if (!r_short(d))
                return Value();
            if (d < 0 || d > 0x8000)
                return err_set("ValueError", "bad marshal data (digit out of range in long)"),
                       Value();
            if (d == 0 && i == count - 1)
                return err_set("ValueError", "bad marshal data (unnormalized long data)"), Value();
            acc |= u64(d) << have;
            have += 15;
            if (have >= 32) {
                if (!limbs.push(u32(acc)))
                    return oom(), Value();
                acc >>= 32;
                have -= 32;
            }
        }
        if (have && !limbs.push(u32(acc)))
            return oom(), Value();
        return big_make(limbs.data(), limbs.size(), n < 0);
    }
};

enum : u8 { RK_TUPLE, RK_LIST, RK_DICT, RK_FROZENDICT, RK_SET, RK_FROZENSET, RK_SLICE };

// A container being filled: `obj` a list the members go into, `n` how many
// are still owed (a dict owes until its NULL), `ref` its slot, or -1.
struct Frame {
    u8 kind;
    i64 n;
    i64 ref;
};

R load(Reader &rd, Value &out)
{
    Root refs{ obj_value(list_new()) };
    Root stack{ obj_value(list_new()) }; // the members gathered so far, per frame
    if (refs.v.is_nil() || stack.v.is_nil())
        return oom();
    Vec<Frame> frames;
    auto refs_list = [&]() { return list_of(refs.v); };

    for (;;) {
        Root got; // what this turn made, Nil for TYPE_NULL
        bool made_null = false;
        i32 code       = rd.r_byte();
        if (code < 0) {
            if (err_kind() == "EOFError") {
                err_clear();
                return err_set("EOFError", "EOF read where object expected");
            }
            return R::Err;
        }
        if (frames.size() + 1 > MAX_DEPTH)
            return err_set("ValueError", "recursion limit exceeded");
        bool flag = (code & FLAG_REF) != 0;
        u8 type   = u8(code & ~FLAG_REF);
        auto keep = [&](Value v) {
            if (flag && !list_push(refs_list(), v))
                return false;
            return true;
        };
        auto reserve = [&]() -> i64 {
            if (!flag)
                return -1;
            i64 idx = i64(refs_list()->items.size());
            if (!list_push(refs_list(), value_none()))
                return -2;
            return idx;
        };
        auto open = [&](u8 kind, i64 n, i64 ref) {
            ListObj *l = list_new();
            if (!l || !list_push(list_of(stack.v), obj_value(l)) ||
                !frames.push(Frame{ kind, n, ref }))
                return false;
            return true;
        };
        i64 n = 0;
        switch (type) {
        case T_NULL:
            made_null = true;
            break;
        case T_NONE:
            got = value_none();
            break;
        case T_STOPITER:
            got = exc_type_value(exc_find("StopIteration"));
            break;
        case T_ELLIPSIS:
            got = value_ellipsis();
            break;
        case T_FALSE:
            got = value_bool(false);
            break;
        case T_TRUE:
            got = value_bool(true);
            break;
        case T_INT:
            if (!rd.r_long(n))
                return R::Err;
            got = int_from_i64(n);
            if (got.v.is_nil() || !keep(got.v))
                return R::Err;
            break;
        case T_INT64: {
            const char *b = rd.take(8);
            if (!b)
                return R::Err;
            i64 x = 0;
            __builtin_memcpy(&x, b, 8);
            got = int_from_i64(x);
            if (got.v.is_nil() || !keep(got.v))
                return R::Err;
            break;
        }
        case T_LONG:
            got = rd.r_pylong();
            if (got.v.is_nil() || !keep(got.v))
                return R::Err;
            break;
        case T_FLOAT:
        case T_BINARY_FLOAT: {
            f64 x = 0;
            if (!(type == T_FLOAT ? rd.r_float_str(x) : rd.r_float_bin(x)))
                return R::Err;
            got = float_new(x);
            if (got.v.is_nil() || !keep(got.v))
                return R::Err;
            break;
        }
        case T_COMPLEX:
        case T_BINARY_COMPLEX: {
            f64 re = 0, im = 0;
            bool ok = type == T_COMPLEX ? rd.r_float_str(re) && rd.r_float_str(im)
                                        : rd.r_float_bin(re) && rd.r_float_bin(im);
            if (!ok)
                return R::Err;
            got = complex_new(re, im);
            if (got.v.is_nil() || !keep(got.v))
                return R::Err;
            break;
        }
        case T_STRING: {
            if (!rd.r_long(n))
                return R::Err;
            if (n < 0 || n > SIZE32)
                return err_set("ValueError", "bad marshal data (bytes object size out of range)");
            const char *p = rd.take(usize(n));
            if (!p)
                return R::Err;
            got = bytes_new(Str(p, usize(n)));
            if (got.v.is_nil() || !keep(got.v))
                return R::Err;
            break;
        }
        case T_ASCII_INTERNED:
        case T_ASCII:
        case T_SHORT_ASCII_INTERNED:
        case T_SHORT_ASCII: {
            if (type == T_ASCII || type == T_ASCII_INTERNED) {
                if (!rd.r_long(n))
                    return R::Err;
                if (n < 0 || n > SIZE32)
                    return err_set("ValueError", "bad marshal data (string size out of range)");
            } else {
                n = rd.r_byte();
                if (n < 0)
                    return R::Err;
            }
            const char *p = rd.take(usize(n));
            if (!p)
                return R::Err;
            // One octet a character: Latin-1, which is what CPython reads.
            Str s      = Str(p, usize(n));
            bool ascii = true;
            for (usize i = 0; i < s.size() && ascii; i++)
                ascii = u8(s[i]) < 0x80;
            if (ascii) {
                bool interned = type == T_ASCII_INTERNED || type == T_SHORT_ASCII_INTERNED;
                StrObj *o     = interned ? str_intern(s) : str_raw(s);
                got           = o ? obj_value(o) : Value();
            } else {
                String u;
                for (usize i = 0; i < s.size(); i++) {
                    char t[4];
                    if (!u.append(Str(t, utf8_encode(char32_t(u8(s[i])), t))))
                        return oom();
                }
                got = str_new(u.str());
            }
            if (got.v.is_nil() || !keep(got.v))
                return err_pending() ? R::Err : oom();
            break;
        }
        case T_INTERNED:
        case T_UNICODE: {
            if (!rd.r_long(n))
                return R::Err;
            if (n < 0 || n > SIZE32)
                return err_set("ValueError", "bad marshal data (string size out of range)");
            const char *p = rd.take(usize(n));
            if (!p)
                return R::Err;
            // surrogatepass: our str already spells a surrogate this way.
            Root b{ bytes_new(Str(p, usize(n))) };
            Root errs{ str_new("surrogatepass") };
            if (b.v.is_nil() || errs.v.is_nil())
                return R::Err;
            CodecCall c;
            c.codec  = Codec::Utf8;
            c.input  = b.v;
            c.errors = errs.v;
            Value s;
            if (codec_run(c, s) != R::Ok || !is_str(s))
                return R::Err;
            if (type == T_INTERNED) {
                StrObj *o = str_intern(str_of(s)->str());
                if (!o)
                    return oom();
                s = obj_value(o);
            }
            got = s;
            if (!keep(got.v))
                return oom();
            break;
        }
        case T_SMALL_TUPLE:
        case T_TUPLE: {
            if (type == T_SMALL_TUPLE) {
                n = rd.r_byte();
                if (n < 0)
                    return R::Err;
            } else {
                if (!rd.r_long(n))
                    return R::Err;
                if (n < 0 || n > SIZE32)
                    return err_set("ValueError", "bad marshal data (tuple size out of range)");
            }
            i64 ref = reserve();
            if (ref == -2 || !open(RK_TUPLE, n, ref))
                return oom();
            break;
        }
        case T_LIST: {
            if (!rd.r_long(n))
                return R::Err;
            if (n < 0 || n > SIZE32)
                return err_set("ValueError", "bad marshal data (list size out of range)");
            // A list is made at once, so a reference inside it can name it.
            if (!open(RK_LIST, n, -1))
                return oom();
            if (flag && !list_push(refs_list(), list_of(stack.v)->items.back()))
                return oom();
            break;
        }
        case T_DICT:
        case T_FROZENDICT: {
            i64 ref = -1;
            if (type == T_DICT) {
                if (!open(RK_DICT, -1, -1))
                    return oom();
                DictObj *d = dict_new();
                if (!d)
                    return oom();
                // The dict itself, first in its frame's list.
                if (!list_push(list_of(list_of(stack.v)->items.back()), obj_value(d)))
                    return oom();
                if (flag && !list_push(refs_list(), obj_value(d)))
                    return oom();
            } else {
                ref = reserve();
                if (ref == -2 || !open(RK_FROZENDICT, -1, ref))
                    return oom();
            }
            break;
        }
        case T_SET:
        case T_FROZENSET: {
            if (!rd.r_long(n))
                return R::Err;
            if (n < 0 || n > SIZE32)
                return err_set("ValueError", "bad marshal data (set size out of range)");
            if (type == T_SET) {
                if (!open(RK_SET, n, -1))
                    return oom();
                SetObj *s = set_new();
                if (!s || !list_push(list_of(list_of(stack.v)->items.back()), obj_value(s)))
                    return oom();
                if (flag && !list_push(refs_list(), obj_value(s)))
                    return oom();
            } else {
                i64 ref = reserve();
                if (ref == -2 || !open(RK_FROZENSET, n, ref))
                    return oom();
            }
            break;
        }
        case T_SLICE: {
            i64 ref = reserve();
            if (ref == -2 || !open(RK_SLICE, 3, ref))
                return oom();
            break;
        }
        case T_CODE:
            if (!rd.allow_code)
                return err_set("ValueError", "unmarshalling code objects is disallowed");
            if (rd.data.size() - rd.at < 4)
                return err_set("EOFError", "marshal data too short");
            return err_set("ValueError", "bad marshal data (unknown type code)");
        case T_REF: {
            if (!rd.r_long(n))
                return R::Err;
            if (n < 0 || usize(n) >= refs_list()->items.size() ||
                is_none(refs_list()->items[usize(n)]))
                return err_set("ValueError", "bad marshal data (invalid reference)");
            got = refs_list()->items[usize(n)];
            break;
        }
        default:
            return err_set("ValueError", "bad marshal data (unknown type code)");
        }

        // A container just opened: its members come next, unless it has none.
        bool opened = got.v.is_nil() && !made_null;
        if (opened && frames[frames.size() - 1].n != 0)
            continue;

        // Deliver what was made, closing every frame it completes.
        for (;;) {
            if (frames.empty()) {
                if (made_null)
                    return err_set("TypeError", "NULL object in marshal data for object");
                out = got.v;
                return R::Ok;
            }
            Frame &f     = frames[frames.size() - 1];
            ListObj *acc = list_of(list_of(stack.v)->items.back());
            bool is_dict = f.kind == RK_DICT || f.kind == RK_FROZENDICT;
            if (!opened) {
                if (made_null && !is_dict) {
                    Str what = f.kind == RK_TUPLE   ? Str("tuple")
                               : f.kind == RK_LIST  ? Str("list")
                               : f.kind == RK_SLICE ? Str("object")
                                                    : Str("set");
                    Buf<64> m;
                    m.put("NULL object in marshal data for ").put(what);
                    return err_set("TypeError", m.str());
                }
                if (!made_null) {
                    if (f.kind == RK_SET) {
                        if (set_add(set_at(acc->items[0]), got.v) != R::Ok)
                            return R::Err;
                    } else if (!list_push(acc, got.v)) {
                        return oom();
                    }
                    if (!is_dict)
                        f.n--;
                }
            }
            opened = false;
            // A dict ends at its NULL; its members come in pairs.
            bool done = is_dict ? made_null : f.n == 0;
            if (is_dict && !made_null) {
                usize base = f.kind == RK_DICT ? 1 : 0;
                if ((acc->items.size() - base) == 2) {
                    Value target = f.kind == RK_DICT ? acc->items[0] : Value();
                    if (f.kind == RK_DICT) {
                        if (dict_set(static_cast<DictObj *>(target.obj()), acc->items[1],
                                     acc->items[2]) != R::Ok)
                            return R::Err;
                        acc->items.pop();
                        acc->items.pop();
                    }
                }
            }
            if (!done)
                break;
            made_null = false;
            // Close the frame.
            Root result;
            Frame closed = f;
            Root members{ list_of(stack.v)->items.back() };
            list_of(stack.v)->items.pop();
            frames.pop();
            ListObj *ms = list_of(members.v);
            switch (closed.kind) {
            case RK_TUPLE: {
                TupleObj *t = tuple_new(ms->items.size());
                if (!t)
                    return oom();
                for (usize i = 0; i < list_of(members.v)->items.size(); i++)
                    t->items()[i] = list_of(members.v)->items[i];
                result = obj_value(t);
                break;
            }
            case RK_LIST:
                result = members.v;
                break;
            case RK_DICT:
                result = ms->items[0];
                break;
            case RK_FROZENDICT: {
                DictObj *d = dict_new();
                if (!d)
                    return oom();
                Root rdict{ obj_value(d) };
                for (usize i = 0; i + 1 < list_of(members.v)->items.size(); i += 2)
                    if (dict_set(static_cast<DictObj *>(rdict.v.obj()),
                                 list_of(members.v)->items[i],
                                 list_of(members.v)->items[i + 1]) != R::Ok)
                        return R::Err;
                rdict.v.obj()->type = &frozendict_type;
                result              = rdict.v;
                break;
            }
            case RK_SET:
                result = ms->items[0];
                break;
            case RK_FROZENSET: {
                SetObj *s = frozenset_new();
                if (!s)
                    return oom();
                Root rs{ obj_value(s) };
                for (Value m : list_of(members.v)->items)
                    if (set_add(set_at(rs.v), m) != R::Ok)
                        return R::Err;
                result = rs.v;
                break;
            }
            default: {
                result = slice_new(ms->items[0], ms->items[1], ms->items[2]);
                if (result.v.is_nil())
                    return R::Err;
                break;
            }
            }
            if (closed.ref >= 0)
                refs_list()->items[usize(closed.ref)] = result.v;
            got = result.v;
        }
    }
}

// Frozen dicts and sets fill their frame's list with members, not with the
// object itself; a set's first slot is the set. `open` for RK_SET and RK_DICT
// put that object in before any member arrives, so the member count above
// never includes it.

// ---------------------------------------------------------------- the module

bool version_arg(Value v, i32 &out)
{
    i64 n = 0;
    if (!as_index(v, n)) {
        if (is_float(v))
            return err_set("TypeError", "'float' object cannot be interpreted as an integer") ==
                   R::Ok;
        return err_not_index(v) == R::Ok;
    }
    out = n > 2147483647 ? 2147483647 : n < -2147483647 ? -2147483647 : i32(n);
    return true;
}

bool allow_code_of(const CallArgs &a, Str who, bool &out)
{
    out = true;
    for (u32 k = 0; k < a.nkw; k++) {
        Str nm = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        if (nm != "allow_code") {
            Buf<96> b;
            b.put(who).put("() got an unexpected keyword argument '").put(nm).put('\'');
            return err_set("TypeError", b.str()) == R::Ok;
        }
        out = py_truth(a.kwvals[k]);
    }
    return true;
}

R positional(const CallArgs &a, Str who, u32 least, u32 most)
{
    if (a.nargs < least || a.nargs > most) {
        char t1[24], t2[24];
        Buf<128> b;
        b.put(who).put("() takes ");
        if (least == most)
            b.put("exactly ").put(int_text(t1, sizeof t1, least));
        else if (a.nargs < least)
            b.put("at least ").put(int_text(t1, sizeof t1, least));
        else
            b.put("at most ").put(int_text(t1, sizeof t1, most));
        b.put(" positional argument").put((a.nargs < least ? least : most) == 1 ? "" : "s");
        b.put(" (").put(int_text(t2, sizeof t2, a.nargs)).put(" given)");
        return err_set("TypeError", b.str());
    }
    return R::Ok;
}

R m_dumps(const CallArgs &a, Value &out)
{
    bool allow  = true;
    i32 version = VERSION;
    if (positional(a, "dumps", 1, 2) != R::Ok || !allow_code_of(a, "dumps", allow) ||
        (a.nargs > 1 && !version_arg(a.args[1], version)))
        return R::Err;
    return dumps_to(a.args[0], version, allow, out);
}

R m_loads(const CallArgs &a, Value &out)
{
    bool allow = true;
    if (positional(a, "loads", 1, 1) != R::Ok || !allow_code_of(a, "loads", allow))
        return R::Err;
    Str data;
    if (!buffer_like(a.args[0], data)) {
        Buf<96> b;
        b.put("a bytes-like object is required, not '").put(type_name(a.args[0])).put('\'');
        return err_set("TypeError", b.str());
    }
    // A copy: the input may be a bytearray the program changes.
    String own;
    if (!own.append(data))
        return oom();
    Reader rd;
    rd.data       = own.str();
    rd.allow_code = allow;
    return load(rd, out);
}

// dump(value, file[, version]): file.write(dumps(value)).
R dump_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_method(k, k->s[0], "write", 1, k->s[1]);
    return cont_done(k, in);
}

R m_dump(const CallArgs &a, Value &out)
{
    bool allow  = true;
    i32 version = VERSION;
    if (positional(a, "dump", 2, 3) != R::Ok || !allow_code_of(a, "dump", allow) ||
        (a.nargs > 2 && !version_arg(a.args[2], version)))
        return R::Err;
    Root data;
    if (dumps_to(a.args[0], version, allow, data.v) != R::Ok)
        return R::Err;
    Root file{ a.args[1] };
    Root kv{ cont_new(dump_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = file.v;
    cont_of(kv.v)->s[1] = data.v;
    out                 = kv.v;
    return R::Ok;
}

// load(file): the whole rest of the file is read, then one object taken from
// it and the file put back where that object ended. s[0] the file, s[1] what
// was read, s[2] the object; i the state.
R load_step(ContObj *k, Value in)
{
    switch (k->i++) {
    case 0:
        return cont_method(k, k->s[0], "read", 1, Value::of_int(0));
    case 1:
        if (!is_bytes(in)) {
            Buf<128> b;
            b.put("file.read() returned not bytes but ").put(type_name(in));
            return err_set("TypeError", b.str());
        }
        return cont_method(k, k->s[0], "read", 0);
    case 2: {
        Str data;
        if (!buffer_like(in, data)) {
            Buf<128> b;
            b.put("file.read() returned not bytes but ").put(type_name(in));
            return err_set("TypeError", b.str());
        }
        k->s[1] = in;
        String own;
        if (!own.append(data))
            return oom();
        Reader rd;
        rd.data       = own.str();
        rd.allow_code = k->j != 0;
        Value got;
        if (load(rd, got) != R::Ok)
            return R::Err;
        k->s[2] = got;
        // Put the file back where the object ended.
        i64 unread = i64(data.size() - rd.at);
        if (unread == 0)
            return cont_done(k, got);
        return cont_method(k, k->s[0], "seek", 2, int_from_i64(-unread), Value::of_int(1));
    }
    default:
        return cont_done(k, k->s[2]);
    }
}

R m_load(const CallArgs &a, Value &out)
{
    bool allow = true;
    if (positional(a, "load", 1, 1) != R::Ok || !allow_code_of(a, "load", allow))
        return R::Err;
    Root file{ a.args[0] };
    Root kv{ cont_new(load_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = file.v;
    cont_of(kv.v)->j    = allow ? 1 : 0;
    out                 = kv.v;
    return R::Ok;
}

constexpr ModDef DEFS[] = {
    { "dump", m_dump },
    { "load", m_load },
    { "dumps", m_dumps },
    { "loads", m_loads },
};

} // namespace

bool marshal_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    return mod_defs(static_cast<DictObj *>(rd.v.obj()), DEFS) &&
           mod_int(static_cast<DictObj *>(rd.v.obj()), "version", VERSION);
}
