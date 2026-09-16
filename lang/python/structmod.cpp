// `_struct`: values to octets and back, over the format language §7 of the
// library reference states.
//
// One table drives everything: a code, its width, and which of the four kinds
// it is. Byte order is a prefix -- `@` native with alignment, `=` native
// without, `<` and `>` explicit, `!` network -- and everything but `@` packs
// without padding, which is the whole of the difference here.
#include "bigint.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "iter.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// struct.error is a class of its own, which the library catches by name. It
// is made once and kept, because every diagnostic here raises an instance.
struct Home {
    Value error;
};

Home *home;

void home_mark()
{
    if (home)
        gc_mark(home->error);
}

Value error_class()
{
    if (!home) {
        home = heap_new<Home>();
        if (!home)
            return oom(), Value();
        gc_root_hook(home_mark);
    }
    if (!home->error.is_nil())
        return home->error;
    Root name{ str_new("error") };
    Root base{ exc_type_value(exc_find("Exception")) };
    if (name.v.is_nil() || base.v.is_nil())
        return Value();
    TupleObj *bases = tuple_new(1);
    if (!bases)
        return oom(), Value();
    bases->items()[0] = base.v;
    Root rb{ obj_value(bases) };
    DictObj *body = dict_new();
    if (!body)
        return oom(), Value();
    Root rd{ obj_value(body) };
    Value cls = type_new(name.v, rb.v, rd.v);
    if (cls.is_nil())
        return Value();
    home->error = cls;
    return cls;
}

R bad(Str message)
{
    Root cls{ error_class() };
    if (cls.v.is_nil())
        return R::Err;
    Root text{ str_new(message) };
    if (text.v.is_nil())
        return R::Err;
    TupleObj *args = tuple_new(1);
    if (!args)
        return oom();
    args->items()[0] = text.v;
    Root ra{ obj_value(args) };
    Value e = exc_inst(cls.v, ra.v);
    return e.is_nil() ? R::Err : err_set_value(e);
}

enum : u8 { K_PAD, K_INT, K_UINT, K_BOOL, K_FLOAT, K_CHAR, K_BYTES, K_PASCAL };

struct Code {
    char c;
    u8 width;
    u8 kind;
};

// `n` and `N` are size_t here, which is four octets on wasm32; `P` is a
// pointer, and the same width.
constexpr Code CODES[] = {
    { 'x', 1, K_PAD },   { 'c', 1, K_CHAR },  { 'b', 1, K_INT },    { 'B', 1, K_UINT },
    { '?', 1, K_BOOL },  { 'h', 2, K_INT },   { 'H', 2, K_UINT },   { 'i', 4, K_INT },
    { 'I', 4, K_UINT },  { 'l', 4, K_INT },   { 'L', 4, K_UINT },   { 'q', 8, K_INT },
    { 'Q', 8, K_UINT },  { 'n', 4, K_INT },   { 'N', 4, K_UINT },   { 'f', 4, K_FLOAT },
    { 'd', 8, K_FLOAT }, { 's', 1, K_BYTES }, { 'p', 1, K_PASCAL }, { 'P', 4, K_UINT },
};

const Code *code_of(char c)
{
    for (const Code &one : CODES)
        if (one.c == c)
            return &one;
    return nullptr;
}

// One item of a compiled format: where it sits, how wide, and how many.
struct Item {
    const Code *code;
    usize at;
    usize count; // the repeat, or the length for s and p
};

struct Format {
    Vec<Item> items;
    usize size;
    bool little;
    bool align;
};

bool host_is_little()
{
    // wasm32 is little-endian, and the ABI says so; this is written out
    // rather than assumed so the one place it matters is named.
    return true;
}

