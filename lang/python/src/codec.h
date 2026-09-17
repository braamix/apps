// The codecs: text to octets and back, and what to do where the two do not
// meet.
//
// Every codec _codecs names is written here, over one engine. A codec runs
// until it is done or until it meets something it cannot encode or decode;
// the built-in error handlers settle that on the spot, and one the program
// registered is a call, so the run parks in a continuation and the VM makes
// it. That is ground rule 2, and it is why a codec is an object rather than a
// loop: its whole state has to outlive the call.
#pragma once

#include "call.h"

enum class Codec : u8 {
    None,
    Utf8,
    Utf7,
    Utf16,
    Utf16Le,
    Utf16Be,
    Utf32,
    Utf32Le,
    Utf32Be,
    Latin1,
    Ascii,
    Charmap,
    UnicodeEscape,
    RawUnicodeEscape,
};

// The handlers written here. Other is any name the registry has to answer.
enum class ErrH : u8 {
    Strict,
    Ignore,
    Replace,
    BackslashReplace,
    XmlCharRefReplace,
    NameReplace,
    SurrogateEscape,
    SurrogatePass,
    Other,
};

// Nil and None are strict.
ErrH errh_of(Value errors);

// What a finished run is handed back as: the text alone, (text, consumed), or
// (text, consumed, byteorder).
enum : u8 { WRAP_NONE, WRAP_CONSUMED, WRAP_BYTEORDER };

struct CodecCall {
    Codec codec   = Codec::None;
    bool encode   = false;
    bool final    = true;
    i32 byteorder = 0; // UTF-16/32: -1 little, 1 big, 0 by BOM
    u8 wrap       = WRAP_NONE;
    Value input;   // a str to encode, bytes to decode
    Value errors;  // a str, or Nil for strict
    Value mapping; // charmap: a str, a dict or an EncodingMap; Nil is latin-1
};

// The result, or a ContObj when a handler of the program's own has to run.
R codec_run(const CodecCall &c, Value &out);

// CPython's _Py_normalize_encoding: runs of anything but letters, digits and
// dots become one '_', and the ends lose theirs. False when it would not fit.
bool enc_normalize(Str name, bool lower, char *out, usize cap, usize &len);

// The encodings str.encode and bytes.decode answer without the registry:
// utf-8, utf-16, utf-32, ascii and latin-1, however they are spelled.
Codec codec_shortcut(Str encoding);

// The handler registered under `name`, or Nil with a LookupError pending.
Value codec_lookup_error(Str name);

// charmap_build's result: an EncodingMap, or a dict where one will not do.
Value charmap_build(Value table);
extern const Type encmap_type;

// The eight handlers, callable from Python, for the registry.
R codec_handler_call(ErrH h, Value exc, Value &out);

// bytes to bytes, as escape_decode and escape_encode do it.
R escape_decode(Str data, Value errors, Value &out);
R escape_encode(Str data, Value &out);

// ------------------------------------------------------------------ callers

// str.encode(encoding, errors), with Nil for what the call left out. `out` may
// be a ContObj, when the registry or a handler of the program's own has to run.
R text_encode(Value s, Value encoding, Value errors, Value &out);

// bytes.decode and str(obj, encoding, errors): the same, the other way.
R text_decode(Value obj, Value encoding, Value errors, Value &out);

// What reaches a descriptor: UTF-8, with surrogates handled the way the
// stream says -- surrogateescape for stdout and stdin, backslashreplace for
// stderr. False leaves a UnicodeEncodeError pending.
bool std_encode(Str text, bool escape, String &out);

// Text from outside that has to become a str whatever it holds: UTF-8, with
// U+FFFD for what is not. Nil only when out of memory.
Value str_lossy(Str bytes);

bool codecs_install(DictObj *into);
