#include <inttypes.h>

#include "string_map.h"
#include "symtab.h"

#ifndef __braam__ // debug dumps nothing calls; on Braam they would be coroutines

extern StringMap symtab;

//
// Print symbol.
//
void print_symbol(const Symbol *sym)
{
    trace_printf("    %s:", sym->name);
    switch (sym->kind) {
    case SYM_FUNC:
        trace_printf(" function");
        if (sym->u.func.global)
            trace_printf(" global");
        if (sym->u.func.defined)
            trace_printf(" defined");
        trace_printf("\n");
        break;
    case SYM_STATIC:
        trace_printf(" static_var");
        if (sym->u.static_var.global)
            trace_printf(" global");
        switch (sym->u.static_var.init_kind) {
        case INIT_NONE:
            trace_printf("\n");
            break;
        case INIT_TENTATIVE:
            trace_printf(" tentative\n");
            break;
        case INIT_INITIALIZED:
            trace_printf(" initialized\n");
            tac_print_static_init(stdout, sym->u.static_var.init_list, 8);
            break;
        }
        break;
    case SYM_CONST:
        trace_printf(" string\n");
        tac_print_static_init(stdout, sym->u.const_init, 8);
        break;
    case SYM_ENUM:
        trace_printf(" enum %d\n", sym->u.enum_val);
        break;
    case SYM_LOCAL:
        trace_printf(" local\n");
        break;
    }
    print_type(stdout, sym->type, 8);
}

static void symtab_print_callback(const char *key, intptr_t ptr, const void *arg)
{
    (void)key;
    (void)arg;
    print_symbol((Symbol *)ptr);
}

//
// Print all symbols.
//
void symtab_print()
{
    map_iterate(&symtab, symtab_print_callback, NULL);
}
#endif
