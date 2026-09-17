// The exception hierarchy.
#include "exc.h"

#include "codec.h"
#include "egroup.h"
#include "func.h"
#include "gc.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"
#include "type.h"
#include "ustr.h"

namespace {

// The type objects, one per row of EXC_TABLE, made on first use. They outlive
// every program, so they are a root rather than heap the collector may take.
struct Types {
    Vec<Value> made;
};

Types *types;

void types_mark()
{
    if (!types)
        return;
    for (usize i = 0; i < types->made.size(); i++)
        gc_mark(types->made[i]);
}

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// A type is called to make an instance: ValueError('x').
R exc_type_call(const CallArgs &a, Value &out, Value cls)
{
    Root rc{ cls };
    if (a.nkw)
        return err_set2("TypeError", "exception takes no keyword arguments", type_name(cls));
    TupleObj *args = tuple_new(a.nargs);
    if (!args)
        return oom();
    for (u32 i = 0; i < a.nargs; i++)
        args->items()[i] = a.args[i];
    Root ra{ obj_value(args) };
    out = exc_construct(rc.v, ra.v);
    return out.is_nil() ? R::Err : R::Ok;
}

void exc_trace(Obj *o)
{
    ExcObj *e = static_cast<ExcObj *>(o);
    gc_mark(e->cls);
    gc_mark(e->dict);
    gc_mark(e->args);
    gc_mark(e->cause);
    gc_mark(e->context);
    gc_mark(e->msg);
    gc_mark(e->excs);
    for (Value v : e->uni)
        gc_mark(v);
}

usize args_len(Value t)
{
    return static_cast<TupleObj *>(t.obj())->len;
}

Value args_item(Value t, usize i)
{
    return static_cast<TupleObj *>(t.obj())->items()[i];
}

TupleObj *args_of(Value v)
{
    return static_cast<TupleObj *>(static_cast<ExcObj *>(v.obj())->args.obj());
}

R unierr_str(Value v, UniKind kind, String &out);

// "[Errno 2] No such file or directory: 'x'"; NotImpl where the arguments
// were not an errno and a message.
R oserror_str(Value v, String &out)
{
    Root rv{ v };
    ExcObj *e = static_cast<ExcObj *>(v.obj());
    Value no = e->uni[OS_ERRNO], text = e->uni[OS_STRERROR];
    Value f1 = e->uni[OS_FILENAME], f2 = e->uni[OS_FILENAME2];
    if (f1.is_nil() && (no.is_nil() || text.is_nil()))
        return R::NotImpl;
    if (!out.append("[Errno "))
        return oom();
    Root rn{ no }, rt{ text }, r1{ f1 }, r2{ f2 };
    if (py_str(rn.v.is_nil() ? value_none() : rn.v, out) != R::Ok || !out.append("] ") ||
        py_str(rt.v.is_nil() ? value_none() : rt.v, out) != R::Ok)
        return R::Err;
    if (r1.v.is_nil())
        return R::Ok;
    if (!out.append(": ") || py_repr(r1.v, out) != R::Ok)
        return R::Err;
    if (r2.v.is_nil())
        return R::Ok;
    if (!out.append(" -> ") || py_repr(r2.v, out) != R::Ok)
        return R::Err;
    return R::Ok;
}

// "'str' object cannot be interpreted as an integer"
R not_index(Value v)
{
    Buf<96> m;
    m.put('\'').put(type_name(v)).put("' object cannot be interpreted as an integer");
    return err_set("TypeError", m.str());
}

// repr is `ValueError('x')`; str is what the arguments say, which for one
// argument is that argument and for none is empty.
R exc_repr(Value v, String &out)
{
    TupleObj *a = args_of(v);
    ExcObj *e   = static_cast<ExcObj *>(v.obj());
    if (!e->excs.is_nil()) {
        // A group shows its members as a list where it was given one.
        Root rv{ v };
        if (!out.append(type_name(rv.v)) || !out.push('('))
            return oom();
        if (py_repr(static_cast<ExcObj *>(rv.v.obj())->msg, out) != R::Ok || !out.append(", "))
            return R::Err;
        a            = args_of(rv.v);
        bool as_list = a->len == 2 && is_list(a->items()[1]);
        TupleObj *xs = static_cast<TupleObj *>(static_cast<ExcObj *>(rv.v.obj())->excs.obj());
        if (as_list) {
            if (!out.push('['))
                return oom();
            for (usize i = 0; i < xs->len; i++) {
                if (i && !out.append(", "))
                    return oom();
                if (py_repr(xs->items()[i], out) != R::Ok)
                    return R::Err;
            }
            if (!out.push(']'))
                return oom();
        } else if (py_repr(static_cast<ExcObj *>(rv.v.obj())->excs, out) != R::Ok) {
            return R::Err;
        }
        return out.push(')') ? R::Ok : oom();
    }
    if (!out.append(type_name(v)) || !out.push('('))
        return oom();
    for (usize i = 0; i < a->len; i++) {
        if (i && !out.append(", "))
            return oom();
        if (py_repr(a->items()[i], out) != R::Ok)
            return R::Err;
    }
    return out.push(')') ? R::Ok : oom();
}

// SyntaxError(msg, (filename, lineno, offset, text)): the message, and
// where, as CPython's __str__ says it.
TupleObj *syntax_details(Value v)
{
    TupleObj *a = args_of(v);
    if (!exc_is(exc_type_of(v), exc_find("SyntaxError")) || a->len != 2 || !is_tuple(a->items()[1]))
        return nullptr;
    TupleObj *d = static_cast<TupleObj *>(a->items()[1].obj());
    return d->len >= 4 ? d : nullptr;
}

R exc_str(Value v, String &out)
{
    TupleObj *a = args_of(v);
    ExcObj *eg  = static_cast<ExcObj *>(v.obj());
    if (!eg->excs.is_nil()) {
        usize n = static_cast<TupleObj *>(eg->excs.obj())->len;
        char tmp[24];
        if (py_str(eg->msg, out) != R::Ok)
            return R::Err;
        bool ok = out.append(" (") && out.append(int_text(tmp, sizeof tmp, i64(n))) &&
                  out.append(n > 1 ? " sub-exceptions)" : " sub-exception)");
        return ok ? R::Ok : oom();
    }
    UniKind uk = unierr_kind(v);
    if (uk != UniKind::None)
        return unierr_str(v, uk, out);
    if (is_oserror(v)) {
        R r = oserror_str(v, out);
        if (r != R::NotImpl)
            return r;
    }
    if (a->len == 0)
        return R::Ok;
    if (TupleObj *d = syntax_details(v)) {
        if (py_str(a->items()[0], out) != R::Ok)
            return R::Err;
        Value file = d->items()[0], line = d->items()[1];
        bool has_line = line.is_int();
        if (is_str(file)) {
            Str f     = str_of(file)->str();
            usize cut = f.size();
            while (cut && f[cut - 1] != '/')
                cut--;
            if (!out.append(" (") || !out.append(f.substr(cut)))
                return oom();
            if (has_line) {
                char tmp[24];
                if (!out.append(", line ") ||
                    !out.append(int_text(tmp, sizeof tmp, i64(line.as_int()))))
                    return oom();
            }
            return out.push(')') ? R::Ok : oom();
        }
        if (has_line) {
            char tmp[24];
            if (!out.append(" (line ") ||
                !out.append(int_text(tmp, sizeof tmp, i64(line.as_int()))) || !out.push(')'))
                return oom();
        }
        return R::Ok;
    }
    // A KeyError names a key, and a key is shown as its repr: KeyError: 'x'.
    if (a->len == 1)
        return exc_is(exc_type_of(v), exc_find("KeyError")) ? py_repr(a->items()[0], out)
                                                            : py_str(a->items()[0], out);
    return py_repr(static_cast<ExcObj *>(v.obj())->args, out);
}

// The class's own names come first -- py_attr asks this only after those.
R exc_getattr(Value v, StrObj *name, Value &out)
{
    ExcObj *e = static_cast<ExcObj *>(v.obj());
    Str n     = name->str();
    if (n == "args") {
        out = e->args;
        return R::Ok;
    }
    if (n == "__cause__") {
        out = e->cause.is_nil() ? value_none() : e->cause;
        return R::Ok;
    }
    if (n == "__context__") {
        out = e->context.is_nil() ? value_none() : e->context;
        return R::Ok;
    }
    // The two the built-in subclasses carry, which upstream's tests read.
    if (n == "value" && exc_is(e->t, exc_find("StopIteration"))) {
        TupleObj *a = args_of(v);
        out         = a->len ? a->items()[0] : value_none();
        return R::Ok;
    }
    if (exc_is(e->t, exc_find("SyntaxError"))) {
        TupleObj *a            = args_of(v);
        TupleObj *d            = syntax_details(v);
        constexpr Str FIELDS[] = { "filename", "lineno", "offset", "text" };
        if (n == "msg") {
            out = a->len ? a->items()[0] : value_none();
            return R::Ok;
        }
        for (u32 k = 0; k < 4; k++)
            if (n == FIELDS[k]) {
                out = d ? d->items()[k] : value_none();
                return R::Ok;
            }
        if (n == "end_lineno" || n == "end_offset" || n == "print_file_and_line") {
            out = value_none();
            return R::Ok;
        }
    }
    if (UniKind uk = unierr_kind(v); uk != UniKind::None) {
        constexpr Str FIELDS[] = { "encoding", "object", "start", "end", "reason" };
        for (u32 k = 0; k < 5; k++)
            if (n == FIELDS[k] && !(k == UNI_ENCODING && uk == UniKind::Translate)) {
                Value f = e->uni[k];
                out     = !f.is_nil()                        ? f
                          : (k == UNI_START || k == UNI_END) ? Value::of_int(0)
                                                             : value_none();
                return R::Ok;
            }
    }
    if (!e->excs.is_nil() && (n == "message" || n == "exceptions")) {
        out = n == "message" ? e->msg : e->excs;
        return R::Ok;
    }
    if (is_oserror(v)) {
        constexpr Str FIELDS[] = { "errno", "strerror", "filename", "filename2" };
        for (u32 k = 0; k < 4; k++)
            if (n == FIELDS[k]) {
                out = e->uni[k].is_nil() ? value_none() : e->uni[k];
                return R::Ok;
            }
        if (n == "characters_written" && !e->uni[OS_WRITTEN].is_nil()) {
            out = e->uni[OS_WRITTEN];
            return R::Ok;
        }
    }
    return R::NotImpl;
}

// BaseException.__init__(self, *args): what a subclass reaches through super().
R b_exc_init(const CallArgs &a, Value &out)
{
    if (a.nkw || !a.nargs || !is_exc(a.args[0]))
        return err_set("TypeError", "BaseException.__init__() needs an exception");
    TupleObj *args = tuple_new(a.nargs - 1);
    if (!args)
        return oom();
    for (u32 i = 1; i < a.nargs; i++)
        args->items()[i - 1] = a.args[i];
    static_cast<ExcObj *>(a.args[0].obj())->args = obj_value(args);
    if (unierr_kind(a.args[0]) != UniKind::None && !unierr_init(a.args[0], obj_value(args)))
        return R::Err;
    if (is_oserror(a.args[0]) && !oserror_init(a.args[0]))
        return R::Err;
    out = value_none();
    return R::Ok;
}

// "'utf-8' codec can't encode character '\ud800' in position 0: surrogates
// not allowed", and its three relatives.
R unierr_str(Value v, UniKind kind, String &out)
{
    ExcObj *e = static_cast<ExcObj *>(v.obj());
    if (e->uni[UNI_OBJECT].is_nil())
        return R::Ok;
    Root rv{ v };
    String reason, encoding;
    if (py_str(e->uni[UNI_REASON], reason) != R::Ok)
        return R::Err;
    e = static_cast<ExcObj *>(rv.v.obj());
    if (kind != UniKind::Translate && py_str(e->uni[UNI_ENCODING], encoding) != R::Ok)
        return R::Err;
    e             = static_cast<ExcObj *>(rv.v.obj());
    Value obj     = e->uni[UNI_OBJECT];
    bool as_bytes = kind == UniKind::Decode;
    if (as_bytes ? !is_bytes(obj) : !is_str(obj))
        return err_set2("TypeError",
                        as_bytes ? Str("object attribute must be bytes")
                                 : Str("object attribute must be unicode"),
                        type_name(obj));
    i64 len   = as_bytes ? i64(static_cast<BytesObj *>(obj.obj())->len) : i64(str_of(obj)->chars);
    i64 start = 0, end = 0;
    as_index(e->uni[UNI_START], start);
    as_index(e->uni[UNI_END], end);
    Buf<160> b;
    if (kind != UniKind::Translate)
        b.put('\'').put(encoding.str()).put("' codec can't ");
    else
        b.put("can't ");
    b.put(kind == UniKind::Encode   ? Str("encode ")
          : kind == UniKind::Decode ? Str("decode ")
                                    : Str("translate "));
    if (start >= 0 && start < len && end >= 0 && end <= len && end == start + 1) {
        if (as_bytes) {
            u8 bad = static_cast<BytesObj *>(obj.obj())->data()[start];
            put_hexw(b.put("byte 0x"), bad, 2);
        } else {
            u32 bad = str_char_at(str_of(obj), usize(start));
            b.put("character '\\");
            if (bad <= 0xff)
                put_hexw(b.put('x'), bad, 2);
            else if (bad <= 0xffff)
                put_hexw(b.put('u'), bad, 4);
            else
                put_hexw(b.put('U'), bad, 8);
            b.put('\'');
        }
        put_i64(b.put(" in position "), start);
    } else {
        put_i64(b.put(as_bytes ? Str("bytes") : Str("characters")).put(" in position "), start);
        put_i64(b.put('-'), end - 1);
    }
    b.put(": ");
    return out.append(b.str()) && out.append(reason.str()) ? R::Ok : oom();
}

} // namespace

