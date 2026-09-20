"""pyexpat and the three libraries over it: the events in order, the errors,
and what ElementTree and minidom make of the same documents."""

import io
import pyexpat
from pyexpat import errors, model
import xml.etree.ElementTree as ET
from xml.dom import minidom

print("--- module")
print(pyexpat.EXPAT_VERSION, pyexpat.version_info, pyexpat.native_encoding)
print(sorted(n for n, _ in pyexpat.features))
print(pyexpat.ErrorString(1), "|", pyexpat.ErrorString(3))
print(errors.XML_ERROR_SYNTAX, "|", errors.XML_ERROR_NO_ELEMENTS)
print(errors.codes["not well-formed (invalid token)"], errors.messages[4])
print(len(errors.codes), len(errors.messages))
print(model.XML_CTYPE_EMPTY, model.XML_CTYPE_SEQ, model.XML_CQUANT_PLUS)
print(pyexpat.XML_PARAM_ENTITY_PARSING_NEVER,
      pyexpat.XML_PARAM_ENTITY_PARSING_UNLESS_STANDALONE,
      pyexpat.XML_PARAM_ENTITY_PARSING_ALWAYS)
print(pyexpat.error is pyexpat.ExpatError, type(pyexpat.ParserCreate()) is pyexpat.XMLParserType)

HANDLERS = [
    "StartElementHandler", "EndElementHandler", "ProcessingInstructionHandler",
    "CharacterDataHandler", "UnparsedEntityDeclHandler", "NotationDeclHandler",
    "StartNamespaceDeclHandler", "EndNamespaceDeclHandler", "CommentHandler",
    "StartCdataSectionHandler", "EndCdataSectionHandler", "DefaultHandler",
    "DefaultHandlerExpand", "NotStandaloneHandler", "ExternalEntityRefHandler",
    "StartDoctypeDeclHandler", "EndDoctypeDeclHandler", "EntityDeclHandler",
    "XmlDeclHandler", "ElementDeclHandler", "AttlistDeclHandler",
    "SkippedEntityHandler",
]


def recorder(p, log, skip=()):
    """Every handler p has, recording its name and arguments. The two that
    steer the parse by what they return are left out: None is not an answer."""
    for name in HANDLERS:
        if name in skip or name in ("NotStandaloneHandler",
                                    "ExternalEntityRefHandler"):
            continue

        def make(name=name):
            def h(*args):
                log.append((name,) + args)
            return h
        setattr(p, name, make())


def show(log):
    for e in log:
        print(e)


DOC = (b"<?xml version='1.0' encoding='UTF-8' standalone='yes'?>\n"
       b"<!DOCTYPE root [\n"
       b"<!ELEMENT root (a|b)*>\n"
       b"<!ELEMENT a (#PCDATA)>\n"
       b"<!ELEMENT b EMPTY>\n"
       b"<!ATTLIST a id ID #IMPLIED x CDATA 'd' y (p|q) #REQUIRED>\n"
       b"<!ENTITY e 'expanded'>\n"
       b"<!NOTATION n SYSTEM 'n.dtd'>\n"
       b"<!ENTITY pic SYSTEM 'up.gif' NDATA n>\n"
       b"]>\n"
       b"<root><?target data?><!--a comment-->"
       b"<a y='p' id='i1'>text &e; more</a>"
       b"<b/><![CDATA[raw <>&]]></root>\n")

print("--- every handler, buffered")
p = pyexpat.ParserCreate()
p.buffer_text = True
log = []
recorder(p, log)
print(p.Parse(DOC, True))
show(log)

print("--- unbuffered, ordered attributes")
p = pyexpat.ParserCreate()
p.ordered_attributes = True
log = []
recorder(p, log, skip=("DefaultHandler", "DefaultHandlerExpand"))
p.Parse(DOC, True)
show(log)

print("--- namespaces")
NSDOC = (b"<a xmlns='u1' xmlns:p='u2' p:k='v'>"
         b"<p:b/><c xmlns=''/></a>")
for sep, triplets in ((None, False), ("|", False), ("|", True)):
    p = pyexpat.ParserCreate(namespace_separator=sep)
    p.namespace_prefixes = triplets
    log = []
    recorder(p, log, skip=("DefaultHandler", "DefaultHandlerExpand"))
    p.Parse(NSDOC, True)
    print("sep", sep, "triplets", triplets)
    show(log)