// The format string, walked once. `out` takes the items and the total size.
bool compile_format(Str f, Format &out)
{
    out.little = host_is_little();
    out.align  = false;
    usize at   = 0;
    if (f.size()) {
        char c = f[0];
        if (c == '@') {
            out.align = true;
            at        = 1;
        } else if (c == '=') {
            at = 1;
        } else if (c == '<') {
            out.little = true;
            at         = 1;
        } else if (c == '>' || c == '!') {
            out.little = false;
            at         = 1;
        } else {
            out.align = true; // no prefix is `@`
        }
    } else {
        out.align = true;
    }

    usize off = 0;
    while (at < f.size()) {
        char c = f[at];
        if (c == ' ') {
            at++;
            continue;
        }
        usize count = 0;
        bool given  = false;
        while (at < f.size() && f[at] >= '0' && f[at] <= '9') {
            count = count * 10 + usize(f[at] - '0');
            if (count > (1u << 24))
                return bad("repeat count is too large"), false;
            at++;
            given = true;
        }
        if (at >= f.size())
            return bad("repeat count given without a format"), false;
        c                = f[at++];
        const Code *code = code_of(c);
        if (!code)
            return bad("bad char in struct format"), false;
        if (!given)
            count = 1;

        // `@` pads each item out to its own width, which is what the C
        // compiler would have done to the struct.
        usize width = code->width;
        if (out.align && width > 1 && code->kind != K_BYTES && code->kind != K_PASCAL) {
            usize pad = off % width;
            if (pad)
                off += width - pad;
        }
        if (code->kind == K_BYTES || code->kind == K_PASCAL) {
            if (!out.items.push(Item{ code, off, count }))
                return oom() == R::Ok;
            off += count;
            continue;
        }
        if (code->kind == K_PAD) {
            off += count;
            continue;
        }
        for (usize i = 0; i < count; i++) {
            if (!out.items.push(Item{ code, off, 1 }))
                return oom() == R::Ok;
            off += width;
        }
    }
    out.size = off;
    return true;
}

// How many values the format takes or answers.
usize value_count(const Format &f)
{
    return f.items.size();
}

void put_uint(String &to, usize at, u64 v, usize width, bool little)
{
    char *p = const_cast<char *>(to.str().data()) + at;
    for (usize i = 0; i < width; i++) {
        usize k = little ? i : width - 1 - i;
        p[k]    = char(u8(v >> (8 * i)));
    }
}

u64 take_uint(Str from, usize at, usize width, bool little)
{
    u64 v = 0;
    for (usize i = 0; i < width; i++) {
        usize k = little ? i : width - 1 - i;
        v |= u64(u8(from[at + k])) << (8 * i);
    }
    return v;
}

// A float, as its octets. wasm32 is IEEE and so is Python's float, so this is
// a reinterpretation rather than a conversion.
u64 f64_bits(f64 x)
{
    u64 out = 0;
    __builtin_memcpy(&out, &x, 8);
    return out;
}

f64 bits_f64(u64 b)
{
    f64 out = 0;
    __builtin_memcpy(&out, &b, 8);
    return out;
}

u32 f32_bits(f32 x)
{
    u32 out = 0;
    __builtin_memcpy(&out, &x, 4);
    return out;
}

f32 bits_f32(u32 b)
{
    f32 out = 0;
    __builtin_memcpy(&out, &b, 4);
    return out;
}

// Eight octets unsigned reaches past i64, so the magnitude is read out of the
// bignum rather than through as_index.
bool as_u64(Value v, u64 &out)
{
    i64 n = 0;
    if (as_index(v, n)) {
        if (n < 0)
            return false;
        out = u64(n);
        return true;
    }
    if (!is_big(v) || big_of(v)->neg || big_of(v)->len > 2)
        return false;
    out = big_of(v)->limbs()[0];
    if (big_of(v)->len > 1)
        out |= u64(big_of(v)->limbs()[1]) << 32;
    return true;
}

bool fits(const Code *c, i64 v)
{
    if (c->kind == K_UINT) {
        if (v < 0)
            return false;
        return c->width >= 8 || u64(v) < (u64(1) << (8 * c->width));
    }
    if (c->width >= 8)
        return true;
    i64 top = i64(1) << (8 * c->width - 1);
    return v >= -top && v < top;
}