UniKind unierr_kind(Value v)
{
    const ExcType *t = exc_type_of(v);
    if (!t)
        return UniKind::None;
    if (exc_is(t, exc_find("UnicodeEncodeError")))
        return UniKind::Encode;
    if (exc_is(t, exc_find("UnicodeDecodeError")))
        return UniKind::Decode;
    if (exc_is(t, exc_find("UnicodeTranslateError")))
        return UniKind::Translate;
    return UniKind::None;
}

bool unierr_init(Value e, Value args)
{
    UniKind kind = unierr_kind(e);
    TupleObj *a  = static_cast<TupleObj *>(args.obj());
    u32 want     = kind == UniKind::Translate ? 4 : 5;
    if (a->len != want) {
        char tmp[24];
        Buf<96> b;
        b.put("function takes exactly ").put(int_text(tmp, sizeof tmp, i64(want)));
        b.put(" arguments (").put(int_text(tmp, sizeof tmp, i64(a->len))).put(" given)");
        return err_set("TypeError", b.str()), false;
    }
    Root re{ e }, ra{ args };
    Value got[5];
    u32 at = 0;
    if (kind != UniKind::Translate)
        got[UNI_ENCODING] = a->items()[at++];
    else
        got[UNI_ENCODING] = Value();
    got[UNI_OBJECT] = a->items()[at++];
    got[UNI_START]  = a->items()[at++];
    got[UNI_END]    = a->items()[at++];
    got[UNI_REASON] = a->items()[at++];
    auto must_str   = [&](Value v, int n) {
        if (is_str(v))
            return true;
        char tmp[24];
        Buf<96> b;
        b.put("argument ").put(int_text(tmp, sizeof tmp, i64(n))).put(" must be str");
        return err_not(b.str(), v) == R::Ok;
    };
    int first = kind == UniKind::Translate ? 0 : 1;
    if (kind != UniKind::Translate && !must_str(got[UNI_ENCODING], 1))
        return false;
    if (kind == UniKind::Decode) {
        Str data;
        if (!bytes_like(got[UNI_OBJECT], data))
            return err_not("a bytes-like object is required", got[UNI_OBJECT], true), false;
        if (!is_bytes(got[UNI_OBJECT])) {
            got[UNI_OBJECT] = bytes_new(data);
            if (got[UNI_OBJECT].is_nil())
                return false;
        }
    } else if (!must_str(got[UNI_OBJECT], first + 1)) {
        return false;
    }
    i64 n = 0;
    for (u32 k = UNI_START; k <= UNI_END; k++)
        if (!as_index(got[k], n))
            return not_index(got[k]), false;
    if (!must_str(got[UNI_REASON], first + 4))
        return false;
    ExcObj *o = static_cast<ExcObj *>(re.v.obj());
    for (u32 k = 0; k < 5; k++)
        o->uni[k] = got[k];
    return true;
}

