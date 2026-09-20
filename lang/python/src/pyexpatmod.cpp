// `pyexpat`: the XML parser under xml.etree, xml.dom and xml.sax, over the
// expat in src/expat/.
//
// Expat calls a handler from deep inside its scanners, and a native here may
// not call Python at all -- ground rule 2. So this is a driver, in the sense
// emulators/simbesm's README gives the word: the parser runs plain C++ until
// it has an event for its caller, and the caller -- a ContObj the VM steps --
// makes the Python call and then resumes it. Expat already had the shape,
// in XML_StopParser(parser, XML_TRUE) and XML_ResumeParser; owning the code
// is what turns it from an escape hatch into the ordinary path.
//
// One expat token can fire several handlers before the parse loop looks at
// the suspension -- `<e/>` fires start and end, an end tag fires one
// EndNamespaceDecl per binding -- so a handler queues its event rather than
// being the event, and the step drains the queue before resuming.
//
// Three handlers steer the parse by what they return, and their answer
// arrives a suspension too late for expat's call site to act on. That is what
// XmlFailSuspendedParse is for: the parse is failed afterwards, with the
// error and the position the refusal would have produced.
#include "bigint.h"
#include "builtin.h"
#include "call.h"
#include "exc.h"
#include "expat/expat.h"
#include "expat/expat_config.h"
#include "func.h"
#include "gc.h"
#include "intern.h"
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

// A TypeError naming the type in the middle of the message, which err_set2
// cannot do: it puts the detail after a colon and CPython's wording here
// does not.
R type_error(Str before, Value v, Str after)
{
    Buf<160> b;
    b.put(before).put(type_name(v)).put(after);
    return err_set("TypeError", b.str());
}

// ------------------------------------------------------------- the handlers

// The order is CPython's handler_info table, because a parser's handlers are
// an array and `pyexpat.errors` is not the only thing that counts on it.
enum Handler : u8 {
    H_StartElement,
    H_EndElement,
    H_ProcessingInstruction,
    H_CharacterData,
    H_UnparsedEntityDecl,
    H_NotationDecl,
    H_StartNamespaceDecl,
    H_EndNamespaceDecl,
    H_Comment,
    H_StartCdataSection,
    H_EndCdataSection,
    H_Default,
    H_DefaultHandlerExpand,
    H_NotStandalone,
    H_ExternalEntityRef,
    H_StartDoctypeDecl,
    H_EndDoctypeDecl,
    H_EntityDecl,
    H_XmlDecl,
    H_ElementDecl,
    H_AttlistDecl,
    H_SkippedEntity,
    H_COUNT,
};

constexpr Str HANDLER_NAMES[H_COUNT] = {
    "StartElementHandler",
    "EndElementHandler",
    "ProcessingInstructionHandler",
    "CharacterDataHandler",
    "UnparsedEntityDeclHandler",
    "NotationDeclHandler",
    "StartNamespaceDeclHandler",
    "EndNamespaceDeclHandler",
    "CommentHandler",
    "StartCdataSectionHandler",
    "EndCdataSectionHandler",
    "DefaultHandler",
    "DefaultHandlerExpand",
    "NotStandaloneHandler",
    "ExternalEntityRefHandler",
    "StartDoctypeDeclHandler",
    "EndDoctypeDeclHandler",
    "EntityDeclHandler",
    "XmlDeclHandler",
    "ElementDeclHandler",
    "AttlistDeclHandler",
    "SkippedEntityHandler",
};

// --------------------------------------------------------------- the object

struct ParserObj : Obj {
    Value handlers[H_COUNT];
    Value intern; // the dict ParserCreate was given, or Nil
    Value parent; // the parser this one was made from: it owns the DTD
    Value evq;    // ListObj: handler index, argument tuple, index, tuple, ...
    Value hold;   // the buffer being parsed, kept alive across the events
    Value readfn; // ParseFile's bound read, kept alive the same way
    XML_Parser p; // null once the object has been finalized
    String buf;   // character data, while buffer_text is on
    usize buffer_size;
    u32 evi;     // how much of evq the step has dispatched
    i32 pending; // the handler whose answer the step is waiting for, or -1
    bool buffer_text;
    bool ordered_attributes;
    bool specified_attributes;
    bool ns_prefixes;
    bool failed;  // an event could not be built: stop, quietly
    bool ns;      // made by ParserCreate with a namespace separator
    bool reparse; // expat has a setter for this and no getter, so it is here
                  // -- upstream's default is on
};

extern const Type parser_type;

ParserObj *parser_of(Value v)
{
    return static_cast<ParserObj *>(v.obj());
}

bool is_parser(Value v)
{
    return v.is_obj() && v.obj()->type == &parser_type;
}

// ----------------------------------------------------------------- the home

struct Home {
    Value error;  // the class pyexpat.ExpatError
    Value errors; // the submodule
    Value model;  // the submodule
};

Home *home;

