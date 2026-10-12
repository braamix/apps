//
// TAC → WebAssembly IR → assembly, one toplevel at a time.
//
#include "codegen.h"

#include <string.h>

#include "internal.h"

// Whether control cannot run off the end of the function's code.
static bool ends_in_transfer(const Wasm_Func *fn)
{
    if (!fn->last)
        return false;
    switch (fn->last->op) {
    case WASM_RETURN:
    case WASM_UNREACHABLE:
    case WASM_BR:
    case WASM_BR_TABLE:
        return true;
    default:
        return false;
    }
}

static void gen_function(const Tac_TopLevel *program, const Tac_TopLevel *tl, FILE *out)
{
    Gen g;
    gen_init(&g, program, tl);
    gen_prologue(&g);
    gen_body(&g);
    // Where control can run off the end, a result's absence is made valid.
    if (g.fn->result != WASM_VOID && !ends_in_transfer(g.fn))
        wasm_append(g.fn, WASM_UNREACHABLE);
    wasm_optimize(g.fn);
    for (int i = 0; i < g.nhelpers; i++)
        fprintf(out, "\t.functype\t%s %s\n", g.helpers[i].name, g.helpers[i].sig);
    wasm_emit_func(out, g.fn);
    gen_done(&g);
    for (const Tac_StaticLocal *s = tl->u.function.static_locals; s; s = s->next)
        emit_static_variable(out, s->name, false, s->type, s->init_list, false, s->alignment);
    if (strcmp(tl->u.function.name, "main") == 0 && !tl->u.function.params)
        wasm_emit_main_aliases(out);
}

// The strictest _Alignas among the declarations of static variable `name`.
static int declared_alignment(const Tac_TopLevel *program, const char *name)
{
    int a = 0;
    for (const Tac_TopLevel *t = program; t; t = t->next)
        if (t->kind == TAC_TOPLEVEL_STATIC_VARIABLE &&
            strcmp(t->u.static_variable.name, name) == 0 && t->u.static_variable.alignment > a)
            a = t->u.static_variable.alignment;
    return a;
}

void wasm_codegen(const Tac_TopLevel *program, const Tac_TopLevel *tl, FILE *out)
{
    if (tl == program)
        wasm_emit_unit_begin(out, program);
    switch (tl->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        gen_function(program, tl, out);
        break;
    case TAC_TOPLEVEL_EXTERN:
        break; // declared by wasm_emit_unit_begin; the linker resolves it
    case TAC_TOPLEVEL_STATIC_VARIABLE:
        if (tac_static_superseded(program, tl))
            break;
        emit_static_variable(out, tl->u.static_variable.name, tl->u.static_variable.global,
                             tl->u.static_variable.type, tl->u.static_variable.init_list, false,
                             declared_alignment(program, tl->u.static_variable.name));
        break;
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        emit_static_variable(out, tl->u.static_constant.name, false, tl->u.static_constant.type,
                             tl->u.static_constant.init, true, 0);
        break;
    }
}
