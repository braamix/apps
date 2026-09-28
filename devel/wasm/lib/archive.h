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

// A member's defined non-local symbols, in the order of its linking
// symbol table. A member that is not a wasm object has none: false,
// and no error, as create_symtab_entry skips what is not ELF.
bool defined_symbols(Bytes file, Vec<Str> &names);
