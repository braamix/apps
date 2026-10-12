//
// Static data: variables, constants and block-scope statics, each in a section of its
// own (as clang puts them) so that wasm-ld drops what nothing reaches.  Little-endian,
// so a long double is its binary128 bits, the low half first.
//
#include <string.h>

#include "float128.h"
#include "internal.h"

static void emit_ascii(FILE *out, const char *s, size_t len)
{
    fputs("\t.ascii\t\"", out);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\')
            fprintf(out, "\\%c", c);
        else if (c < ' ' || c > '~')
            fprintf(out, "\\%03o", c);
        else
            fputc(c, out);
    }
    fputs("\"\n", out);
}

// Emit one initializer item; returns its size in bytes.
static int emit_init(FILE *out, const Tac_StaticInit *it)
{
    switch (it->kind) {
    case TAC_STATIC_INIT_I8:
        fprintf(out, "\t.int8\t%d\n", it->u.char_val);
        return 1;
    case TAC_STATIC_INIT_U8:
        fprintf(out, "\t.int8\t%u\n", it->u.uchar_val);
        return 1;
    case TAC_STATIC_INIT_I16:
        fprintf(out, "\t.int16\t%d\n", it->u.short_val);
        return 2;
    case TAC_STATIC_INIT_U16:
        fprintf(out, "\t.int16\t%u\n", it->u.ushort_val);
        return 2;
    case TAC_STATIC_INIT_I32:
        fprintf(out, "\t.int32\t%d\n", it->u.int_val);
        return 4;
    case TAC_STATIC_INIT_U32:
        fprintf(out, "\t.int32\t%u\n", it->u.uint_val);
        return 4;
    case TAC_STATIC_INIT_I64:
        fprintf(out, "\t.int64\t%lld\n", (long long)it->u.long_val);
        return 8;
    case TAC_STATIC_INIT_U64:
        fprintf(out, "\t.int64\t%llu\n", (unsigned long long)it->u.ulong_val);
        return 8;
    case TAC_STATIC_INIT_FLOAT: {
        float f = (float)it->u.float_val;
        uint32_t bits;
        memcpy(&bits, &f, sizeof(bits));
        fprintf(out, "\t.int32\t0x%08x\n", bits);
        return 4;
    }
    case TAC_STATIC_INIT_DOUBLE: {
        uint64_t bits;
        memcpy(&bits, &it->u.double_val, sizeof(bits));
        fprintf(out, "\t.int64\t0x%016llx\n", (unsigned long long)bits);
        return 8;
    }
    case TAC_STATIC_INIT_LONG_DOUBLE:
        fprintf(out, "\t.int64\t0x%016llx\n\t.int64\t0x%016llx\n",
                (unsigned long long)it->u.long_double_val.lo,
                (unsigned long long)it->u.long_double_val.hi);
        return 16;
    case TAC_STATIC_INIT_ZERO:
        if (it->u.zero_bytes > 0)
            fprintf(out, "\t.skip\t%d\n", it->u.zero_bytes);
        return it->u.zero_bytes;
    case TAC_STATIC_INIT_STRING: {
        size_t len = it->u.string.val ? it->u.string.len : 0;
        if (len)
            emit_ascii(out, it->u.string.val, len);
        if (it->u.string.null_terminated) {
            fputs("\t.int8\t0\n", out);
            len++;
        }
        return (int)len;
    }
    case TAC_STATIC_INIT_POINTER:
    case TAC_STATIC_INIT_FAT_POINTER:
        if (it->u.pointer.byte_offset)
            fprintf(out, "\t.int32\t%s%+d\n", wasm_name(it->u.pointer.name),
                    it->u.pointer.byte_offset);
        else
            fprintf(out, "\t.int32\t%s\n", wasm_name(it->u.pointer.name));
        return 4;
    }
    return 0;
}

static bool all_zero(const Tac_StaticInit *init)
{
    for (const Tac_StaticInit *it = init; it; it = it->next)
        if (it->kind != TAC_STATIC_INIT_ZERO)
            return false;
    return true;
}

void emit_static_variable(FILE *out, const char *name, bool global, const Tac_Type *type,
                          const Tac_StaticInit *init, bool readonly, int alignment)
{
    name      = wasm_name(name);
    int size  = wasm_type_size(type);
    int align = wasm_type_align(type) > alignment ? wasm_type_align(type) : alignment;
    int log2  = 0;
    while ((1 << log2) < align)
        log2++;
    bool bss = all_zero(init);
    fprintf(out, "\t.type\t%s,@object\n", name);
    fprintf(out, "\t.section\t.%s.%s,\"\",@\n", readonly ? "rodata" : bss ? "bss" : "data", name);
    if (global)
        fprintf(out, "\t.globl\t%s\n", name);
    if (log2)
        fprintf(out, "\t.p2align\t%d, 0x0\n", log2);
    fprintf(out, "%s:\n", name);
    int n = 0;
    if (!bss)
        for (const Tac_StaticInit *it = init; it; it = it->next)
            n += emit_init(out, it);
    if (n < size)
        fprintf(out, "\t.skip\t%d\n", size - n);
    else if (size == 0 && n == 0)
        fprintf(out, "\t.skip\t1\n"); // an empty object still gets an address
    fprintf(out, "\t.size\t%s, %d\n", name, size > n ? size : n);
}
