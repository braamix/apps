#include "size.h"

#include "archive.h"
#include "kernel/string.h"
#include "module.h"
#include "wasm.h"

using namespace wasm;

namespace {

// A number as printf's %llo, %llu or %llx writes it; with `alt`, as %#
// writes it: 0 before an octal number, 0x before a hex one, neither for zero.
struct Num {
    char t[32];
    usize n = 0;

    Num(u64 v, u32 radix, bool alt)
    {
        char d[24];
        usize k   = 0;
        bool zero = v == 0;
        do {
            d[k++] = "0123456789abcdef"[v % radix];
            v /= radix;
        } while (v);
        if (alt && !zero && radix == 16) {
            t[n++] = '0';
            t[n++] = 'x';
        } else if (alt && !zero && radix == 8) {
            t[n++] = '0';
        }
        while (k)
            t[n++] = d[--k];
    }

    Str str() const { return Str(t, n); }
};

// How wide llvm-size reckons a number, for the System V columns: as a C
// literal, so zero is 0x0 in hex and 00 in octal.
usize width(u64 v, u32 radix)
{
    usize k = 0;
    do {
        k++;
        v /= radix;
    } while (v);
    return k + (radix == 16 ? 2 : radix == 8 ? 1 : 0);
}

void right(Out &o, Str t, usize w)
{
    for (usize i = t.size(); i < w; i++)
        o.put(' ');
    o.put(t);
}

void number(Out &o, u64 v, usize w, u32 radix, bool alt = true)
{
    right(o, Num(v, radix, alt).str(), w);
}

usize max(usize a, usize b)
{
    return a > b ? a : b;
}

bool is_wasm(Bytes f)
{
    return f.size() >= 4 && f[0] == MAGIC[0] && f[1] == MAGIC[1] && f[2] == MAGIC[2] &&
           f[3] == MAGIC[3];
}

bool is_bitcode(Bytes f)
{
    return f.size() >= 4 && f[0] == 'B' && f[1] == 'C' && f[2] == 0xc0 && f[3] == 0xde;
}

// An object or a shared library puts its sections at address 0; a program
// at their offsets in the file.
bool unplaced(const Vec<Section> &sections)
{
    for (const Section &s : sections)
        if (s.id == SEC_CUSTOM &&
            (s.name == "linking" || s.name == "dylink" || s.name == "dylink.0"))
            return true;
    return false;
}

void sysv(Sizer &s, Str name, Str archive, const Vec<Section> &sections)
{
    Out &o  = s.out;
    u32 r   = s.c.radix;
    bool at = !unplaced(sections);
    o.put(name);
    if (archive.empty())
        o.put("  :\n");
    else
        o.put("   (ex ").put(archive).put("):\n");

    // The widest of each column, and two spaces more; the total is not
    // counted, so a long one runs over.
    usize nw = 7, sw = 4, aw = 4;
    u64 total = 0;
    for (const Section &x : sections) {
        total += x.body.size();
        nw = max(nw, x.name.size());
        sw = max(sw, width(x.body.size(), r));
        aw = max(aw, width(at ? x.start : 0, r));
    }
    nw += 2;
    sw += 2;
    aw += 2;

    o.left("section", nw).put(' ');
    right(o, "size", sw);
    o.put(' ');
    right(o, "addr", aw);
    o.put('\n');
    for (const Section &x : sections) {
        o.left(x.name, nw).put(' ');
        number(o, x.body.size(), sw, r);
        o.put(' ');
        number(o, at ? x.start : 0, aw, r);
        o.put('\n');
    }
    o.left("Total", nw).put(' ');
    number(o, total, sw, r);
    o.put("\n\n\n");
}

// text, data and bss, then their sum in decimal (octal for -o) and hex.
void berkeley_line(Out &o, u64 text, u64 data, u64 bss, u32 r)
{
    u64 total = text + data + bss;
    number(o, text, 7, r);
    o.put('\t');
    number(o, data, 7, r);
    o.put('\t');
    number(o, bss, 7, r);
    o.put('\t');
    number(o, total, 7, r == 8 ? 8 : 10, false);
    o.put('\t');
    number(o, total, 7, 16, false);
    o.put('\t');
}

// Code is text and data is data. Wasm has no bss: memory the data does not
// fill is zero already.
void berkeley(Sizer &s, Str name, Str archive, const Vec<Section> &sections)
{
    Out &o   = s.out;
    u64 text = 0, data = 0;
    for (const Section &x : sections) {
        if (x.id == SEC_CODE)
            text += x.body.size();
        else if (x.id == SEC_DATA)
            data += x.body.size();
    }
    s.text += text;
    s.data += data;
    if (!s.header) {
        o.put("   text\t   data\t    bss\t    ").put(s.c.radix == 8 ? "oct" : "dec");
        o.put("\t    hex\tfilename\n");
        s.header = true;
    }
    berkeley_line(o, text, data, 0, s.c.radix);
    o.put(name);
    if (!archive.empty())
        o.put(" (ex ").put(archive).put(')');
    o.put('\n');
}

// One module; `what` names it in an error.
void module(Sizer &s, Str name, Str archive, Bytes file, Str what, Diag &diag)
{
    Out err;
    if (is_bitcode(file)) {
        err.put(what).put(": LLVM bitcode (from -flto), not a wasm module");
        diag.error(err.str());
        return;
    }
    Vec<Section> sections;
    if (!read_module(what, file, sections, err)) {
        diag.error(err.str());
        return;
    }
    if (s.c.format == SizeFormat::Sysv)
        sysv(s, name, archive, sections);
    else
        berkeley(s, name, archive, sections);
}

} // namespace

void size_file(Sizer &s, Str name, Bytes file, Diag &diag)
{
    if (!is_archive(file)) {
        module(s, name, Str(), file, name, diag);
        return;
    }
    Vec<Member> members;
    Vec<ArchiveSymbol> index;
    Out err;
    if (!read_archive(name, file, members, index, err)) {
        diag.error(err.str());
        return;
    }
    for (const Member &m : members) {
        if (!is_wasm(m.data))
            continue;
        String what;
        if (!what.append(name) || !what.push('(') || !what.append(m.name) || !what.push(')')) {
            diag.error("out of memory");
            return;
        }
        module(s, m.name, name, m.data, what.str(), diag);
    }
}

void size_totals(Sizer &s)
{
    if (s.c.format != SizeFormat::Berkeley || !s.c.totals)
        return;
    berkeley_line(s.out, s.text, s.data, s.bss, s.c.radix);
    s.out.put("(TOTALS)\n");
}
