//
// WebAssembly IR → assembly, in the syntax of the LLVM assembler (clang
// --target=wasm32 -c), which makes relocatable objects for wasm-ld.
//
#include <math.h>
#include <string.h>

#include "internal.h"
#include "xalloc.h"

const char *wasm_name(const char *name)
{
    // The assembler reads these as float literals wherever an operand may be one (call
    // infinity), and takes no quoted name: another, beyond C's names.
    static const char *const renamed[][2] = {
        { "inf", "inf.vcc" },
        { "nan", "nan.vcc" },
        { "infinity", "infinity.vcc" },
    };
    for (size_t i = 0; i < sizeof(renamed) / sizeof(renamed[0]); i++)
        if (strcmp(name, renamed[i][0]) == 0)
            return renamed[i][1];
    return name;
}

const char *wasm_symbol(const Tac_TopLevel *program, const char *name)
{
    if (strcmp(name, "main") != 0)
        return wasm_name(name);
    // clang's names for main: __main_argc_argv when it takes the arguments, else
    // __original_main, beside a main(argc, argv) that calls it.
    for (const Tac_TopLevel *t = program; t; t = t->next)
        if (t->kind == TAC_TOPLEVEL_FUNCTION && strcmp(t->u.function.name, "main") == 0)
            return t->u.function.params ? "__main_argc_argv" : "__original_main";
    return name;
}

// The signature, as `.functype` and `call_indirect` spell it: (i32, i64) -> (f64).
static void print_sig(FILE *out, const Wasm_ValType *params, int nparams, Wasm_ValType result)
{
    fputc('(', out);
    for (int i = 0; i < nparams; i++)
        fprintf(out, "%s%s", i ? ", " : "", wasm_valtype_name(params[i]));
    fprintf(out, ") -> (%s)", wasm_valtype_name(result));
}

char *wasm_sig_string(const Wasm_Sig *sig)
{
    char buf[64 * 5 + 32];
    int n = snprintf(buf, sizeof(buf), "(");
    for (int i = 0; i < sig->nparams; i++)
        n += snprintf(buf + n, sizeof(buf) - n, "%s%s", i ? ", " : "",
                      wasm_valtype_name(sig->params[i]));
    snprintf(buf + n, sizeof(buf) - n, ") -> (%s)", wasm_valtype_name(sig->result));
    return xstrdup(buf);
}

static void print_functype(FILE *out, const char *name, const Wasm_Sig *sig)
{
    fprintf(out, "\t.functype\t%s ", name);
    print_sig(out, sig->params, sig->nparams, sig->result);
    fputc('\n', out);
}

// The features clang's objects record with Braam's flags (-mreference-types
// -mbulk-memory -msign-ext -mmutable-globals -mnontrapping-fptoint on its default CPU),
// so that wasm-ld links ours with theirs.  The assembler adds no such section itself.
static const char *const features[] = {
    "bulk-memory",     "bulk-memory-opt",     "call-indirect-overlong", "multivalue",
    "mutable-globals", "nontrapping-fptoint", "reference-types",        "sign-ext",
};

void wasm_emit_unit_begin(FILE *out, const Tac_TopLevel *program)
{
    // The shadow stack's pointer, which wasm-ld defines; declared whether used or not
    // (unused, it costs nothing).
    fprintf(out, "\t.globaltype\t__stack_pointer, i32\n");
    // The table of functions called through pointers, which wasm-ld makes.
    fprintf(out, "\t.tabletype\t__indirect_function_table, funcref\n");
    for (const Tac_TopLevel *t = program; t; t = t->next) {
        Wasm_Sig sig;
        if (t->kind == TAC_TOPLEVEL_FUNCTION && t->u.function.type) {
            wasm_signature(t->u.function.type, &sig);
            print_functype(out, wasm_symbol(program, t->u.function.name), &sig);
            if (strcmp(t->u.function.name, "main") == 0 && !t->u.function.params)
                fprintf(out, "\t.functype\tmain (i32, i32) -> (i32)\n");
        } else if (t->kind == TAC_TOPLEVEL_EXTERN && t->u.extern_.type &&
                   t->u.extern_.type->kind == TAC_TYPE_FUN_TYPE &&
                   strcmp(t->u.extern_.name, "__va_start") != 0 && // expanded in place
                   !wasm_stack_builtin(t->u.extern_.name)) {
            wasm_signature(t->u.extern_.type, &sig);
            print_functype(out, wasm_symbol(program, t->u.extern_.name), &sig);
        }
    }
    int n = (int)(sizeof(features) / sizeof(features[0]));
    fprintf(out, "\t.section\t.custom_section.target_features,\"\",@\n");
    fprintf(out, "\t.int8\t%d\n", n);
    for (int i = 0; i < n; i++)
        fprintf(out, "\t.int8\t43\n\t.int8\t%d\n\t.ascii\t\"%s\"\n", (int)strlen(features[i]),
                features[i]);
}

