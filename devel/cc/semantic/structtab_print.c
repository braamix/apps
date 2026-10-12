#include <inttypes.h>

#include "string_map.h"
#include "structtab.h"

#ifndef __braam__ // debug dumps nothing calls; on Braam they would be coroutines

extern StringMap structtab;

//
// Print struct/union.
//
void print_struct(const StructDef *def)
{
    trace_printf("    struct %s: size %d bytes, alignment %d\n", def->tag, def->size,
                 def->alignment);
    for (const FieldDef *field = def->members; field; field = field->next) {
        trace_printf("        field %s: offset %d\n", field->name, field->offset);
        print_type(stdout, field->type, 12);
    }
}

static void structtab_print_callback(const char *key, intptr_t ptr, const void *arg)
{
    (void)key;
    (void)arg;
    print_struct((const StructDef *)ptr);
}

//
// Print all structs.
//
void structtab_print()
{
    map_iterate(&structtab, structtab_print_callback, NULL);
}
#endif