// One value into the buffer. `to` is already the right length.
R pack_one(String &to, const Item &it, const Format &f, Value v)
{
    const Code *c = it.code;
    switch (c->kind) {
    case K_BOOL:
        put_uint(to, it.at, py_truth(v) ? 1 : 0, 1, f.little);
        return R::Ok;
    case K_FLOAT: {
        f64 x = 0;
        if (!as_number(v, x))
            return bad("required argument is not a float");
        if (c->width == 4)
            put_uint(to, it.at, f32_bits(f32(x)), 4, f.little);
        else
            put_uint(to, it.at, f64_bits(x), 8, f.little);
        return R::Ok;
    }
    case K_CHAR: {
        Str octets;
        if (!bytes_like(v, octets) || octets.size() != 1)
            return bad("char format requires bytes of length 1");
        put_uint(to, it.at, u64(u8(octets[0])), 1, f.little);
        return R::Ok;
    }
    case K_BYTES:
    case K_PASCAL: {
        Str octets;
        if (!bytes_like(v, octets))
            return bad("argument for 's' must be a bytes object");
        usize room  = it.count;
        char *p     = const_cast<char *>(to.str().data()) + it.at;
        usize start = 0;
        if (c->kind == K_PASCAL) {
            if (!room)
                return R::Ok;
            usize n = octets.size() > room - 1 ? room - 1 : octets.size();
            p[0]    = char(u8(n));
            start   = 1;
            room    = n + 1;
        }
        for (usize i = start; i < it.count; i++)
            p[i] = i - start < octets.size() ? octets[i - start] : '\0';
        return R::Ok;
    }
    default: {
        if (is_float(v))
            return bad("required argument is not an integer");
        if (c->kind == K_UINT && c->width >= 8) {
            u64 u = 0;
            if (!as_u64(v, u))
                return bad("argument out of range");
            put_uint(to, it.at, u, 8, f.little);
            return R::Ok;
        }
        i64 n = 0;
        if (!as_index(v, n)) {
            // A bignum that does not fit i64 is out of range for every code.
            if (is_big(v))
                return bad("argument out of range");
            return bad("required argument is not an integer");
        }
        if (!fits(c, n))
            return bad("argument out of range");
        put_uint(to, it.at, u64(n), c->width, f.little);
        return R::Ok;
    }
    }
}

R unpack_one(Str from, const Item &it, const Format &f, Value &out)
{
    const Code *c = it.code;
    switch (c->kind) {
    case K_BOOL:
        out = value_bool(take_uint(from, it.at, 1, f.little) != 0);
        return R::Ok;
    case K_FLOAT:
        out = float_new(c->width == 4 ? f64(bits_f32(u32(take_uint(from, it.at, 4, f.little))))
                                      : bits_f64(take_uint(from, it.at, 8, f.little)));
        return out.is_nil() ? R::Err : R::Ok;
    case K_CHAR: {
        char one = from[it.at];
        out      = bytes_new(Str(&one, 1));
        return out.is_nil() ? R::Err : R::Ok;
    }
    case K_BYTES:
        out = bytes_new(from.substr(it.at, it.count));
        return out.is_nil() ? R::Err : R::Ok;
    case K_PASCAL: {
        usize n = it.count ? usize(u8(from[it.at])) : 0;
        if (n > it.count - 1)
            n = it.count ? it.count - 1 : 0;
        out = bytes_new(from.substr(it.at + 1, n));
        return out.is_nil() ? R::Err : R::Ok;
    }
    case K_UINT: {
        u64 v = take_uint(from, it.at, c->width, f.little);
        // Eight octets unsigned is past i64, so the bignum takes it.
        if (c->width >= 8 && (v >> 63)) {
            u32 limbs[2] = { u32(v), u32(v >> 32) };
            out          = big_make(limbs, 2, false);
        } else {
            out = int_from_i64(i64(v));
        }
        return out.is_nil() ? R::Err : R::Ok;
    }
    default: {
        u64 raw = take_uint(from, it.at, c->width, f.little);
        i64 v   = 0;
        if (c->width >= 8) {
            v = i64(raw);
        } else {
            u64 sign = u64(1) << (8 * c->width - 1);
            v        = i64(raw & (sign - 1)) - i64(raw & sign);
        }
        out = int_from_i64(v);
        return out.is_nil() ? R::Err : R::Ok;
    }
    }
}