R unierr_store(Value e, Str name, Value v)
{
    constexpr Str NAMES[] = { "encoding", "object", "start", "end", "reason" };
    UniKind kind          = unierr_kind(e);
    if (kind == UniKind::None)
        return R::NotImpl;
    for (u32 k = 0; k < 5; k++) {
        if (name != NAMES[k] || (k == UNI_ENCODING && kind == UniKind::Translate))
            continue;
        i64 n = 0;
        if (k == UNI_START || k == UNI_END) {
            if (v.is_nil())
                return err_set("TypeError", "can't delete numeric/char attribute");
            if (!as_index(v, n))
                return not_index(v);
        }
        static_cast<ExcObj *>(e.obj())->uni[k] = v;
        return R::Ok;
    }
    return R::NotImpl;
}

// The hierarchy, base before derived so a forward reference is never needed.
// The order is the one CPython's own docs list it in.
const ExcType EXC_TABLE[] = {
    { "BaseException", nullptr },
    { "SystemExit", &EXC_TABLE[0] },
    { "KeyboardInterrupt", &EXC_TABLE[0] },
    { "GeneratorExit", &EXC_TABLE[0] },
    { "Exception", &EXC_TABLE[0] },

    { "StopIteration", &EXC_TABLE[4] },
    { "StopAsyncIteration", &EXC_TABLE[4] },
    { "ArithmeticError", &EXC_TABLE[4] },
    { "FloatingPointError", &EXC_TABLE[7] },
    { "OverflowError", &EXC_TABLE[7] },
    { "ZeroDivisionError", &EXC_TABLE[7] },
    { "AssertionError", &EXC_TABLE[4] },
    { "AttributeError", &EXC_TABLE[4] },
    { "EOFError", &EXC_TABLE[4] },
    { "ImportError", &EXC_TABLE[4] },
    { "LookupError", &EXC_TABLE[4] },
    { "IndexError", &EXC_TABLE[15] },
    { "KeyError", &EXC_TABLE[15] },
    { "MemoryError", &EXC_TABLE[4] },
    { "NameError", &EXC_TABLE[4] },
    { "UnboundLocalError", &EXC_TABLE[19] },
    { "OSError", &EXC_TABLE[4] },
    { "RuntimeError", &EXC_TABLE[4] },
    { "NotImplementedError", &EXC_TABLE[22] },
    { "RecursionError", &EXC_TABLE[22] },
    { "SyntaxError", &EXC_TABLE[4] },
    { "IndentationError", &EXC_TABLE[25] },
    { "TabError", &EXC_TABLE[26] },
    { "SystemError", &EXC_TABLE[4] },
    { "TypeError", &EXC_TABLE[4] },
    { "ValueError", &EXC_TABLE[4] },
    { "UnicodeError", &EXC_TABLE[30] },

    // Appended rather than slotted in: the bases above are by index, so a new
    // row in the middle would renumber every one after it.
    { "ModuleNotFoundError", &EXC_TABLE[14] },
    { "UnicodeEncodeError", &EXC_TABLE[31] },
    { "UnicodeDecodeError", &EXC_TABLE[31] },
    { "BufferError", &EXC_TABLE[4] },
    { "BaseExceptionGroup", &EXC_TABLE[0] },
    { "ExceptionGroup", &EXC_TABLE[36], &EXC_TABLE[4] },
    { "ImportCycleError", &EXC_TABLE[14] },
    { "UnicodeTranslateError", &EXC_TABLE[31] },
    { "ReferenceError", &EXC_TABLE[4] },
    { "Warning", &EXC_TABLE[4] },
    { "UserWarning", &EXC_TABLE[41] },
    { "DeprecationWarning", &EXC_TABLE[41] },
    { "PendingDeprecationWarning", &EXC_TABLE[41] },
    { "SyntaxWarning", &EXC_TABLE[41] },
    { "RuntimeWarning", &EXC_TABLE[41] },
    { "FutureWarning", &EXC_TABLE[41] },
    { "ImportWarning", &EXC_TABLE[41] },
    { "UnicodeWarning", &EXC_TABLE[41] },
    { "BytesWarning", &EXC_TABLE[41] },
    { "ResourceWarning", &EXC_TABLE[41] },
    { "EncodingWarning", &EXC_TABLE[41] },
    { "ConnectionError", &EXC_TABLE[21] },
    { "BrokenPipeError", &EXC_TABLE[53] },
    { "ConnectionAbortedError", &EXC_TABLE[53] },
    { "ConnectionRefusedError", &EXC_TABLE[53] },
    { "ConnectionResetError", &EXC_TABLE[53] },
    { "BlockingIOError", &EXC_TABLE[21] },
    { "ChildProcessError", &EXC_TABLE[21] },
    { "FileExistsError", &EXC_TABLE[21] },
    { "FileNotFoundError", &EXC_TABLE[21] },
    { "InterruptedError", &EXC_TABLE[21] },
    { "IsADirectoryError", &EXC_TABLE[21] },
    { "NotADirectoryError", &EXC_TABLE[21] },
    { "PermissionError", &EXC_TABLE[21] },
    { "ProcessLookupError", &EXC_TABLE[21] },
    { "TimeoutError", &EXC_TABLE[21] },
    { "PythonFinalizationError", &EXC_TABLE[22] },
    { "_IncompleteInputError", &EXC_TABLE[25] },
};