void home_mark()
{
    if (!home)
        return;
    gc_mark(home->error);
    gc_mark(home->errors);
    gc_mark(home->model);
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

// ExpatError(text), with .code, .lineno and .offset, pending.
R expat_error(Str text, i64 code, i64 lineno, i64 offset)
{
    Home *h = here();
    if (!h || h->error.is_nil())
        return err_set("Exception", text);
    Root m{ str_new(text) };
    if (m.v.is_nil())
        return R::Err;
    TupleObj *t = tuple_new(1);
    if (!t)
        return oom();
    t->items()[0] = m.v;
    Root rt{ obj_value(t) };
    Root e{ exc_construct(h->error, rt.v) };
    if (e.v.is_nil())
        return R::Err;
    struct {
        Str name;
        i64 value;
    } attrs[] = { { "code", code }, { "lineno", lineno }, { "offset", offset } };
    for (auto &at : attrs) {
        StrObj *k = str_intern(at.name);
        Root n{ int_from_i64(at.value) };
        if (!k || n.v.is_nil())
            return R::Err;
        if (inst_setattr(e.v, k, n.v) != R::Ok)
            return R::Err;
    }
    return err_set_value(e.v);
}

// The exception a failed XML_Parse raises: expat's message, then the place,
// worded as CPython words it.
R parse_error(ParserObj *self, enum XML_Error code)
{
    XML_Size line = XML_GetCurrentLineNumber(self->p);
    XML_Size col  = XML_GetCurrentColumnNumber(self->p);
    Buf<160> b;
    b.put(Str(XML_ErrorString(code), expat_strlen(XML_ErrorString(code))));
    b.put(": line ");
    b.put(u64(line));
    b.put(", column ");
    b.put(u64(col));
    return expat_error(b.str(), code, i64(line), i64(col));
}

// ------------------------------------------------------- building the event

// expat takes C strings and a str here is not NUL-terminated, so it is
// copied with the terminator every entry point below expects.
bool as_cstr(Value v, String &out)
{
    return out.assign(str_of(v)->str()) && out.push('\0');
}

// A str from expat's UTF-8, or None where expat had nothing to say.
Value conv(const XML_Char *s)
{
    return s ? str_new(Str(s, expat_strlen(s))) : value_none();
}

Value conv_n(const XML_Char *s, int len)
{
    return str_new(Str(s, usize(len)));
}

// The same, through the parser's intern table where it has one: a name that
// occurs a thousand times is then one object, which is why ParserCreate takes
// the dict at all.
Value conv_intern(ParserObj *self, const XML_Char *s)
{
    Root v{ conv(s) };
    if (v.v.is_nil() || self->intern.is_nil() || !is_str(v.v))
        return v.v;
    DictObj *d = static_cast<DictObj *>(self->intern.obj());
    Value got;
    R r = dict_get(d, v.v, got);
    if (r == R::Ok)
        return got;
    if (r == R::Err)
        return Value();
    if (dict_set(d, v.v, v.v) != R::Ok)
        return Value();
    return v.v;
}

// An event could not be built. Stop calling Python: the pending error is what
// the parse answers with, and expat is told to finish where it stands.
void fail(ParserObj *self)
{
    self->failed = true;
    XML_StopParser(self->p, XML_FALSE);
}

// Is there anything to do for `h`? Nothing is built where no handler is set,
// which is also what keeps a parse with no handlers at full speed.
bool wants(ParserObj *self, u32 h)
{
    return !self->failed && !self->handlers[h].is_nil();
}

bool push_event(ParserObj *self, u32 h, Value args)
{
    ListObj *q = static_cast<ListObj *>(self->evq.obj());
    Root a{ args };
    Value n = int_from_i64(i64(h));
    if (n.is_nil() || !list_push(q, n))
        return false;
    q = static_cast<ListObj *>(self->evq.obj());
    return list_push(q, a.v);
}

// The buffered character data, as its own event. CPython's
// flush_character_buffer, one suspension earlier.
bool flush_text(ParserObj *self)
{
    if (self->buf.empty())
        return true;
    Root s{ str_new(self->buf.str()) };
    self->buf.clear();
    if (s.v.is_nil())
        return false;
    TupleObj *t = tuple_new(1);
    if (!t)
        return false;
    t->items()[0] = s.v;
    return push_event(self, H_CharacterData, obj_value(t));
}

// Queue `n` arguments for `h` and stop the parse. The buffered text goes
// first, because that is the order the handlers were called in.
void queue(ParserObj *self, u32 h, const Value *vals, u32 n)
{
    if (!flush_text(self)) {
        fail(self);
        return;
    }
    TupleObj *t = tuple_new(n);
    if (!t) {
        oom();
        fail(self);
        return;
    }
    for (u32 i = 0; i < n; i++)
        t->items()[i] = vals[i];
    if (!push_event(self, h, obj_value(t))) {
        if (!err_pending())
            oom();
        fail(self);
        return;
    }
    XML_StopParser(self->p, XML_TRUE);
}

// ------------------------------------------------------------ the handlers

ParserObj *self_of(void *userData)
{
    return static_cast<ParserObj *>(userData);
}

void h_start_element(void *ud, const XML_Char *name, const XML_Char **atts)
{
    ParserObj *self = self_of(ud);
    if (!wants(self, H_StartElement))
        return;
    int max = 0;
    if (self->specified_attributes) {
        max = XML_GetSpecifiedAttributeCount(self->p);
    } else {
        while (atts[max])
            max += 2;
    }
    Value a[2];
    Roots pin{ a, 2 };
    if (self->ordered_attributes) {
        ListObj *l = list_new();
        if (!l) {
            oom();
            fail(self);
            return;
        }
        a[1] = obj_value(l);
        for (int i = 0; i < max; i++) {
            Root v{ (i & 1) ? conv(atts[i]) : conv_intern(self, atts[i]) };
            if (v.v.is_nil() || !list_push(static_cast<ListObj *>(a[1].obj()), v.v)) {
                if (!err_pending())
                    oom();
                fail(self);
                return;
            }
        }
    } else {
        DictObj *d = dict_new();
        if (!d) {
            oom();
            fail(self);
            return;
        }
        a[1] = obj_value(d);
        for (int i = 0; i < max; i += 2) {
            Root k{ conv_intern(self, atts[i]) };
            Root v{ k.v.is_nil() ? Value() : conv(atts[i + 1]) };
            if (v.v.is_nil() || dict_set(static_cast<DictObj *>(a[1].obj()), k.v, v.v) != R::Ok) {
                if (!err_pending())
                    oom();
                fail(self);
                return;
            }
        }
    }
    a[0] = conv_intern(self, name);
    if (a[0].is_nil()) {
        fail(self);
        return;
    }
    queue(self, H_StartElement, a, 2);
}

void h_end_element(void *ud, const XML_Char *name)
{
    ParserObj *self = self_of(ud);
    if (!wants(self, H_EndElement))
        return;
    Value a[1];
    Roots pin{ a, 1 };
    a[0] = conv_intern(self, name);
    if (a[0].is_nil()) {
        fail(self);
        return;
    }
    queue(self, H_EndElement, a, 1);
}

// The one handler that does not always suspend: with buffer_text on it
// gathers, and the text becomes an event at the next handler or at the end
// of the buffer being parsed.
void h_character_data(void *ud, const XML_Char *data, int len)
{
    ParserObj *self = self_of(ud);
    if (!wants(self, H_CharacterData))
        return;
    usize n = usize(len);
    if (!self->buffer_text) {
        Value a[1];
        Roots pin{ a, 1 };
        a[0] = conv_n(data, len);
        if (a[0].is_nil()) {
            fail(self);
            return;
        }
        queue(self, H_CharacterData, a, 1);
        return;
    }
    if (n > self->buffer_size - self->buf.size()) {
        if (!flush_text(self)) {
            fail(self);
            return;
        }
    }
    if (!self->buf.append(Str(data, n))) {
        oom();
        fail(self);
        return;
    }
    // A run longer than the buffer is its own event rather than a growing
    // buffer, which is what CPython does with a fixed-size one.
    if (self->buf.size() >= self->buffer_size) {
        if (!flush_text(self)) {
            fail(self);
            return;
        }
        XML_StopParser(self->p, XML_TRUE);
    }
}

void h_processing_instruction(void *ud, const XML_Char *target, const XML_Char *data)
{
    ParserObj *self = self_of(ud);
    if (!wants(self, H_ProcessingInstruction))
        return;
    Value a[2];
    Roots pin{ a, 2 };
    a[0] = conv_intern(self, target);
    a[1] = a[0].is_nil() ? Value() : conv(data);
    if (a[1].is_nil()) {
        fail(self);
        return;
    }
    queue(self, H_ProcessingInstruction, a, 2);
}

void h_unparsed_entity_decl(void *ud, const XML_Char *entityName, const XML_Char *base,
                            const XML_Char *systemId, const XML_Char *publicId,
                            const XML_Char *notationName)
{
    ParserObj *self = self_of(ud);
    if (!wants(self, H_UnparsedEntityDecl))
        return;
    const XML_Char *src[5] = { entityName, base, systemId, publicId, notationName };
    Value a[5];
    Roots pin{ a, 5 };
    for (u32 i = 0; i < 5; i++) {
        a[i] = conv_intern(self, src[i]);
        if (a[i].is_nil()) {
            fail(self);
            return;
        }
    }
    queue(self, H_UnparsedEntityDecl, a, 5);
}

void h_notation_decl(void *ud, const XML_Char *notationName, const XML_Char *base,
                     const XML_Char *systemId, const XML_Char *publicId)
{
    ParserObj *self = self_of(ud);
    if (!wants(self, H_NotationDecl))
        return;
    const XML_Char *src[4] = { notationName, base, systemId, publicId };
    Value a[4];
    Roots pin{ a, 4 };
    for (u32 i = 0; i < 4; i++) {
        a[i] = conv_intern(self, src[i]);
        if (a[i].is_nil()) {
            fail(self);
            return;
        }
    }
    queue(self, H_NotationDecl, a, 4);
}

void h_start_namespace_decl(void *ud, const XML_Char *prefix, const XML_Char *uri)
{
    ParserObj *self = self_of(ud);
    if (!wants(self, H_StartNamespaceDecl))
        return;
    Value a[2];
    Roots pin{ a, 2 };
    a[0] = conv_intern(self, prefix);
    a[1] = a[0].is_nil() ? Value() : conv_intern(self, uri);
    if (a[1].is_nil()) {
        fail(self);
        return;
    }
    queue(self, H_StartNamespaceDecl, a, 2);
}

void h_end_namespace_decl(void *ud, const XML_Char *prefix)
{
    ParserObj *self = self_of(ud);
    if (!wants(self, H_EndNamespaceDecl))
        return;
    Value a[1];
    Roots pin{ a, 1 };
    a[0] = conv_intern(self, prefix);
    if (a[0].is_nil()) {
        fail(self);
        return;
    }
    queue(self, H_EndNamespaceDecl, a, 1);
}

void h_comment(void *ud, const XML_Char *data)
{
    ParserObj *self = self_of(ud);
    if (!wants(self, H_Comment))
        return;
    Value a[1];
    Roots pin{ a, 1 };
    a[0] = conv(data);
    if (a[0].is_nil()) {
        fail(self);
        return;
    }
    queue(self, H_Comment, a, 1);
}

void h_start_cdata(void *ud)
{
    ParserObj *self = self_of(ud);
    if (wants(self, H_StartCdataSection))
        queue(self, H_StartCdataSection, nullptr, 0);
}

void h_end_cdata(void *ud)
{
    ParserObj *self = self_of(ud);
    if (wants(self, H_EndCdataSection))
        queue(self, H_EndCdataSection, nullptr, 0);
}

void h_default(void *ud, const XML_Char *s, int len)
{
    ParserObj *self = self_of(ud);
    if (!wants(self, H_Default))
        return;
    Value a[1];
    Roots pin{ a, 1 };
    a[0] = conv_n(s, len);
    if (a[0].is_nil()) {
        fail(self);
        return;
    }
    queue(self, H_Default, a, 1);
}

void h_default_expand(void *ud, const XML_Char *s, int len)
{
    ParserObj *self = self_of(ud);
    if (!wants(self, H_DefaultHandlerExpand))
        return;
    Value a[1];
    Roots pin{ a, 1 };
    a[0] = conv_n(s, len);
    if (a[0].is_nil()) {
        fail(self);
        return;
    }
    queue(self, H_DefaultHandlerExpand, a, 1);
}

void h_start_doctype_decl(void *ud, const XML_Char *name, const XML_Char *sysid,
                          const XML_Char *pubid, int has_internal_subset)
{
    ParserObj *self = self_of(ud);
    if (!wants(self, H_StartDoctypeDecl))
        return;
    const XML_Char *src[3] = { name, sysid, pubid };
    Value a[4];
    Roots pin{ a, 4 };
    for (u32 i = 0; i < 3; i++) {
        a[i] = conv_intern(self, src[i]);
        if (a[i].is_nil()) {
            fail(self);
            return;
        }
    }
    a[3] = int_from_i64(has_internal_subset);
    if (a[3].is_nil()) {
        fail(self);
        return;
    }
    queue(self, H_StartDoctypeDecl, a, 4);
}

void h_end_doctype_decl(void *ud)
{
    ParserObj *self = self_of(ud);
    if (wants(self, H_EndDoctypeDecl))
        queue(self, H_EndDoctypeDecl, nullptr, 0);
}

void h_entity_decl(void *ud, const XML_Char *entityName, int is_parameter_entity,
                   const XML_Char *value, int value_length, const XML_Char *base,
                   const XML_Char *systemId, const XML_Char *publicId, const XML_Char *notationName)
{
    ParserObj *self = self_of(ud);
    if (!wants(self, H_EntityDecl))
        return;
    Value a[7];
    Roots pin{ a, 7 };
    a[0] = conv_intern(self, entityName);
    a[1] = a[0].is_nil() ? Value() : int_from_i64(is_parameter_entity);
    a[2] = a[1].is_nil() ? Value() : (value ? conv_n(value, value_length) : value_none());
    const XML_Char *src[4] = { base, systemId, publicId, notationName };
    for (u32 i = 0; i < 4 && !a[2].is_nil(); i++)
        a[3 + i] = conv_intern(self, src[i]);
    for (u32 i = 0; i < 7; i++)
        if (a[i].is_nil()) {
            fail(self);
            return;
        }
    queue(self, H_EntityDecl, a, 7);
}

void h_xml_decl(void *ud, const XML_Char *version, const XML_Char *encoding, int standalone)
{
    ParserObj *self = self_of(ud);
    if (!wants(self, H_XmlDecl))
        return;
    Value a[3];
    Roots pin{ a, 3 };
    a[0] = conv(version);
    a[1] = a[0].is_nil() ? Value() : conv(encoding);
    a[2] = a[1].is_nil() ? Value() : int_from_i64(standalone);
    if (a[2].is_nil()) {
        fail(self);
        return;
    }
    queue(self, H_XmlDecl, a, 3);
}

// A content model is a tree of (type, quant, name, children) tuples. It is
// walked recursively, and the depth is the DTD's, so a model nested past the
// native stack would be a trap: the limit here is the same 200 the VM has.
Value conv_model(const XML_Content *m, u32 depth)
{
    if (depth > 200) {
        err_set("RecursionError", "maximum recursion depth exceeded in a content model");
        return Value();
    }
    Value a[4];
    Roots pin{ a, 4 };
    TupleObj *kids = tuple_new(m->numchildren);
    if (!kids)
        return Value();
    a[3] = obj_value(kids);
    for (unsigned i = 0; i < m->numchildren; i++) {
        Value child = conv_model(&m->children[i], depth + 1);
        if (child.is_nil())
            return Value();
        static_cast<TupleObj *>(a[3].obj())->items()[i] = child;
    }
    a[0] = int_from_i64(m->type);
    a[1] = a[0].is_nil() ? Value() : int_from_i64(m->quant);
    a[2] = a[1].is_nil() ? Value() : conv(m->name);
    if (a[2].is_nil())
        return Value();
    TupleObj *t = tuple_new(4);
    if (!t)
        return Value();
    for (u32 i = 0; i < 4; i++)
        t->items()[i] = a[i];
    return obj_value(t);
}

void h_element_decl(void *ud, const XML_Char *name, XML_Content *model)
{
    ParserObj *self = self_of(ud);
    if (wants(self, H_ElementDecl)) {
        Value a[2];
        Roots pin{ a, 2 };
        a[0] = conv_intern(self, name);
        a[1] = a[0].is_nil() ? Value() : conv_model(model, 0);
        if (a[1].is_nil()) {
            if (!err_pending())
                oom();
            fail(self);
        } else {
            queue(self, H_ElementDecl, a, 2);
        }
    }
    XML_FreeContentModel(self->p, model);
}

void h_attlist_decl(void *ud, const XML_Char *elname, const XML_Char *attname,
                    const XML_Char *att_type, const XML_Char *dflt, int isrequired)
{
    ParserObj *self = self_of(ud);
    if (!wants(self, H_AttlistDecl))
        return;
    Value a[5];
    Roots pin{ a, 5 };
    a[0] = conv_intern(self, elname);
    a[1] = a[0].is_nil() ? Value() : conv_intern(self, attname);
    a[2] = a[1].is_nil() ? Value() : conv(att_type);
    a[3] = a[2].is_nil() ? Value() : conv(dflt);
    a[4] = a[3].is_nil() ? Value() : int_from_i64(isrequired);
    if (a[4].is_nil()) {
        fail(self);
        return;
    }
    queue(self, H_AttlistDecl, a, 5);
}

void h_skipped_entity(void *ud, const XML_Char *entityName, int is_parameter_entity)
{
    ParserObj *self = self_of(ud);
    if (!wants(self, H_SkippedEntity))
        return;
    Value a[2];
    Roots pin{ a, 2 };
    a[0] = conv_intern(self, entityName);
    a[1] = a[0].is_nil() ? Value() : int_from_i64(is_parameter_entity);
    if (a[1].is_nil()) {
        fail(self);
        return;
    }
    queue(self, H_SkippedEntity, a, 2);
}

// The two that steer the parse. Each suspends and answers expat with the
// value that lets the parse reach the suspension check; what Python really
// said is applied on the way back, through XmlFailSuspendedParse.
int h_not_standalone(void *ud)
{
    ParserObj *self = self_of(ud);
    if (wants(self, H_NotStandalone))
        queue(self, H_NotStandalone, nullptr, 0);
    return 1;
}

int h_external_entity_ref(XML_Parser parser, const XML_Char *context, const XML_Char *base,
                          const XML_Char *systemId, const XML_Char *publicId)
{
    ParserObj *self = static_cast<ParserObj *>(XML_GetUserData(parser));
    if (!wants(self, H_ExternalEntityRef))
        return 1;
    Value a[4];
    Roots pin{ a, 4 };
    a[0] = conv(context);
    a[1] = a[0].is_nil() ? Value() : conv_intern(self, base);
    a[2] = a[1].is_nil() ? Value() : conv_intern(self, systemId);
    a[3] = a[2].is_nil() ? Value() : conv_intern(self, publicId);
    if (a[3].is_nil()) {
        fail(self);
        return 1;
    }
    queue(self, H_ExternalEntityRef, a, 4);
    return 1;
}

// --------------------------------------------------------- wiring a handler

void set_handler(ParserObj *self, u32 h, bool on)
{
    XML_Parser p = self->p;
    switch (h) {
    case H_StartElement:
        XML_SetStartElementHandler(p, on ? h_start_element : nullptr);
        break;
    case H_EndElement:
        XML_SetEndElementHandler(p, on ? h_end_element : nullptr);
        break;
    case H_ProcessingInstruction:
        XML_SetProcessingInstructionHandler(p, on ? h_processing_instruction : nullptr);
        break;
    case H_CharacterData:
        XML_SetCharacterDataHandler(p, on ? h_character_data : nullptr);
        break;
    case H_UnparsedEntityDecl:
        XML_SetUnparsedEntityDeclHandler(p, on ? h_unparsed_entity_decl : nullptr);
        break;
    case H_NotationDecl:
        XML_SetNotationDeclHandler(p, on ? h_notation_decl : nullptr);
        break;
    case H_StartNamespaceDecl:
        XML_SetStartNamespaceDeclHandler(p, on ? h_start_namespace_decl : nullptr);
        break;
    case H_EndNamespaceDecl:
        XML_SetEndNamespaceDeclHandler(p, on ? h_end_namespace_decl : nullptr);
        break;
    case H_Comment:
        XML_SetCommentHandler(p, on ? h_comment : nullptr);
        break;
    case H_StartCdataSection:
        XML_SetStartCdataSectionHandler(p, on ? h_start_cdata : nullptr);
        break;
    case H_EndCdataSection:
        XML_SetEndCdataSectionHandler(p, on ? h_end_cdata : nullptr);
        break;
    case H_Default:
        XML_SetDefaultHandler(p, on ? h_default : nullptr);
        break;
    case H_DefaultHandlerExpand:
        XML_SetDefaultHandlerExpand(p, on ? h_default_expand : nullptr);
        break;
    case H_NotStandalone:
        XML_SetNotStandaloneHandler(p, on ? h_not_standalone : nullptr);
        break;
    case H_ExternalEntityRef:
        XML_SetExternalEntityRefHandler(p, on ? h_external_entity_ref : nullptr);
        break;
    case H_StartDoctypeDecl:
        XML_SetStartDoctypeDeclHandler(p, on ? h_start_doctype_decl : nullptr);
        break;
    case H_EndDoctypeDecl:
        XML_SetEndDoctypeDeclHandler(p, on ? h_end_doctype_decl : nullptr);
        break;
    case H_EntityDecl:
        XML_SetEntityDeclHandler(p, on ? h_entity_decl : nullptr);
        break;
    case H_XmlDecl:
        XML_SetXmlDeclHandler(p, on ? h_xml_decl : nullptr);
        break;
    case H_ElementDecl:
        XML_SetElementDeclHandler(p, on ? h_element_decl : nullptr);
        break;
    case H_AttlistDecl:
        XML_SetAttlistDeclHandler(p, on ? h_attlist_decl : nullptr);
        break;
    case H_SkippedEntity:
        XML_SetSkippedEntityHandler(p, on ? h_skipped_entity : nullptr);
        break;
    default:
        break;
    }
}

// --------------------------------------------------------------- the driver

enum Phase : u32 { PH_FEED, PH_READ, PH_GOT, PH_DRAIN, PH_END };

// What a handler that steers the parse answered. CPython reads it with
// PyLong_AsLong, so an int or a bool and nothing else.
bool handler_rc(Value v, i64 &out)
{
    if (!is_intval(v)) {
        type_error("'", v, "' object cannot be interpreted as an integer");
        return false;
    }
    return int_to_i64(v, out);
}

R drive(ContObj *k, Value in);

R drive_fail(ParserObj *self)
{
    // The parse is over however this ends: a handler that raised or an event
    // that could not be built leaves the parser finished, as CPython's
    // flag_error leaves it handler-less.
    self->failed = true;
    if (self->p)
        XML_StopParser(self->p, XML_FALSE);
    self->buf.clear();
    self->evi     = 0;
    self->pending = -1;
    if (!self->evq.is_nil())
        static_cast<ListObj *>(self->evq.obj())->items.clear();
    self->hold   = Value();
    self->readfn = Value();
    return R::Err;
}

void drive_unwound(ContObj *k)
{
    if (is_parser(k->s[0]))
        drive_fail(parser_of(k->s[0]));
}

// One queued event, if there is one. R::NotImpl means the queue is empty.
R next_event(ContObj *k, ParserObj *self)
{
    ListObj *q = static_cast<ListObj *>(self->evq.obj());
    while (self->evi < q->items.size()) {
        i64 h = 0;
        if (!int_to_i64(q->items[self->evi], h))
            return R::Err;
        Value args = q->items[self->evi + 1];
        self->evi += 2;
        // A handler the last event's call took away has nothing left to
        // answer: expatbuilder reassigns StartElementHandler mid-parse.
        if (self->handlers[usize(h)].is_nil())
            continue;
        self->pending = i32(h);
        return cont_call_v(k, self->handlers[usize(h)], args);
    }
    self->evi = 0;
    q->items.clear();
    return R::NotImpl;
}

// The answer of the handler the step was waiting for.
bool take_answer(ParserObj *self, Value in)
{
    i32 h         = self->pending;
    self->pending = -1;
    if (h != H_ExternalEntityRef && h != H_NotStandalone)
        return true;
    i64 rc = 0;
    if (!handler_rc(in, rc))
        return false;
    if (rc == 0)
        XmlFailSuspendedParse(self->p, h == H_ExternalEntityRef ? XML_ERROR_EXTERNAL_ENTITY_HANDLING
                                                                : XML_ERROR_NOT_STANDALONE);
    return true;
}

R drive(ContObj *k, Value in)
{
    ParserObj *self = parser_of(k->s[0]);
    if (self->pending >= 0 && !take_answer(self, in))
        return drive_fail(self);

    for (;;) {
        R got = next_event(k, self);
        if (got == R::Err)
            return drive_fail(self);
        if (got == R::Ok)
            return R::Ok;
        if (self->failed)
            return drive_fail(self);

        switch (k->i) {
        case PH_FEED: {
            Str data = Str(reinterpret_cast<const char *>(k->x[0]), usize(k->x[1]));
            k->i     = PH_DRAIN;
            if (XML_Parse(self->p, data.data(), int(data.size()), int(k->x[2])) == XML_STATUS_ERROR)
                k->i = PH_END;
            break;
        }
        case PH_READ: {
            Value n = int_from_i64(i64(self->buffer_size));
            if (n.is_nil())
                return drive_fail(self);
            k->i = PH_GOT;
            return cont_call(k, k->s[1], n);
        }
        case PH_GOT: {
            Str data;
            if (!buffer_like(in, data)) {
                err_set2("TypeError", "read() did not return bytes", type_name(in));
                return drive_fail(self);
            }
            k->s[2] = in; // the chunk, alive while expat looks at it
            k->i    = PH_DRAIN;
            k->x[3] = data.empty();
            if (XML_Parse(self->p, data.data(), int(data.size()), data.empty() ? 1 : 0) ==
                XML_STATUS_ERROR)
                k->i = PH_END;
            break;
        }
        case PH_DRAIN: {
            XML_ParsingStatus st;
            XML_GetParsingStatus(self->p, &st);
            if (st.parsing == XML_SUSPENDED) {
                if (XML_ResumeParser(self->p) == XML_STATUS_ERROR)
                    k->i = PH_END;
                break;
            }
            // Nothing suspended, so the buffer is spent: the last of the
            // character data becomes an event, and then it is over.
            if (!flush_text(self)) {
                if (!err_pending())
                    oom();
                return drive_fail(self);
            }
            if (k->j == 1 && k->x[3] == 0) {
                k->i = PH_READ;
                break;
            }
            k->i = PH_END;
            break;
        }
        case PH_END: {
            if (self->failed)
                return drive_fail(self);
            enum XML_Error code = XML_GetErrorCode(self->p);
            self->hold          = Value();
            self->readfn        = Value();
            if (code != XML_ERROR_NONE)
                return parse_error(self, code);
            Value one = int_from_i64(1);
            return one.is_nil() ? R::Err : cont_done(k, one);
        }
        }
    }
}

// The ContObj a Parse or a ParseFile hands back.
R start_drive(Value pv, Value &out, u32 phase)
{
    Root p{ pv };
    Root kv{ cont_new(drive) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = p.v;
    k->fail    = drive_unwound;
    k->i       = phase;
    out        = kv.v;
    return R::Ok;
}

// ------------------------------------------------------------- the methods

ParserObj *self_parser(const CallArgs &a, Str who)
{
    if (!a.nargs || !is_parser(method_self(a.args[0]))) {
        err_set2("TypeError", "a pyexpat parser is required", who);
        return nullptr;
    }
    ParserObj *self = parser_of(method_self(a.args[0]));
    if (!self->p) {
        err_set("ValueError", "parser is closed");
        return nullptr;
    }
    return self;
}

// A parse that is parked on a handler holds a pointer into the buffer it was
// given, so a second one on the same parser would take that away under it.
// Expat refuses it too, one call later; this is that refusal, in time.
bool reentered(ParserObj *self, Str who)
{
    XML_ParsingStatus st;
    XML_GetParsingStatus(self->p, &st);
    if (st.parsing != XML_SUSPENDED)
        return false;
    Buf<96> b;
    b.put(who).put("() cannot be called from inside a handler");
    err_set("RuntimeError", b.str());
    return true;
}

R pm_parse(const CallArgs &a, Value &out)
{
    ParserObj *self = self_parser(a, "Parse");
    if (!self || !meth_args(a, "Parse", 1, 2))
        return R::Err;
    if (reentered(self, "Parse"))
        return R::Err;
    Value data = a.args[1];
    Str bytes;
    if (is_str(data)) {
        bytes = str_of(data)->str();
        XML_SetEncoding(self->p, "utf-8");
    } else if (!buffer_like(data, bytes)) {
        return type_error("a bytes-like object is required, not '", data, "'");
    }
    i64 final = 0;
    if (a.nargs > 2 && !int_to_i64(a.args[2], final))
        final = py_truth(a.args[2]) ? 1 : 0;
    self->hold = data;
    R r        = start_drive(method_self(a.args[0]), out, PH_FEED);
    if (r != R::Ok)
        return r;
    ContObj *k = cont_of(out);
    k->x[0]    = i64(reinterpret_cast<usize>(bytes.data()));
    k->x[1]    = i64(bytes.size());
    k->x[2]    = final ? 1 : 0;
    return R::Ok;
}

R pm_parse_file(const CallArgs &a, Value &out)
{
    ParserObj *self = self_parser(a, "ParseFile");
    if (!self || !meth_args(a, "ParseFile", 1, 1))
        return R::Err;
    if (reentered(self, "ParseFile"))
        return R::Err;
    Root file{ a.args[1] };
    StrObj *name = str_intern("read");
    if (!name)
        return R::Err;
    Value fn;
    R got = method_find(file.v, name, fn);
    if (got == R::Err)
        return R::Err;
    if (got == R::NotImpl) {
        Got g = py_attr(file.v, name, fn);
        if (g == Got::Error)
            return R::Err;
        if (g != Got::Ok)
            return err_set("TypeError", "argument must have 'read' attribute");
    }
    self->readfn = fn;
    R r          = start_drive(method_self(a.args[0]), out, PH_READ);
    if (r != R::Ok)
        return r;
    ContObj *k = cont_of(out);
    k->s[1]    = fn;
    k->j       = 1;
    return R::Ok;
}

R pm_setbase(const CallArgs &a, Value &out)
{
    ParserObj *self = self_parser(a, "SetBase");
    if (!self || !meth_args(a, "SetBase", 1, 1))
        return R::Err;
    if (!is_str(a.args[1]))
        return err_set2("TypeError", "SetBase() argument must be str", type_name(a.args[1]));
    String base;
    if (!as_cstr(a.args[1], base))
        return oom();
    if (!XML_SetBase(self->p, base.data()))
        return oom();
    out = value_none();
    return R::Ok;
}

R pm_getbase(const CallArgs &a, Value &out)
{
    ParserObj *self = self_parser(a, "GetBase");
    if (!self || !meth_args(a, "GetBase", 0, 0))
        return R::Err;
    out = conv(XML_GetBase(self->p));
    return out.is_nil() ? R::Err : R::Ok;
}

R pm_getspecified(const CallArgs &a, Value &out)
{
    ParserObj *self = self_parser(a, "GetSpecifiedAttributeCount");
    if (!self || !meth_args(a, "GetSpecifiedAttributeCount", 0, 0))
        return R::Err;
    out = int_from_i64(XML_GetSpecifiedAttributeCount(self->p));
    return out.is_nil() ? R::Err : R::Ok;
}

R pm_getinputcontext(const CallArgs &a, Value &out)
{
    ParserObj *self = self_parser(a, "GetInputContext");
    if (!self || !meth_args(a, "GetInputContext", 0, 0))
        return R::Err;
    // What is left of the buffer from the event on, which is what CPython
    // answers; and None outside a handler, which is where there is no event.
    XML_ParsingStatus st;
    XML_GetParsingStatus(self->p, &st);
    int offset = 0, size = 0;
    const char *ctx =
        st.parsing == XML_SUSPENDED ? XML_GetInputContext(self->p, &offset, &size) : nullptr;
    out = ctx ? bytes_new(Str(ctx + offset, usize(size - offset))) : value_none();
    return out.is_nil() ? R::Err : R::Ok;
}

R pm_setparamentityparsing(const CallArgs &a, Value &out)
{
    ParserObj *self = self_parser(a, "SetParamEntityParsing");
    if (!self || !meth_args(a, "SetParamEntityParsing", 1, 1))
        return R::Err;
    i64 flag = 0;
    if (!int_to_i64(a.args[1], flag))
        return type_error("'", a.args[1], "' object cannot be interpreted as an integer");
    out = int_from_i64(XML_SetParamEntityParsing(self->p, XML_ParamEntityParsing(flag)));
    return out.is_nil() ? R::Err : R::Ok;
}

R pm_useforeigndtd(const CallArgs &a, Value &out)
{
    ParserObj *self = self_parser(a, "UseForeignDTD");
    if (!self || !meth_args(a, "UseForeignDTD", 0, 1))
        return R::Err;
    bool flag         = a.nargs > 1 ? py_truth(a.args[1]) : true;
    enum XML_Error rc = XML_UseForeignDTD(self->p, flag ? XML_TRUE : XML_FALSE);
    if (rc != XML_ERROR_NONE)
        return parse_error(self, rc);
    out = value_none();
    return R::Ok;
}

R pm_setreparse(const CallArgs &a, Value &out)
{
    ParserObj *self = self_parser(a, "SetReparseDeferralEnabled");
    if (!self || !meth_args(a, "SetReparseDeferralEnabled", 1, 1))
        return R::Err;
    self->reparse = py_truth(a.args[1]);
    XML_SetReparseDeferralEnabled(self->p, self->reparse ? XML_TRUE : XML_FALSE);
    out = value_none();
    return R::Ok;
}

R pm_getreparse(const CallArgs &a, Value &out)
{
    ParserObj *self = self_parser(a, "GetReparseDeferralEnabled");
    if (!self || !meth_args(a, "GetReparseDeferralEnabled", 0, 0))
        return R::Err;
    out = value_bool(self->reparse);
    return R::Ok;
}

Value parser_make(const XML_Char *encoding, const XML_Char *nssep, Value interned, ParserObj *from,
                  const XML_Char *context);

R pm_externalcreate(const CallArgs &a, Value &out)
{
    ParserObj *self = self_parser(a, "ExternalEntityParserCreate");
    if (!self || !meth_args(a, "ExternalEntityParserCreate", 1, 2))
        return R::Err;
    String ctxbuf, encbuf;
    const char *context = nullptr;
    if (!is_none(a.args[1])) {
        if (!is_str(a.args[1]))
            return err_set2("TypeError", "context must be str or None", type_name(a.args[1]));
        if (!as_cstr(a.args[1], ctxbuf))
            return oom();
        context = ctxbuf.data();
    }
    const char *encoding = nullptr;
    if (a.nargs > 2 && !is_none(a.args[2])) {
        if (!is_str(a.args[2]))
            return err_set2("TypeError", "encoding must be str or None", type_name(a.args[2]));
        if (!as_cstr(a.args[2], encbuf))
            return oom();
        encoding = encbuf.data();
    }
    Root parent{ method_self(a.args[0]) };
    out = parser_make(encoding, nullptr, self->intern, parser_of(parent.v), context);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method PARSER_METHODS[] = {
    { "Parse", pm_parse },
    { "ParseFile", pm_parse_file },
    { "SetBase", pm_setbase },
    { "GetBase", pm_getbase },
    { "GetSpecifiedAttributeCount", pm_getspecified },
    { "GetInputContext", pm_getinputcontext },
    { "ExternalEntityParserCreate", pm_externalcreate },
    { "SetParamEntityParsing", pm_setparamentityparsing },
    { "UseForeignDTD", pm_useforeigndtd },
    { "SetReparseDeferralEnabled", pm_setreparse },
    { "GetReparseDeferralEnabled", pm_getreparse },
};

// ------------------------------------------------------------- the attributes

i32 handler_index(Str name)
{
    for (u32 i = 0; i < H_COUNT; i++)
        if (HANDLER_NAMES[i] == name)
            return i32(i);
    return -1;
}

R parser_getattr(Value v, StrObj *name, Value &out)
{
    ParserObj *self = parser_of(v);
    Str n           = name->str();
    i32 h           = handler_index(n);
    if (h >= 0) {
        out = self->handlers[h].is_nil() ? value_none() : self->handlers[h];
        return R::Ok;
    }
    if (!self->p)
        return R::NotImpl;
    if (n == "intern") {
        out = self->intern.is_nil() ? value_none() : self->intern;
        return R::Ok;
    }
    struct {
        Str name;
        XML_Size (*fn)(XML_Parser);
    } sizes[] = { { "CurrentLineNumber", XML_GetCurrentLineNumber },
                  { "CurrentColumnNumber", XML_GetCurrentColumnNumber },
                  { "ErrorLineNumber", XML_GetCurrentLineNumber },
                  { "ErrorColumnNumber", XML_GetCurrentColumnNumber } };
    for (auto &s : sizes)
        if (s.name == n) {
            out = int_from_i64(i64(s.fn(self->p)));
            return out.is_nil() ? R::Err : R::Ok;
        }
    if (n == "CurrentByteIndex" || n == "ErrorByteIndex") {
        out = int_from_i64(i64(XML_GetCurrentByteIndex(self->p)));
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (n == "ErrorCode") {
        out = int_from_i64(i64(XML_GetErrorCode(self->p)));
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (n == "buffer_size") {
        out = int_from_i64(i64(self->buffer_size));
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (n == "buffer_used") {
        out = int_from_i64(i64(self->buf.size()));
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (n == "buffer_text") {
        out = value_bool(self->buffer_text);
        return R::Ok;
    }
    if (n == "ordered_attributes") {
        out = value_bool(self->ordered_attributes);
        return R::Ok;
    }
    if (n == "specified_attributes") {
        out = value_bool(self->specified_attributes);
        return R::Ok;
    }
    if (n == "namespace_prefixes") {
        out = value_bool(self->ns_prefixes);
        return R::Ok;
    }
    return R::NotImpl;
}

R parser_setattr(Value v, StrObj *name, Value val)
{
    ParserObj *self = parser_of(v);
    Str n           = name->str();
    i32 h           = handler_index(n);
    if (h >= 0) {
        if (val.is_nil())
            return err_set2("AttributeError", "cannot delete", n);
        bool on           = !is_none(val);
        self->handlers[h] = on ? val : Value();
        if (self->p)
            set_handler(self, u32(h), on);
        return R::Ok;
    }
    if (val.is_nil())
        return err_set2("AttributeError", "cannot delete", n);
    if (n == "buffer_text") {
        bool on = py_truth(val);
        if (!on && !self->buf.empty()) {
            // Turning it off mid-parse drops what was gathered; CPython
            // flushes, which it can do because it may call Python here.
            if (!flush_text(self))
                return oom();
        }
        self->buffer_text = on;
        return R::Ok;
    }
    if (n == "buffer_size") {
        i64 size = 0;
        if (!int_to_i64(val, size))
            return err_set2("TypeError", "buffer_size must be an integer", type_name(val));
        if (size <= 0)
            return err_set("ValueError", "buffer_size must be greater than zero");
        self->buffer_size = usize(size);
        return R::Ok;
    }
    if (n == "ordered_attributes") {
        self->ordered_attributes = py_truth(val);
        return R::Ok;
    }
    if (n == "specified_attributes") {
        self->specified_attributes = py_truth(val);
        return R::Ok;
    }
    if (n == "namespace_prefixes") {
        self->ns_prefixes = py_truth(val);
        if (self->p)
            XML_SetReturnNSTriplet(self->p, self->ns_prefixes ? 1 : 0);
        return R::Ok;
    }
    return err_set2("AttributeError", "pyexpat.xmlparser has no attribute", n);
}

R parser_repr(Value v, String &out)
{
    Buf<64> b;
    b.put("<pyexpat.xmlparser object at ");
    b.put_hex(u32(reinterpret_cast<usize>(v.obj())));
    b.put(">");
    return out.append(b.str()) ? R::Ok : oom();
}

void parser_trace(Obj *o)
{
    ParserObj *s = static_cast<ParserObj *>(o);
    for (Value &h : s->handlers)
        gc_mark(h);
    gc_mark(s->intern);
    gc_mark(s->parent);
    gc_mark(s->evq);
    gc_mark(s->hold);
    gc_mark(s->readfn);
}

void parser_fini(Obj *o)
{
    ParserObj *s = static_cast<ParserObj *>(o);
    // A parser made by ExternalEntityParserCreate shares its parent's DTD and
    // reaches the root parser to free a block, so it must go first. It always
    // does: the sweep walks the heap newest first and a child is newer.
    if (s->p) {
        XML_ParserFree(s->p);
        s->p = nullptr;
    }
    s->buf.~String();
}

constexpr Type parser_type{ .name    = "pyexpat.xmlparser",
                            .trace   = parser_trace,
                            .fini    = parser_fini,
                            .repr    = parser_repr,
                            .getattr = parser_getattr,
                            .setattr = parser_setattr };

// ------------------------------------------------------------ making one

Value parser_make(const XML_Char *encoding, const XML_Char *nssep, Value interned, ParserObj *from,
                  const XML_Char *context)
{
    Root iv{ interned };
    Root pv{ from ? obj_value(from) : Value() };
    ListObj *q = list_new();
    if (!q)
        return (oom(), Value());
    Root qv{ obj_value(q) };
    ParserObj *self = static_cast<ParserObj *>(obj_alloc(&parser_type, sizeof(ParserObj)));
    if (!self)
        return (oom(), Value());
    new (&self->buf) String();
    for (Value &h : self->handlers)
        h = Value();
    self->intern  = iv.v;
    self->parent  = pv.v;
    self->evq     = qv.v;
    self->hold    = Value();
    self->readfn  = Value();
    self->p       = nullptr;
    self->evi     = 0;
    self->pending = -1;
    self->failed  = false;

    ParserObj *parent          = pv.v.is_nil() ? nullptr : parser_of(pv.v);
    self->buffer_size          = parent ? parent->buffer_size : 8192;
    self->buffer_text          = parent ? parent->buffer_text : false;
    self->ordered_attributes   = parent ? parent->ordered_attributes : false;
    self->specified_attributes = parent ? parent->specified_attributes : false;
    self->ns_prefixes          = parent ? parent->ns_prefixes : false;
    self->ns                   = parent ? parent->ns : (nssep != nullptr);
    self->reparse              = parent ? parent->reparse : true;

    Root sv{ obj_value(self) };
    if (parent)
        self->p = XML_ExternalEntityParserCreate(parent->p, context, encoding);
    else if (nssep)
        self->p = XML_ParserCreateNS(encoding, nssep[0]);
    else
        self->p = XML_ParserCreate(encoding);
    if (!self->p)
        return (oom(), Value());
    XML_SetUserData(self->p, self);
    if (self->ns_prefixes)
        XML_SetReturnNSTriplet(self->p, 1);
    if (parent) {
        XML_SetReparseDeferralEnabled(self->p, self->reparse ? XML_TRUE : XML_FALSE);
        for (u32 i = 0; i < H_COUNT; i++)
            if (!parent->handlers[i].is_nil()) {
                self->handlers[i] = parent->handlers[i];
                set_handler(self, i, true);
            }
    }
    return sv.v;
}

R b_parser_create(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "encoding", "namespace_separator", "intern" };
    Value got[3];
    Roots pin{ got, 3 };
    if (!func_take(a, "ParserCreate", NAMES, 0, got))
        return R::Err;
    String encbuf;
    const char *encoding = nullptr;
    if (!got[0].is_nil() && !is_none(got[0])) {
        if (!is_str(got[0]))
            return type_error("ParserCreate() argument 'encoding' must be str or None, not ",
                              got[0], "");
        if (!as_cstr(got[0], encbuf))
            return oom();
        encoding = encbuf.data();
    }
    XML_Char sep[2]       = { 0, 0 };
    const XML_Char *nssep = nullptr;
    if (!got[1].is_nil() && !is_none(got[1])) {
        if (!is_str(got[1]))
            return type_error(
                "ParserCreate() argument 'namespace_separator' must be str or None, not ", got[1],
                "");
        Str s = str_of(got[1])->str();
        if (s.size() > 1)
            return err_set("ValueError",
                           "namespace_separator must be at most one character, omitted, or None");
        sep[0] = s.empty() ? '\0' : s[0];
        nssep  = sep;
    }
    Root interned;
    if (!got[2].is_nil() && !is_none(got[2])) {
        if (!got[2].is_obj() || !is_dict(got[2]))
            return type_error("ParserCreate() argument 'intern' must be dict or None, not ", got[2],
                              "");
        interned = got[2];
    } else if (got[2].is_nil()) {
        DictObj *d = dict_new();
        if (!d)
            return oom();
        interned = obj_value(d);
    }
    out = parser_make(encoding, nssep, interned.v, nullptr, nullptr);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_error_string(const CallArgs &a, Value &out)
{
    if (!args_only(a, "ErrorString", 1, 1))
        return R::Err;
    i64 code = 0;
    if (!int_to_i64(a.args[0], code))
        return type_error("'", a.args[0], "' object cannot be interpreted as an integer");
    const XML_LChar *s = XML_ErrorString(XML_Error(code));
    out                = s ? str_new(Str(s, expat_strlen(s))) : value_none();
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr ModDef DEFS[] = {
    { "ParserCreate", b_parser_create },
    { "ErrorString", b_error_string },
};

// --------------------------------------------------------- the error names

// Code 0 is not exposed: XML_ERROR_NONE is not an error.
constexpr Str ERROR_NAMES[] = {
    "",
    "XML_ERROR_NO_MEMORY",
    "XML_ERROR_SYNTAX",
    "XML_ERROR_NO_ELEMENTS",
    "XML_ERROR_INVALID_TOKEN",
    "XML_ERROR_UNCLOSED_TOKEN",
    "XML_ERROR_PARTIAL_CHAR",
    "XML_ERROR_TAG_MISMATCH",
    "XML_ERROR_DUPLICATE_ATTRIBUTE",
    "XML_ERROR_JUNK_AFTER_DOC_ELEMENT",
    "XML_ERROR_PARAM_ENTITY_REF",
    "XML_ERROR_UNDEFINED_ENTITY",
    "XML_ERROR_RECURSIVE_ENTITY_REF",
    "XML_ERROR_ASYNC_ENTITY",
    "XML_ERROR_BAD_CHAR_REF",
    "XML_ERROR_BINARY_ENTITY_REF",
    "XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF",
    "XML_ERROR_MISPLACED_XML_PI",
    "XML_ERROR_UNKNOWN_ENCODING",
    "XML_ERROR_INCORRECT_ENCODING",
    "XML_ERROR_UNCLOSED_CDATA_SECTION",
    "XML_ERROR_EXTERNAL_ENTITY_HANDLING",
    "XML_ERROR_NOT_STANDALONE",
    "XML_ERROR_UNEXPECTED_STATE",
    "XML_ERROR_ENTITY_DECLARED_IN_PE",
    "XML_ERROR_FEATURE_REQUIRES_XML_DTD",
    "XML_ERROR_CANT_CHANGE_FEATURE_ONCE_PARSING",
    "XML_ERROR_UNBOUND_PREFIX",
    "XML_ERROR_UNDECLARING_PREFIX",
    "XML_ERROR_INCOMPLETE_PE",
    "XML_ERROR_XML_DECL",
    "XML_ERROR_TEXT_DECL",
    "XML_ERROR_PUBLICID",
    "XML_ERROR_SUSPENDED",
    "XML_ERROR_NOT_SUSPENDED",
    "XML_ERROR_ABORTED",
    "XML_ERROR_FINISHED",
    "XML_ERROR_SUSPEND_PE",
    "XML_ERROR_RESERVED_PREFIX_XML",
    "XML_ERROR_RESERVED_PREFIX_XMLNS",
    "XML_ERROR_RESERVED_NAMESPACE_URI",
    "XML_ERROR_INVALID_ARGUMENT",
    "XML_ERROR_NO_BUFFER",
    "XML_ERROR_AMPLIFICATION_LIMIT_BREACH",
    "XML_ERROR_NOT_STARTED",
};

bool build_errors(Home *h)
{
    Root mv{ module_new("pyexpat.errors") };
    if (mv.v.is_nil())
        return false;
    DictObj *d = module_dict(mv.v);
    if (!module_defaults(d) ||
        !mod_str(d, "__doc__", "Constants used to describe error conditions."))
        return false;
    DictObj *codes = dict_new();
    if (!codes)
        return (oom(), false);
    Root cv{ obj_value(codes) };
    DictObj *msgs = dict_new();
    if (!msgs)
        return (oom(), false);
    Root sv{ obj_value(msgs) };
    for (u32 i = 1; i < sizeof(ERROR_NAMES) / sizeof(ERROR_NAMES[0]); i++) {
        const XML_LChar *text = XML_ErrorString(XML_Error(i));
        Root t{ str_new(Str(text, expat_strlen(text))) };
        Root n{ int_from_i64(i64(i)) };
        if (t.v.is_nil() || n.v.is_nil())
            return false;
        d = module_dict(mv.v);
        if (!mod_put(d, ERROR_NAMES[i], t.v) ||
            dict_set(static_cast<DictObj *>(cv.v.obj()), t.v, n.v) != R::Ok ||
            dict_set(static_cast<DictObj *>(sv.v.obj()), n.v, t.v) != R::Ok)
            return false;
    }
    d = module_dict(mv.v);
    if (!mod_put(d, "codes", cv.v) || !mod_put(d, "messages", sv.v))
        return false;
    h->errors = mv.v;
    return true;
}

bool build_model(Home *h)
{
    Root mv{ module_new("pyexpat.model") };
    if (mv.v.is_nil())
        return false;
    DictObj *d = module_dict(mv.v);
    if (!module_defaults(d) ||
        !mod_str(d, "__doc__", "Constants used to interpret content model information."))
        return false;
    struct {
        Str name;
        i64 value;
    } consts[] = {
        { "XML_CTYPE_EMPTY", XML_CTYPE_EMPTY },   { "XML_CTYPE_ANY", XML_CTYPE_ANY },
        { "XML_CTYPE_MIXED", XML_CTYPE_MIXED },   { "XML_CTYPE_NAME", XML_CTYPE_NAME },
        { "XML_CTYPE_CHOICE", XML_CTYPE_CHOICE }, { "XML_CTYPE_SEQ", XML_CTYPE_SEQ },
        { "XML_CQUANT_NONE", XML_CQUANT_NONE },   { "XML_CQUANT_OPT", XML_CQUANT_OPT },
        { "XML_CQUANT_REP", XML_CQUANT_REP },     { "XML_CQUANT_PLUS", XML_CQUANT_PLUS },
    };
    for (auto &c : consts)
        if (!mod_int(module_dict(mv.v), c.name, c.value))
            return false;
    h->model = mv.v;
    return true;
}

bool build_features(DictObj *into)
{
    ListObj *l = list_new();
    if (!l)
        return (oom(), false);
    Root lv{ obj_value(l) };
    const XML_Feature *f = XML_GetFeatureList();
    for (usize i = 0; f[i].feature != XML_FEATURE_END; i++) {
        Root name{ str_new(Str(f[i].name, expat_strlen(f[i].name))) };
        Root value{ int_from_i64(i64(f[i].value)) };
        if (name.v.is_nil() || value.v.is_nil())
            return false;
        TupleObj *t = tuple_new(2);
        if (!t)
            return (oom(), false);
        t->items()[0] = name.v;
        t->items()[1] = value.v;
        if (!list_push(static_cast<ListObj *>(lv.v.obj()), obj_value(t)))
            return (oom(), false);
    }
    return mod_put(into, "features", lv.v);
}

} // namespace

bool pyexpat_install(DictObj *into)
{
    Home *h = here();
    if (!h)
        return oom() == R::Ok;
    Root rd{ obj_value(into) };
    if (h->error.is_nil()) {
        h->error = mod_exc_class("pyexpat", "ExpatError", "Exception");
        if (h->error.is_nil())
            return false;
    }
    if (h->errors.is_nil() && !build_errors(h))
        return false;
    if (h->model.is_nil() && !build_model(h))
        return false;
    if (!method_install(&parser_type, PARSER_METHODS))
        return false;

    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_defs(d, DEFS) || !mod_put(d, "error", h->error) ||
        !mod_put(d, "ExpatError", h->error) || !mod_put(d, "errors", h->errors) ||
        !mod_put(d, "model", h->model))
        return false;
    Root pt{ type_wrap(&parser_type) };
    if (pt.v.is_nil() || !mod_put(static_cast<DictObj *>(rd.v.obj()), "XMLParserType", pt.v))
        return false;

    const XML_LChar *ver = XML_ExpatVersion();
    XML_Expat_Version vi = XML_ExpatVersionInfo();
    Root tv;
    {
        TupleObj *t = tuple_new(3);
        if (!t)
            return oom() == R::Ok;
        tv                 = obj_value(t);
        const int parts[3] = { vi.major, vi.minor, vi.micro };
        for (u32 i = 0; i < 3; i++) {
            Value n = int_from_i64(parts[i]);
            if (n.is_nil())
                return false;
            static_cast<TupleObj *>(tv.v.obj())->items()[i] = n;
        }
    }
    d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_str(d, "EXPAT_VERSION", Str(ver, expat_strlen(ver))) ||
        !mod_put(d, "version_info", tv.v) || !mod_str(d, "native_encoding", "UTF-8") ||
        !mod_int(d, "XML_PARAM_ENTITY_PARSING_NEVER", XML_PARAM_ENTITY_PARSING_NEVER) ||
        !mod_int(d, "XML_PARAM_ENTITY_PARSING_UNLESS_STANDALONE",
                 XML_PARAM_ENTITY_PARSING_UNLESS_STANDALONE) ||
        !mod_int(d, "XML_PARAM_ENTITY_PARSING_ALWAYS", XML_PARAM_ENTITY_PARSING_ALWAYS))
        return false;
    return build_features(static_cast<DictObj *>(rd.v.obj()));
}