// The compiled format a call works from. `Struct` keeps one; the plain
// functions compile theirs and throw it away.
struct StructObj : Obj {
    Value text; // the format as it was given
    Format f;
};

StructObj *struct_of(Value v)
{
    return static_cast<StructObj *>(v.obj());
}

void struct_trace(Obj *o)
{
    gc_mark(static_cast<StructObj *>(o)->text);
}

void struct_fini(Obj *o)
{
    static_cast<StructObj *>(o)->f.items.~Vec();
}

R struct_repr(Value v, String &out)
{
    if (!out.append("Struct("))
        return oom();
    if (py_repr(struct_of(v)->text, out) != R::Ok)
        return R::Err;
    return out.push(')') ? R::Ok : oom();
}

R struct_getattr(Value v, StrObj *name, Value &out)
{
    Str n = name->str();
    if (n == "format")
        out = struct_of(v)->text;
    else if (n == "size")
        out = int_from_i64(i64(struct_of(v)->f.size));
    else
        return R::NotImpl;
    return out.is_nil() ? R::Err : R::Ok;
}

extern const Type struct_type;

// The format of a call: a str, a bytes, or a Struct already compiled.
bool take_format(Value v, Format &out, StructObj **kept)
{
    *kept = nullptr;
    if (v.is_obj() && v.obj()->type == &struct_type) {
        *kept = struct_of(v);
        return true;
    }
    Str text;
    if (is_str(v))
        text = str_of(v)->str();
    else if (!bytes_like(v, text))
        return err_set2("TypeError", "a format must be str or bytes", type_name(v)), false;
    return compile_format(text, out);
}

R pack_into(const Format &f, const Value *args, u32 n, String &out)
{
    if (n != value_count(f))
        return bad("wrong number of arguments for this format");
    if (!out.reserve(f.size))
        return oom();
    for (usize i = 0; i < f.size; i++)
        if (!out.push('\0'))
            return oom();
    for (usize i = 0; i < f.items.size(); i++)
        if (pack_one(out, f.items[i], f, args[i]) != R::Ok)
            return R::Err;
    return R::Ok;
}

Value unpack_from(const Format &f, Str octets, usize at)
{
    if (at + f.size > octets.size())
        return bad("buffer is too small for this format"), Value();
    TupleObj *t = tuple_new(value_count(f));
    if (!t)
        return oom(), Value();
    Root rt{ obj_value(t) };
    for (usize i = 0; i < f.items.size(); i++) {
        Item it = f.items[i];
        it.at += at;
        Value one;
        if (unpack_one(octets, it, f, one) != R::Ok)
            return Value();
        static_cast<TupleObj *>(rt.v.obj())->items()[i] = one;
    }
    return rt.v;
}

// The three shapes every entry point takes: the format is argument zero for a
// function and `self` for a method.
R do_calcsize(Value fmt, Value &out)
{
    Format f;
    StructObj *kept = nullptr;
    if (!take_format(fmt, f, &kept))
        return R::Err;
    out = int_from_i64(i64(kept ? kept->f.size : f.size));
    return out.is_nil() ? R::Err : R::Ok;
}

R s_calcsize(const CallArgs &a, Value &out)
{
    if (!args_only(a, "calcsize", 1, 1))
        return R::Err;
    return do_calcsize(a.args[0], out);
}