const usize EXC_COUNT = sizeof(EXC_TABLE) / sizeof(EXC_TABLE[0]);

constexpr Type exc_obj_type{ .name    = "Exception",
                             .trace   = exc_trace,
                             .repr    = exc_repr,
                             .str     = exc_str,
                             .getattr = exc_getattr };

const ExcType *exc_find(Str name)
{
    for (usize i = 0; i < EXC_COUNT; i++)
        if (EXC_TABLE[i].name == name)
            return &EXC_TABLE[i];
    return nullptr;
}

bool exc_is(const ExcType *t, const ExcType *base)
{
    for (; t; t = t->base) {
        if (t == base)
            return true;
        if (t->also && exc_is(t->also, base))
            return true;
    }
    return false;
}

Value exc_type_value(const ExcType *t)
{
    if (!t)
        return Value();
    if (!types) {
        types = heap_new<Types>();
        if (!types)
            return oom(), Value();
        gc_root_hook(types_mark);
        if (!types->made.resize(EXC_COUNT))
            return oom(), Value();
    }
    usize i = usize(t - EXC_TABLE);
    if (i >= EXC_COUNT)
        return err_set("SystemError", "an exception type outside the table"), Value();
    if (!types->made[i].is_nil())
        return types->made[i];

    // The base first, so the MRO is built over types that already have one.
    Root base{ t->base ? exc_type_value(t->base) : Value() };
    if (t->base && base.v.is_nil())
        return Value();
    if (t->also) {
        Root also{ exc_type_value(t->also) };
        TupleObj *two = also.v.is_nil() ? nullptr : tuple_new(2);
        if (!two)
            return also.v.is_nil() ? Value() : (oom(), Value());
        two->items()[0] = base.v;
        two->items()[1] = also.v;
        base            = obj_value(two);
    }
    Value o = type_make_native(t->name, base.v, &exc_obj_type, t);
    if (o.is_nil())
        return Value();
    types->made[i] = o;
    return o;
}