print("--- chunks and positions")
p = pyexpat.ParserCreate()
p.buffer_text = True
log = []
p.StartElementHandler = lambda n, a: log.append(
    ("start", n, p.CurrentLineNumber, p.CurrentColumnNumber, p.CurrentByteIndex))
p.EndElementHandler = lambda n: log.append(("end", n, p.CurrentLineNumber))
p.CharacterDataHandler = lambda d: log.append(("chars", d))
text = b"<r>\n  <a>one</a>\n  <b>two</b>\n</r>\n"
for i in range(0, len(text), 5):
    p.Parse(text[i:i + 5], False)
print(p.Parse(b"", True))
show(log)

print("--- buffer_size")
p = pyexpat.ParserCreate()
p.buffer_text = True
p.buffer_size = 8
log = []
p.CharacterDataHandler = lambda d: log.append(d)
p.Parse(b"<r>" + b"x" * 30 + b"</r>", True)
print(log, p.buffer_size, p.buffer_used)

print("--- specified attributes")
for flag in (False, True):
    p = pyexpat.ParserCreate()
    p.specified_attributes = flag
    log = []
    p.StartElementHandler = lambda n, a: log.append((n, a))
    p.Parse(b"<!DOCTYPE r [<!ATTLIST r x CDATA 'dflt' y CDATA #IMPLIED>]>"
            b"<r y='given'/>", True)
    print(flag, log, p.GetSpecifiedAttributeCount())

print("--- input context")
p = pyexpat.ParserCreate()
ctx = []
p.StartElementHandler = lambda n, a: ctx.append((n, p.GetInputContext()))
p.Parse(b"<outer><inner/></outer>", True)
print(ctx)

print("--- intern")
shared = {}
p = pyexpat.ParserCreate(intern=shared)
names = []
p.StartElementHandler = lambda n, a: names.append(n)
p.Parse(b"<r><r/><r/></r>", True)
print(names[0] is names[1] is names[2], sorted(shared), p.intern is shared)

print("--- ParseFile")
p = pyexpat.ParserCreate()
log = []
recorder(p, log, skip=("DefaultHandler", "DefaultHandlerExpand"))
print(p.ParseFile(io.BytesIO(b"<r><a>x</a><b/></r>")))
show(log)

print("--- errors")
for bad in (b"<a>", b"<a></b>", b"<a x=1/>", b"<a>&undef;</a>", b"", b"<a>\x80</a>",
            b"<a><![CDATA[x</a>", b"<a b='1' b='2'/>", b"<?xml version='2.0'?><a/>"):
    p = pyexpat.ParserCreate()
    try:
        p.Parse(bad, True)
        print(bad, "-> ok")
    except pyexpat.ExpatError as e:
        print(bad, "->", e, "|", e.code, e.lineno, e.offset,
              p.ErrorCode, p.ErrorLineNumber, p.ErrorColumnNumber, p.ErrorByteIndex)

print("--- handler raises")
p = pyexpat.ParserCreate()
seen = []


def boom(name, atts):
    seen.append(name)
    raise ValueError("from the handler: " + name)


p.StartElementHandler = boom
try:
    p.Parse(b"<a><b/></a>", True)
except ValueError as e:
    print("raised", e, seen)

print("--- handler removed mid-parse")
p = pyexpat.ParserCreate()
seen = []


def once(name, atts):
    seen.append(name)
    p.StartElementHandler = None


p.StartElementHandler = once
p.Parse(b"<a><b/><c/></a>", True)
print(seen)

print("--- external entities")
DTD = (b"<!DOCTYPE r SYSTEM 'outer.dtd' [<!ENTITY % pe SYSTEM 'inner.dtd'>%pe;]>"
       b"<r>&ext;</r>")