R s_pack(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs < 1)
        return err_set("TypeError", "pack() needs a format");
    Format f;
    StructObj *kept = nullptr;
    Root rf{ a.args[0] };
    if (!take_format(rf.v, f, &kept))
        return R::Err;
    String text;
    if (pack_into(kept ? kept->f : f, a.args + 1, a.nargs - 1, text) != R::Ok)
        return R::Err;
    out = bytes_new(text.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R s_pack_into(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs < 3)
        return err_set("TypeError", "pack_into() needs a format, a buffer and an offset");
    Format f;
    StructObj *kept = nullptr;
    Root rf{ a.args[0] };
    if (!take_format(rf.v, f, &kept))
        return R::Err;
    i64 at = 0;
    if (!as_index(a.args[2], at))
        return err_set("TypeError", "offset must be an integer");
    String text;
    if (pack_into(kept ? kept->f : f, a.args + 3, a.nargs - 3, text) != R::Ok)
        return R::Err;
    // Only something whose octets may be written through: a bytearray, or a
    // memoryview onto one.
    if (is_bytearray(a.args[1])) {
        ArrayObj *b = array_of(a.args[1]);
        if (at < 0 || usize(at) + text.size() > b->data.size())
            return bad("buffer is too small for this format");
        for (usize i = 0; i < text.size(); i++)
            b->data[usize(at) + i] = u8(text.str()[i]);
    } else if (is_memview(a.args[1])) {
        Str room;
        bool writable = false;
        if (!memview_bytes(a.args[1], room, &writable))
            return R::Err;
        if (!writable)
            return bad("buffer is read-only");
        if (at < 0 || usize(at) + text.size() > room.size())
            return bad("buffer is too small for this format");
        char *p = const_cast<char *>(room.data());
        for (usize i = 0; i < text.size(); i++)
            p[usize(at) + i] = text.str()[i];
    } else {
        return err_set2("TypeError", "pack_into() wants a writable buffer", type_name(a.args[1]));
    }
    out = value_none();
    return R::Ok;
}

R s_unpack(const CallArgs &a, Value &out)
{
    if (!args_only(a, "unpack", 2, 2))
        return R::Err;
    Format f;
    StructObj *kept = nullptr;
    Root rf{ a.args[0] };
    if (!take_format(rf.v, f, &kept))
        return R::Err;
    const Format &use = kept ? kept->f : f;
    Str octets;
    if (!bytes_like(a.args[1], octets))
        return err_set2("TypeError", "unpack() wants a buffer", type_name(a.args[1]));
    if (octets.size() != use.size)
        return bad("unpack requires a buffer of exactly the format's size");
    out = unpack_from(use, octets, 0);
    return out.is_nil() ? R::Err : R::Ok;
}

R s_unpack_from(const CallArgs &a, Value &out)
{
    if (a.nargs < 2 || a.nargs > 3)
        return err_set("TypeError", "unpack_from() takes 2 or 3 arguments");
    Format f;
    StructObj *kept = nullptr;
    Root rf{ a.args[0] };
    if (!take_format(rf.v, f, &kept))
        return R::Err;
    i64 at = 0;
    if (a.nargs > 2 && !as_index(a.args[2], at))
        return err_set("TypeError", "offset must be an integer");
    for (u32 k = 0; k < a.nkw; k++)
        if (is_str(a.kwnames[k]) && str_of(a.kwnames[k])->str() == "offset" &&
            !as_index(a.kwvals[k], at))
            return err_set("TypeError", "offset must be an integer");
    Str octets;
    if (!bytes_like(a.args[1], octets))
        return err_set2("TypeError", "unpack_from() wants a buffer", type_name(a.args[1]));
    if (at < 0)
        at += i64(octets.size());
    if (at < 0)
        return bad("offset is out of range");
    out = unpack_from(kept ? kept->f : f, octets, usize(at));
    return out.is_nil() ? R::Err : R::Ok;
}