Value exc_inst(Value cls, Value args)
{
    Root rc{ cls }, ra{ args };
    if (ra.v.is_nil()) {
        TupleObj *e = tuple_new(0);
        if (!e)
            return oom(), Value();
        ra = obj_value(e);
    }
    ExcObj *o = static_cast<ExcObj *>(type_alloc_inst(rc.v, sizeof(ExcObj)));
    if (!o)
        return Value();
    o->flags |= OBJ_EXC;
    o->t       = type_obj(rc.v)->exc;
    o->args    = ra.v;
    o->cause   = Value();
    o->context = Value();
    o->msg     = Value();
    o->excs    = Value();
    for (Value &v : o->uni)
        v = Value();
    return obj_value(o);
}

Value exc_construct(Value cls, Value args)
{
    if (is_egroup_type(cls))
        return egroup_new(cls, args);
    Root rc{ cls }, ra{ args };
    // OSError(2, ...) is a FileNotFoundError: the number picks the class, for
    // OSError itself and nothing derived from it.
    const ExcType *os = &EXC_TABLE[21];
    if (!type_obj(rc.v)->heap && type_obj(rc.v)->exc == os) {
        TupleObj *t = static_cast<TupleObj *>(ra.v.obj());
        i64 code    = 0;
        if (t->len >= 2 && t->len <= 5 && as_index(t->items()[0], code) && oserror_for(code) != os) {
            rc = exc_type_value(oserror_for(code));
            if (rc.v.is_nil())
                return Value();
        }
    }
    Root made{ exc_inst(rc.v, ra.v) };
    // A class of the program's own is checked by the __init__ it reaches.
    if (!made.v.is_nil() && unierr_kind(made.v) != UniKind::None && !type_obj(rc.v)->heap &&
        !unierr_init(made.v, static_cast<ExcObj *>(made.v.obj())->args))
        return Value();
    if (!made.v.is_nil() && is_oserror(made.v) && !oserror_init(made.v))
        return Value();
    return made.v;
}