// A float constant: hex, which is exact, or infinity, or nan (the default quiet NaN) or
// nan:0x<significand> (another), as the assembler spells them.  `mbits` is the
// significand's width: 52 or 23.
static void print_float(FILE *out, double v, uint64_t significand, int mbits)
{
    if (isnan(v)) {
        fputs(signbit(v) ? "-nan" : "nan", out);
        if (significand != 1ull << (mbits - 1))
            fprintf(out, ":0x%llx", (unsigned long long)significand);
    } else if (isinf(v)) {
        fprintf(out, "%sinfinity", v < 0 ? "-" : "");
    } else {
        fprintf(out, "%a", v);
    }
}

static int log2_of(int n)
{
    int l = 0;
    while ((1 << l) < n)
        l++;
    return l;
}

static void emit_instr(FILE *out, const Wasm_Instr *in)
{
    fprintf(out, "\t%s", wasm_op_name(in->op));
    switch (wasm_op_form(in->op)) {
    case WASM_FORM_NONE:
        break;
    case WASM_FORM_LOCAL:
    case WASM_FORM_DEPTH:
        fprintf(out, "\t%lld", (long long)in->imm);
        break;
    case WASM_FORM_GLOBAL:
    case WASM_FORM_CALL:
        fprintf(out, "\t%s", in->sym);
        break;
    case WASM_FORM_I32:
    case WASM_FORM_I64:
        if (in->sym && in->imm)
            fprintf(out, "\t%s%+lld", in->sym, (long long)in->imm);
        else if (in->sym)
            fprintf(out, "\t%s", in->sym);
        else
            fprintf(out, "\t%lld", (long long)in->imm);
        break;
    case WASM_FORM_F32: {
        uint32_t bits = (uint32_t)in->imm;
        float f;
        memcpy(&f, &bits, sizeof(f));
        fputc('\t', out);
        print_float(out, f, bits & 0x7fffff, 23);
        break;
    }
    case WASM_FORM_F64: {
        uint64_t bits = (uint64_t)in->imm;
        double d;
        memcpy(&d, &bits, sizeof(d));
        fputc('\t', out);
        print_float(out, d, bits & 0xfffffffffffffull, 52);
        break;
    }
    case WASM_FORM_MEM:
        if (in->sym && in->imm)
            fprintf(out, "\t%s%+lld", in->sym, (long long)in->imm);
        else if (in->sym)
            fprintf(out, "\t%s", in->sym);
        else
            fprintf(out, "\t%lld", (long long)in->imm);
        if (in->align)
            fprintf(out, ":p2align=%d", log2_of(in->align));
        break;
    case WASM_FORM_TABLE:
        fputs("\t{", out);
        for (int i = 0; i < in->ntable; i++)
            fprintf(out, "%s%d", i ? ", " : "", in->table[i]);
        fputc('}', out);
        break;
    case WASM_FORM_INDIRECT:
        fprintf(out, "\t__indirect_function_table, %s", in->sym);
        break;
    case WASM_FORM_BLOCK:
        if (in->bt != WASM_VOID)
            fprintf(out, "\t%s", wasm_valtype_name(in->bt));
        break;
    case WASM_FORM_MEMORY:
        fputs("\t0", out);
        break;
    case WASM_FORM_MEMORY2:
        fputs("\t0, 0", out);
        break;
    }
    fputc('\n', out);
}

void wasm_emit_func(FILE *out, const Wasm_Func *fn)
{
    fprintf(out, "\t.section\t.text.%s,\"\",@\n", fn->name);
    if (fn->global)
        fprintf(out, "\t.globl\t%s\n", fn->name);
    fprintf(out, "\t.type\t%s,@function\n", fn->name);
    fprintf(out, "%s:\n", fn->name);
    fprintf(out, "\t.functype\t%s ", fn->name);
    print_sig(out, fn->params, fn->nparams, fn->result);
    fputc('\n', out);
    if (fn->nlocals) {
        fputs("\t.local\t", out);
        for (int i = 0; i < fn->nlocals; i++)
            fprintf(out, "%s%s", i ? ", " : "", wasm_valtype_name(fn->locals[i]));
        fputc('\n', out);
    }
    for (const Wasm_Instr *in = fn->first; in; in = in->next)
        emit_instr(out, in);
    fputs("\tend_function\n", out);
}

void wasm_emit_main_aliases(FILE *out)
{
    fputs(
        "\t.section\t.text.main,\"\",@\n"
        "\t.globl\tmain\n"
        "\t.type\tmain,@function\n"
        "main:\n"
        "\t.functype\tmain (i32, i32) -> (i32)\n"
        "\tcall\t__original_main\n"
        "\tend_function\n"
        "\t.globl\t__main_void\n"
        "\t.type\t__main_void,@function\n"
        "__main_void = __original_main\n",
        out);
}