R s_iter_unpack(const CallArgs &a, Value &out)
{
    if (!args_only(a, "iter_unpack", 2, 2))
        return R::Err;
    Format f;
    StructObj *kept = nullptr;
    Root rf{ a.args[0] };
    if (!take_format(rf.v, f, &kept))
        return R::Err;
    const Format &use = kept ? kept->f : f;
    Str octets;
    if (!bytes_like(a.args[1], octets))
        return err_set2("TypeError", "iter_unpack() wants a buffer", type_name(a.args[1]));
    if (!use.size)
        return bad("cannot iteratively unpack with a format of size zero");
    if (octets.size() % use.size)
        return bad("the buffer is not a multiple of the format's size");
    // A list of tuples, walked: laziness would buy nothing over a buffer that
    // is already whole in memory.
    ListObj *l = list_new();
    if (!l)
        return oom();
    Root rl{ obj_value(l) };
    for (usize at = 0; at < octets.size(); at += use.size) {
        Value one = unpack_from(use, octets, at);
        if (one.is_nil() || !list_push(list_of(rl.v), one))
            return R::Err;
    }
    out = seq_iter(rl.v);
    return out.is_nil() ? R::Err : R::Ok;
}

// The methods of a compiled Struct, each the function above with self in
// front of it.
R m_forward(const CallArgs &a, Value &out, R (*fn)(const CallArgs &, Value &))
{
    Value args[8];
    Roots pin{ args, 8 };
    if (a.nargs > 8)
        return err_set("TypeError", "too many arguments");
    args[0] = method_self(a.args[0]);
    for (u32 i = 1; i < a.nargs; i++)
        args[i] = a.args[i];
    CallArgs b;
    b.args    = args;
    b.nargs   = a.nargs;
    b.kwnames = a.kwnames;
    b.kwvals  = a.kwvals;
    b.nkw     = a.nkw;
    return fn(b, out);
}

R m_pack(const CallArgs &a, Value &out)
{
    return m_forward(a, out, s_pack);
}

R m_pack_into(const CallArgs &a, Value &out)
{
    return m_forward(a, out, s_pack_into);
}

R m_unpack(const CallArgs &a, Value &out)
{
    return m_forward(a, out, s_unpack);
}

R m_unpack_from(const CallArgs &a, Value &out)
{
    return m_forward(a, out, s_unpack_from);
}

R m_iter_unpack(const CallArgs &a, Value &out)
{
    return m_forward(a, out, s_iter_unpack);
}

constexpr Method STRUCT_METHODS[] = {
    { "pack", m_pack },
    { "pack_into", m_pack_into },
    { "unpack", m_unpack },
    { "unpack_from", m_unpack_from },
    { "iter_unpack", m_iter_unpack },
};

constexpr Type struct_type{ .name    = "Struct",
                            .trace   = struct_trace,
                            .fini    = struct_fini,
                            .repr    = struct_repr,
                            .getattr = struct_getattr };

R b_struct_new(const CallArgs &a, Value &out)
{
    if (!args_only(a, "Struct", 1, 1))
        return R::Err;
    Str text;
    if (is_str(a.args[0]))
        text = str_of(a.args[0])->str();
    else if (!bytes_like(a.args[0], text))
        return err_set2("TypeError", "a format must be str or bytes", type_name(a.args[0]));
    Format f;
    if (!compile_format(text, f))
        return R::Err;
    Root rt{ a.args[0] };
    StructObj *s = static_cast<StructObj *>(obj_alloc(&struct_type, sizeof(StructObj)));
    if (!s)
        return oom();
    s->text = rt.v;
    new (&s->f) Format();
    s->f.size   = f.size;
    s->f.little = f.little;
    s->f.align  = f.align;
    for (usize i = 0; i < f.items.size(); i++)
        if (!s->f.items.push(f.items[i]))
            return oom();
    out = obj_value(s);
    return R::Ok;
}

constexpr ModDef DEFS[] = {
    { "calcsize", s_calcsize },       { "pack", s_pack },
    { "pack_into", s_pack_into },     { "unpack", s_unpack },
    { "unpack_from", s_unpack_from }, { "iter_unpack", s_iter_unpack },
};

} // namespace

bool struct_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!method_install(&struct_type, STRUCT_METHODS))
        return false;
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_defs(d, DEFS) || !mod_type(d, &struct_type, b_struct_new))
        return false;
    // struct.error is its own class, which the library catches by name.
    Root err{ error_class() };
    return !err.v.is_nil() && mod_put(d, "error", err.v) && mod_int(d, "_PY_STRUCT_RANGE_CHECK", 1);
}