Value exc_new(const ExcType *t, Value args)
{
    Root ra{ args };
    Value cls = exc_type_value(t);
    return cls.is_nil() ? Value() : exc_inst(cls, ra.v);
}

void exc_slots(Type &s)
{
    s.trace   = exc_trace;
    s.repr    = exc_repr;
    s.str     = exc_str;
    s.getattr = exc_getattr;
}

Value exc_make(Str name, Str message)
{
    const ExcType *t = exc_find(name);
    if (!t)
        t = exc_find("Exception");
    if (message.empty())
        return exc_new(t, Value());

    Root s{ str_new(message) };
    if (s.v.is_nil())
        return Value();
    TupleObj *args = tuple_new(1);
    if (!args)
        return oom(), Value();
    args->items()[0] = s.v;
    return exc_new(t, obj_value(args));
}

Value exc_syntax(Str kind, Str message, Str file, u32 line, u32 col, Str text)
{
    const ExcType *t = exc_find(kind);
    if (!t || !exc_is(t, exc_find("SyntaxError")))
        return Value();
    Root msg{ str_new(message) };
    Root f{ file.empty() ? value_none() : str_lossy(file) };
    Root x{ text.empty() ? value_none() : str_lossy(text) };
    TupleObj *d = tuple_new(4);
    if (msg.v.is_nil() || f.v.is_nil() || x.v.is_nil() || !d)
        return d ? Value() : (oom(), Value());
    d->items()[0] = f.v;
    d->items()[1] = int_from_i64(line);
    d->items()[2] = int_from_i64(col);
    d->items()[3] = x.v;
    Root rd{ obj_value(d) };
    TupleObj *args = tuple_new(2);
    if (!args)
        return oom(), Value();
    args->items()[0] = msg.v;
    args->items()[1] = rd.v;
    return exc_new(t, obj_value(args));
}