for answer in (1, 0):
    p = pyexpat.ParserCreate()
    p.SetParamEntityParsing(pyexpat.XML_PARAM_ENTITY_PARSING_ALWAYS)
    log = []
    recorder(p, log, skip=("DefaultHandler", "DefaultHandlerExpand"))

    def ext(context, base, sysid, pubid, answer=answer, p=p):
        log.append(("ExternalEntityRefHandler", context, base, sysid, pubid))
        if not answer:
            return 0
        sub = p.ExternalEntityParserCreate(context)
        body = {b"outer.dtd": b"<!ENTITY ext 'from the outer subset'>",
                b"inner.dtd": b"<!ENTITY ent2 'from a parameter entity'>"}
        return sub.Parse(body.get(sysid.encode(), b""), True)

    p.ExternalEntityRefHandler = ext
    try:
        print("answer", answer, "->", p.Parse(DTD, True))
    except pyexpat.ExpatError as e:
        print("answer", answer, "->", e, e.code)
    show(log)

print("--- NotStandaloneHandler")
for answer in (1, 0):
    p = pyexpat.ParserCreate()
    p.SetParamEntityParsing(pyexpat.XML_PARAM_ENTITY_PARSING_ALWAYS)
    log = []
    p.NotStandaloneHandler = lambda answer=answer: (log.append("asked"), answer)[1]
    p.ExternalEntityRefHandler = lambda *a: 1
    try:
        print("answer", answer, "->",
              p.Parse(b"<?xml version='1.0' standalone='no'?>"
                      b"<!DOCTYPE r SYSTEM 'r.dtd'><r/>", True))
    except pyexpat.ExpatError as e:
        print("answer", answer, "->", e, e.code)
    print(log)

print("--- SetBase and UseForeignDTD")
p = pyexpat.ParserCreate()
p.SetBase("/base/here")
print(p.GetBase())
p.UseForeignDTD(True)
log = []
p.ExternalEntityRefHandler = lambda *a: (log.append(a), 1)[1]
p.StartElementHandler = lambda n, a: log.append(("start", n))
p.Parse(b"<r/>", True)
print(log)

print("--- reparse deferral")
p = pyexpat.ParserCreate()
print(p.GetReparseDeferralEnabled())
p.SetReparseDeferralEnabled(False)
print(p.GetReparseDeferralEnabled())
log = []
p.StartElementHandler = lambda n, a: log.append(n)
p.Parse(b"<r", False)
p.Parse(b"><a/></r>", True)
print(log)

print("--- bad arguments")
for call, args in ((pyexpat.ParserCreate, ("utf-8", "xy")),
                   (pyexpat.ParserCreate, (1,)),
                   (pyexpat.ErrorString, ("x",))):
    try:
        call(*args)
        print("no error for", args)
    except Exception as e:
        print(type(e).__name__, e)
p = pyexpat.ParserCreate()
try:
    p.Parse(3, True)
except TypeError as e:
    print("TypeError", e)
try:
    p.buffer_size = 0
except ValueError as e:
    print("ValueError", e)
try:
    p.NoSuchHandler = None
except AttributeError:
    print("AttributeError for an unknown attribute")

print("--- ElementTree")
root = ET.fromstring(DOC.decode())
print(root.tag, sorted(root.attrib.items()))
for e in root.iter():
    print(e.tag, repr(e.text), sorted(e.attrib.items()))
print(ET.tostring(root))
print(ET.tostring(ET.fromstring("<a xmlns='u'><b/></a>")))

print("--- iterparse")
for event, elem in ET.iterparse(io.BytesIO(b"<r><a>x</a><b/></r>"),
                                events=("start", "end")):
    print(event, elem.tag, repr(elem.text))

print("--- XMLPullParser")
pull = ET.XMLPullParser(events=("start", "end"))
for chunk in (b"<r><a", b">t</a>", b"<b/></r>"):
    pull.feed(chunk)
    for event, elem in pull.read_events():
        print(event, elem.tag)
pull.close()

print("--- ElementTree errors")
try:
    ET.fromstring("<a></b>")
except ET.ParseError as e:
    print(type(e).__name__, e, e.code, e.position)

print("--- minidom")
d = minidom.parseString(DOC)
print(d.documentElement.tagName, d.documentElement.getAttribute("x"))
print(d.toxml())
print(d.toprettyxml(indent="  "))
print([n.nodeName for n in d.documentElement.childNodes])
print(d.getElementsByTagName("a")[0].firstChild.data)
d2 = minidom.parseString("<a><b/></a>")
print(d2.documentElement.firstChild.tagName, d2.documentElement.hasChildNodes())
d2.unlink()
