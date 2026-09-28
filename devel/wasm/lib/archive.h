// Archives: the GNU and BSD ar formats. Every Str and Bytes is a view into
// the archive's own bytes.
#pragma once

#include "emit.h"
#include "kernel/span.h"
#include "kernel/str.h"
#include "kernel/vec.h"
#include "out.h"

// An archive member: its name, its bytes inside the archive's, and the
// fields of its header. `size` is the header's, which counts a BSD name.
struct Member {
    Str name;
    Bytes data;
    u32 file_off;
    u32 mode;
    u32 uid;
    u32 gid;
    u64 mtime;
    u32 size;
};

// An entry of the archive's symbol table.
struct ArchiveSymbol {
    Str name;
    u32 member; // index into `members`
};

bool is_archive(Bytes file);

// Lists the members of an archive, skipping its symbol and name tables, and
// the entries of its GNU symbol table, `/` or `/SYM64/`, when it has one.
bool read_archive(Str name, Bytes file, Vec<Member> &members, Vec<ArchiveSymbol> &index, Out &err);

// A member's 60-byte header, GNU style: `name` is already the field,
// "name/" or "/<offset into //>", at most 16 bytes. A number too wide for
// its field is written as the widest that fits, as libarchive writes it.
void emit_ar_header(Emit &e, Str name, u64 mtime, u32 uid, u32 gid, u32 mode, u64 size);

// A member's symbols, as llvm-ar indexes them: an object's defined non-local
// symbols, in linking order; a linked module's, as llvm reads its name or
// export section. False, and no error, for what is not a wasm module.
bool defined_symbols(Bytes file, Vec<Str> &names);

// GNU or BSD, and whether a symbol table leads it.
struct ArchiveLayout {
    bool gnu;
    bool symtab;
};
ArchiveLayout archive_layout(Bytes file);

// A member to write.
struct ArchiveEntry {
    Str name;
    Bytes data;
};

// A GNU archive, as llvm writes it deterministically: dates, owners and
// groups 0, modes 644. With `symtab`, a `/` of defined_symbols leads it,
// eight zero bytes if there are none.
void write_archive(Span<const ArchiveEntry> members, bool symtab, Emit &e);