R key_error(Value key)
{
    Root rk{ key };
    TupleObj *t = tuple_new(1);
    if (!t)
        return err_set("MemoryError", "out of memory");
    t->items()[0] = rk.v;
    Root args{ obj_value(t) };
    Value e = exc_new(exc_find("KeyError"), args.v);
    return e.is_nil() ? R::Err : err_set_value(e, "KeyError");
}

bool exc_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    Root base{ exc_type_value(&EXC_TABLE[0]) };
    Root fn{ native_new("__init__", b_exc_init) };
    StrObj *init = str_intern("__init__");
    if (base.v.is_nil() || !init || fn.v.is_nil())
        return false;
    if (dict_set(static_cast<DictObj *>(type_obj(base.v)->dict.obj()), obj_value(init), fn.v) !=
        R::Ok)
        return false;
    if (!egroup_install())
        return false;
    for (usize i = 0; i < EXC_COUNT; i++) {
        Value t = exc_type_value(&EXC_TABLE[i]);
        if (t.is_nil())
            return false;
        Root rt{ t };
        StrObj *name = str_intern(EXC_TABLE[i].name);
        if (!name)
            return oom() == R::Ok;
        if (dict_set(static_cast<DictObj *>(rd.v.obj()), obj_value(name), rt.v) != R::Ok)
            return false;
    }
    // The two old names for OSError.
    Root os{ exc_type_value(exc_find("OSError")) };
    constexpr Str ALIASES[] = { "EnvironmentError", "IOError" };
    for (Str alias : ALIASES) {
        StrObj *name = str_intern(alias);
        if (os.v.is_nil() || !name)
            return false;
        if (dict_set(static_cast<DictObj *>(rd.v.obj()), obj_value(name), os.v) != R::Ok)
            return false;
    }
    return true;
}

bool exc_line(Value e, String &out)
{
    if (!is_exc(e))
        return py_repr(e, out) == R::Ok;
    if (!out.append(type_name(e)))
        return false;
    String tail;
    if (syntax_details(e)) {
        if (py_str(args_of(e)->items()[0], tail) != R::Ok)
            return false;
    } else if (exc_str(e, tail) != R::Ok) {
        return false;
    }
    if (!tail.empty() && (!out.append(": ") || !out.append(tail.str())))
        return false;
    // Each note follows on a line of its own, as a traceback prints them.
    ExcObj *x  = static_cast<ExcObj *>(e.obj());
    StrObj *nn = str_intern("__notes__");
    Value notes;
    if (!nn || x->dict.is_nil() ||
        dict_get(static_cast<DictObj *>(x->dict.obj()), obj_value(nn), notes) != R::Ok)
        return err_clear(), true;
    if (!is_list(notes) && !is_tuple(notes))
        return true;
    usize n = is_list(notes) ? list_of(notes)->items.size() : args_len(notes);
    for (usize i = 0; i < n; i++) {
        Value one = is_list(notes) ? list_of(notes)->items[i] : args_item(notes, i);
        if (!out.push('\n'))
            return false;
        if (is_str(one)) {
            if (!out.append(str_of(one)->str()))
                return false;
        } else if (py_repr(one, out) != R::Ok) {
            return err_clear(), true;
        }
    }
    return true;
}

// The call a type answers, reached from the VM: ValueError('x'), and the same
// for a class deriving from one, whose __init__ the VM runs afterwards.
R exc_type_invoke(Value type, const CallArgs &a, Value &out)
{
    return exc_type_call(a, out, type);
}

// ------------------------------------------------------------------ OSError

bool is_oserror(Value v)
{
    return is_exc(v) && exc_is(exc_type_of(v), &EXC_TABLE[21]);
}

const ExcType *oserror_for(i64 code)
{
    struct Pick {
        i64 code;
        Str name;
    };
    constexpr Pick PICKS[] = {
        { 11, "BlockingIOError" },         { 114, "BlockingIOError" },
        { 115, "BlockingIOError" },        { 10, "ChildProcessError" },
        { 32, "BrokenPipeError" },         { 108, "BrokenPipeError" },
        { 103, "ConnectionAbortedError" }, { 111, "ConnectionRefusedError" },
        { 104, "ConnectionResetError" },   { 17, "FileExistsError" },
        { 2, "FileNotFoundError" },        { 21, "IsADirectoryError" },
        { 20, "NotADirectoryError" },      { 4, "InterruptedError" },
        { 13, "PermissionError" },         { 1, "PermissionError" },
        { 3, "ProcessLookupError" },       { 110, "TimeoutError" },
    };
    for (const Pick &p : PICKS)
        if (p.code == code)
            return exc_find(p.name);
    return &EXC_TABLE[21];
}

