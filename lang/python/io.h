// `_io`: CPython's io layers, written natively.
//
// io.py and _pyio.py both import `_io`, sys.stdout has to exist before any
// library can be found, and print is the hottest path there is, so the layers
// are C++ -- as CPython's own are -- rather than _pyio.py over a native floor.
// _pyio.py is still copied, and runs over `posix`, which is how its half of
// CPython's io tests is reached.
//
// Every read and write is a system call, and so a continuation. Each layer
// answers from its own buffer without one where it can: a line already
// decoded, bytes already read, a write that fits. Only a refill or a flush
// parks, and a layer reaches the one below it by calling its method, so a
// program's own raw stream under a native BufferedReader is the same path as
// FileIO's.
#pragma once

#include "posix.h"

enum IoKind : u8 {
    IO_BASE,
    IO_RAWBASE,
    IO_BUFBASE,
    IO_TEXTBASE,
    IO_FILE,
    IO_BYTES,
    IO_BUFFERED,
    IO_PAIR,
    IO_TEXT,
    IO_STRING,
    IO_NLDECODER,
};

// What every object of the family starts with.
struct IoObj : Obj {
    Value dict;   // attributes stored on it: open() sets TextIOWrapper.mode
    u8 kind;      // IoKind
    bool closed;  // _IOBase's own flag, which the abstract layers answer
    bool inside;  // laid inside an instance: that instance is what finalizes
    bool finalizing;
};

extern const Type iobase_type;
extern const Type rawbase_type;
extern const Type bufbase_type;
extern const Type textbase_type;
extern const Type fileio_type;
extern const Type bytesio_type;
extern const Type bufreader_type;
extern const Type bufwriter_type;
extern const Type bufrandom_type;
extern const Type bufpair_type;
extern const Type textio_type;
extern const Type stringio_type;
extern const Type nldecoder_type;

// Is this an object of the family, built-in or inside an instance? `io` takes
// the native part.
bool io_of(Value v, IoObj *&io);

inline IoObj *io_cast(Value v)
{
    return static_cast<IoObj *>(v.obj());
}

// The object a native method acts on, checked against `kind` (or any of the
// family when `kind` is IO_BASE). Null with TypeError pending. Marks a native
// laid inside an instance, so the instance alone finalizes.
IoObj *io_self(const CallArgs &a, u8 kind, Str who);

// Is `v` exactly a built-in of this kind, not a subclass instance? Only then
// may a layer above call it directly rather than through its methods.
bool io_is_plain(Value v, u8 kind);

// The shared trace and fini: the dict.
void io_trace(Obj *o);

// getattr and setattr for the dict, after the type's own names.
R io_dict_get(Value v, StrObj *name, Value &out);
R io_dict_set(Value v, StrObj *name, Value x);

// setattr for a kind whose names in `readonly` may not be set, and whose
// _finalizing takes a bool; everything else goes to the dict.
R io_set_attr(Value v, StrObj *name, Value x, const Str *readonly, usize n, Str tname);

template <usize N>
inline R io_set_attr(Value v, StrObj *name, Value x, const Str (&readonly)[N], Str tname)
{
    return io_set_attr(v, name, x, readonly, N, tname);
}

// UnsupportedOperation(msg), pending. Always R::Err.
R io_unsupported(Str msg);

// ValueError("I/O operation on closed file."), or `msg`. Always R::Err.
R io_closed_err(Str msg = Str());

// The class UnsupportedOperation, made once.
Value io_unsupported_type();

// Register a native that may hold unflushed data or an open descriptor, so
// the program's end can close it; fini takes it off again.
void io_track(IoObj *o);
void io_untrack(Obj *o);

// The native finalizer every kind shares: its close, or Nil.
Value io_del(Value v);

// ------------------------------------------------------------ the kinds

// iobase.cpp: the abstract layers, the module, open().
bool io_install(DictObj *into);
R io_open(const CallArgs &a, Value &out);

// iofile.cpp
bool fileio_methods();
R fileio_new(const CallArgs &a, Value &out);
// A FileIO over one of the three descriptors the process was given.
Value fileio_std(i32 fd, bool writable);
// Its mode as CPython spells it: 'rb', 'wb+' and so on.
Str file_mode(Value v);
// Bytes into a plain FileIO: the count, or a ContObj.
R file_write(Value self, Str data, Value &out);
// A plain FileIO over 1 or 2, whose writes need no call.
bool file_is_sink(Value v);

// What a plain FileIO knows of itself without a call. `seekable` is -1 when
// finding out takes one.
struct FileFacts {
    bool closed, readable, writable;
    i8 seekable;
};
FileFacts file_facts(Value v);

// iobuf.cpp
bool buffered_methods();
R bufreader_new(const CallArgs &a, Value &out);
R bufwriter_new(const CallArgs &a, Value &out);
R bufrandom_new(const CallArgs &a, Value &out);
R bufpair_new(const CallArgs &a, Value &out);
Value buffered_std(Value raw, bool writable);
// Bytes into a plain BufferedWriter: Ok when they fit its buffer and nothing
// had to be called, NotImpl when the caller has to use write() instead.
R buffered_put(Value w, Str data);

// open()'s file argument, when it is a descriptor.
bool is_fd_value(Value v);

// iotext.cpp
bool text_methods();
// One of the three the process started with, which the end flushes and
// leaves open.
bool textio_is_std(Value v);
R textio_new(const CallArgs &a, Value &out);
R nldecoder_new(const CallArgs &a, Value &out);
Value textio_std(Value buffer, i32 fd);
void textio_set_line_buffering(Value v, bool on);
// print's way in: `text` written to `file`, a plain TextIOWrapper, with
// `out` a ContObj when that needs a call. NotImpl: not a plain one.
R textio_print(Value file, Str text, Value &out);

// A newline decoder made to answer `newlines` for what `seen` says.
void nldecoder_set_seen(Value v, u8 seen);

// iomem.cpp
bool mem_methods();
R bytesio_new(const CallArgs &a, Value &out);
R stringio_new(const CallArgs &a, Value &out);

// ------------------------------------------------------------ the program's end

// Is there something open the program's end has to close?
bool io_exit_pending();

// A ContObj that flushes the three standard streams and closes everything
// else still open, reporting what fails. Nil when there is nothing to do.
Value io_exit_runner();

// The writable span of a buffer a readinto may fill: a bytearray, an array,
// or a memoryview over one. False with TypeError pending.
bool io_writable_span(Value v, u8 *&p, usize &n);
