// The exception hierarchy, and the two objects it needs to be Python-visible:
// the type that `except` matches against and the instance that carries the
// arguments.
//
// A type is a static descriptor with a base pointer, not an object with a
// dict: classes are phase 9, and until there is a metatype there is nothing a
// heap type would buy. Matching walks the base chain, so it is a few pointer
// compares and no allocation.
#pragma once

#include "kernel/result.h"
#include "obj.h"

struct ExcType {
    Str name;
    const ExcType *base;           // null only for BaseException
    const ExcType *also = nullptr; // a second base: ExceptionGroup is an Exception
};

// The whole hierarchy, in one table so a name lookup is a linear scan over it
// and nothing can be declared without being findable.
extern const ExcType EXC_TABLE[];
extern const usize EXC_COUNT;

// By name, or null. This is how the error channel's kind becomes an object.
const ExcType *exc_find(Str name);

// Is `t` a `base`, or something under it?
bool exc_is(const ExcType *t, const ExcType *base);

extern const Type exc_obj_type; // the handlers an exception instance uses

// An instance. The first three fields are InstObj's, in InstObj's order, so a
// user class deriving from an exception is an instance in every other respect:
// it has a class, an attribute dict and the whole descriptor protocol.
struct ExcObj : Obj {
    Value cls;    // TypeObj
    Value dict;   // instance attributes
    Value unused; // InstObj's native slot; an exception delegates to nothing
    const ExcType *t;
    Value args;    // TupleObj, always
    Value cause;   // `raise X from Y`, or Nil
    Value context; // what was being handled when this was raised, or Nil
    Value msg;     // a group's message, or Nil
    Value excs;    // a group's members, a tuple, or Nil
    Value uni[5];  // a UnicodeError's encoding, object, start, end and reason
    Value tb;      // __traceback__, or Nil
    bool suppress; // __suppress_context__
};

// ---------------------------------------------------------------- traceback

struct TracebackObj : Obj {
    Value next;  // the frame further in, or Nil
    Value frame; // FrameObj
    i32 lasti;
    i32 lineno;
};

extern const Type traceback_type;

inline bool is_traceback(Value v)
{
    return v.is_obj() && v.obj()->type == &traceback_type;
}

inline TracebackObj *tb_of(Value v)
{
    return static_cast<TracebackObj *>(v.obj());
}

// Nil with the error pending.
Value tb_new(Value next, Value frame, i32 lasti, i32 lineno);

struct CallArgs;
// types.TracebackType(tb_next, tb_frame, tb_lasti, tb_lineno).
R traceback_ctor(const CallArgs &a, Value &out);

// BaseException.with_traceback.
R exc_with_traceback(const CallArgs &a, Value &out);

// A user class deriving from one of these carries its own type, so the flag
// rather than the descriptor is what says an object is an exception.
inline bool is_exc(Value v)
{
    return v.is_obj() && (v.obj()->flags & OBJ_EXC) != 0;
}

inline const ExcType *exc_type_of(Value v)
{
    return is_exc(v) ? static_cast<ExcObj *>(v.obj())->t : nullptr;
}

// The type object for a row of the table, made on first use and kept:
// `ValueError` names the same object every time it is looked up.
Value exc_type_value(const ExcType *t);

// A fresh instance of `cls`, which is a class deriving from an exception.
Value exc_inst(Value cls, Value args);

// What calling `cls` with `args` makes: exc_inst, or a checked group.
Value exc_construct(Value cls, Value args);

// An instance. `args` may be Nil for none.
Value exc_new(const ExcType *t, Value args);

// The shorthand every raise from C++ wants: one string argument, or none.
Value exc_make(Str name, Str message);

// A SyntaxError, or a kind under it, with its details:
// (msg, (filename, lineno, offset, text)). An empty file or text is None.
Value exc_syntax(Str kind, Str message, Str file, u32 line, u32 col, Str text);

// KeyError(key), pending: the key itself is the argument, as CPython's is.
R key_error(Value key);

// Put every name in the table into a namespace.
bool exc_install(DictObj *into);

// "ValueError: bad thing", the last line of a traceback.
bool exc_line(Value e, String &out);

struct CallArgs;

// Calling a type makes an instance: ValueError('x'). The VM dispatches here.
R exc_type_invoke(Value type, const CallArgs &a, Value &out);

// The handlers a class deriving from an exception takes for its own slots.
void exc_slots(Type &s);

// ------------------------------------------------------------ UnicodeError

enum class UniKind : u8 { None, Encode, Decode, Translate };
enum : u8 { UNI_ENCODING, UNI_OBJECT, UNI_START, UNI_END, UNI_REASON };

// Which of the three a value is an instance of, if any.
UniKind unierr_kind(Value v);

// The five fields from a constructor's arguments, checked as CPython's
// __init__ checks them. False with a TypeError pending.
bool unierr_init(Value e, Value args);

// The field `name` of a UnicodeError, stored; Nil `v` deletes. NotImpl when
// `name` is not one of the five.
R unierr_store(Value e, Str name, Value v);

// -------------------------------------------------------------- ImportError

// ImportError's msg, name, path and name_from live in `uni` too.
bool is_importerr(Value v);
struct CallArgs;
R importerr_init(Value e, const CallArgs &a, u32 from);
R importerr_store(Value e, Str name, Value v);

// An ImportError of class `kind` with `msg`, and `name` and `path` unless Nil.
// Always returns R::Err.
R exc_raise_import(Str kind, Value msg, Value name, Value path);

// ------------------------------------------------------------------ OSError

// OSError's errno, strerror, filename, filename2 and BlockingIOError's
// characters_written, kept where a UnicodeError keeps its five: no class is
// both.
enum : u8 { OS_ERRNO, OS_STRERROR, OS_FILENAME, OS_FILENAME2, OS_WRITTEN };

bool is_oserror(Value v);

// The subclass OSError(code, ...) makes, or OSError itself.
const ExcType *oserror_for(i64 code);

// The fields from a constructor's arguments, as CPython's oserror_init reads
// them; `args` may be cut to (errno, strerror). False with an error pending.
bool oserror_init(Value e);

// A field, stored; Nil `v` deletes. NotImpl when `name` is not one.
R oserror_store(Value e, Str name, Value v);

// strerror(code) in glibc's words, empty for a number it does not know.
// miscmod.cpp, beside the errno table.
Str errno_text(i64 code);

// The errno a Braam error stands for.
i32 errno_of(Error e);

// OSError(code, strerror(code), f1, None, f2) pending, as the subclass the
// number picks; a Nil filename is left out. Always R::Err.
R err_errno(i32 code, Value f1 = Value(), Value f2 = Value());

// The same for a failed system call.
R err_os(Error e, Value f1 = Value(), Value f2 = Value());