bool oserror_init(Value e)
{
    ExcObj *o = static_cast<ExcObj *>(e.obj());
    for (Value &f : o->uni)
        f = Value();
    TupleObj *a = args_of(e);
    if (a->len < 2 || a->len > 5)
        return true;
    Value f1 = a->len > 2 ? a->items()[2] : Value();
    Value f2 = a->len > 4 ? a->items()[4] : Value();
    o->uni[OS_ERRNO]    = a->items()[0];
    o->uni[OS_STRERROR] = a->items()[1];
    if (f1.is_nil() || is_none(f1))
        return true;
    // BlockingIOError's third argument is what was written, when it is a number.
    bool blocking = o->t == exc_find("BlockingIOError") && !type_obj(o->cls)->heap;
    i64 n         = 0;
    if (blocking && (as_index(f1, n) || is_float(f1))) {
        if (!as_index(f1, n))
            return err_set("TypeError", "an integer is required"), false;
        o->uni[OS_WRITTEN] = f1;
        return true;
    }
    o->uni[OS_FILENAME] = f1;
    if (!f2.is_nil() && !is_none(f2))
        o->uni[OS_FILENAME2] = f2;
    Root re{ e };
    TupleObj *two = tuple_new(2);
    if (!two)
        return oom(), false;
    TupleObj *had  = args_of(re.v);
    two->items()[0] = had->items()[0];
    two->items()[1] = had->items()[1];
    static_cast<ExcObj *>(re.v.obj())->args = obj_value(two);
    return true;
}

R oserror_store(Value e, Str name, Value v)
{
    constexpr Str FIELDS[] = { "errno", "strerror", "filename", "filename2" };
    ExcObj *o              = static_cast<ExcObj *>(e.obj());
    for (u32 k = 0; k < 4; k++)
        if (name == FIELDS[k]) {
            o->uni[k] = v.is_nil() || is_none(v) ? Value() : v;
            return R::Ok;
        }
    if (name != "characters_written" || o->t != exc_find("BlockingIOError"))
        return R::NotImpl;
    if (v.is_nil()) {
        if (o->uni[OS_WRITTEN].is_nil())
            return err_set("AttributeError", "characters_written");
        o->uni[OS_WRITTEN] = Value();
        return R::Ok;
    }
    i64 n = 0;
    if (!as_index(v, n))
        return not_index(v);
    o->uni[OS_WRITTEN] = v;
    return R::Ok;
}

i32 errno_of(Error e)
{
    switch (e) {
    case Error::Invalid:
        return 22;
    case Error::NoMemory:
        return 12;
    case Error::NotFound:
        return 2;
    case Error::Exists:
        return 17;
    case Error::NotDir:
        return 20;
    case Error::IsDir:
        return 21;
    case Error::Perm:
        return 13;
    case Error::Io:
        return 5;
    case Error::Cancelled:
        return 125;
    case Error::Again:
        return 11;
    case Error::Unsupported:
        return 95;
    case Error::Closed:
        return 32;
    case Error::NotEmpty:
        return 39;
    case Error::Loop:
        return 40;
    case Error::Intr:
        return 4;
    }
    return 5;
}

R err_errno(i32 code, Value f1, Value f2)
{
    Root r1{ f1 }, r2{ f2 };
    Str text = errno_text(code);
    Buf<32> unknown;
    if (text.empty()) {
        char tmp[24];
        unknown.put("Unknown error ").put(int_text(tmp, sizeof tmp, code));
        text = unknown.str();
    }
    Root msg{ str_new(text) };
    if (msg.v.is_nil())
        return R::Err;
    u32 n       = r2.v.is_nil() ? (r1.v.is_nil() ? 2 : 3) : 5;
    TupleObj *t = tuple_new(n);
    if (!t)
        return oom();
    Value *it = t->items();
    it[0]     = Value::of_int(code);
    it[1]     = msg.v;
    if (n > 2)
        it[2] = r1.v;
    if (n > 3) {
        it[3] = value_none();
        it[4] = r2.v;
    }
    Root rt{ obj_value(t) };
    Root cls{ exc_type_value(oserror_for(code)) };
    if (cls.v.is_nil())
        return R::Err;
    Value e = exc_construct(cls.v, rt.v);
    return e.is_nil() ? R::Err : err_set_value(e);
}

R err_os(Error e, Value f1, Value f2)
{
    return err_errno(errno_of(e), f1, f2);
}
