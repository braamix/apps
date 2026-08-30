// The module table, replacing upstream's dlopen.
//
// Citrus loads a conversion module by building a shared-object name and asking
// the dynamic loader for it, then dlsym'ing "_citrus_<name>_<iface>_getops".
// There is no dynamic loader here, so the modules are linked in and the table
// below is the lookup. Upstream's own fuzzing build does the same thing with
// RTLD_SELF, so the shape is not an invention.
//
// A namespace-scope global must be trivially destructible, which a table of
// pointers and function pointers is; it lands in .rodata. Naming every getops
// here is also what keeps --gc-sections from dropping the modules.

#include "citrus_module.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "citrus_namespace.h"

struct _citrus_iconv_ops;
struct _citrus_stdenc_ops;
struct _citrus_mapper_ops;

extern "C" {

// The two iconv drivers.
int _citrus_iconv_std_iconv_getops(struct _citrus_iconv_ops *);
int _citrus_iconv_none_iconv_getops(struct _citrus_iconv_ops *);

// The stdenc modules. "NONE" is not among them: citrus_stdenc.c reaches
// citrus_none.cpp directly, without a module at all.
int _citrus_UTF8_stdenc_getops(struct _citrus_stdenc_ops *);
int _citrus_UTF8MAC_stdenc_getops(struct _citrus_stdenc_ops *);
int _citrus_UTF7_stdenc_getops(struct _citrus_stdenc_ops *);
int _citrus_UTF1632_stdenc_getops(struct _citrus_stdenc_ops *);
int _citrus_ISO2022_stdenc_getops(struct _citrus_stdenc_ops *);
int _citrus_HZ_stdenc_getops(struct _citrus_stdenc_ops *);
int _citrus_VIQR_stdenc_getops(struct _citrus_stdenc_ops *);
int _citrus_MSKanji_stdenc_getops(struct _citrus_stdenc_ops *);
int _citrus_ZW_stdenc_getops(struct _citrus_stdenc_ops *);
int _citrus_BIG5_stdenc_getops(struct _citrus_stdenc_ops *);
int _citrus_UES_stdenc_getops(struct _citrus_stdenc_ops *);
int _citrus_GBK2K_stdenc_getops(struct _citrus_stdenc_ops *);
int _citrus_DECHanyu_stdenc_getops(struct _citrus_stdenc_ops *);
int _citrus_EUC_stdenc_getops(struct _citrus_stdenc_ops *);
int _citrus_EUCTW_stdenc_getops(struct _citrus_stdenc_ops *);
int _citrus_JOHAB_stdenc_getops(struct _citrus_stdenc_ops *);
int _citrus_DECKanji_stdenc_getops(struct _citrus_stdenc_ops *);

// The mappers. mapper_parallel is a second entry point in mapper_serial's
// source, as it is upstream — two targets, one file.
int _citrus_mapper_std_mapper_getops(struct _citrus_mapper_ops *);
int _citrus_mapper_serial_mapper_getops(struct _citrus_mapper_ops *);
int _citrus_mapper_parallel_mapper_getops(struct _citrus_mapper_ops *);
int _citrus_mapper_zone_mapper_getops(struct _citrus_mapper_ops *);
int _citrus_mapper_646_mapper_getops(struct _citrus_mapper_ops *);
int _citrus_mapper_none_mapper_getops(struct _citrus_mapper_ops *);
}

namespace {

// The three getops signatures differ only in their argument, and the call
// sites cast back to the right one exactly as they cast dlsym's void *. A
// generic function pointer is what the table can hold in the meantime.
typedef void (*GetOps)();

struct ModuleEntry {
    const char *name;
    const char *iface;
    GetOps getops;
};

// The names are the ones esdb's ENCODING fields and mapper.dir's second column
// hold, which is what citrus passes to _citrus_load_module. const rather than
// constexpr because a cast between function pointer types is not a constant
// expression; the initialiser is still a link-time one, so there is no dynamic
// initialisation and nothing to destroy.
const ModuleEntry MODULES[] = {
    { "iconv_std", "iconv", (GetOps)_citrus_iconv_std_iconv_getops },
    { "iconv_none", "iconv", (GetOps)_citrus_iconv_none_iconv_getops },

    { "UTF8", "stdenc", (GetOps)_citrus_UTF8_stdenc_getops },
    { "UTF8MAC", "stdenc", (GetOps)_citrus_UTF8MAC_stdenc_getops },
    { "UTF7", "stdenc", (GetOps)_citrus_UTF7_stdenc_getops },
    { "UTF1632", "stdenc", (GetOps)_citrus_UTF1632_stdenc_getops },
    { "ISO2022", "stdenc", (GetOps)_citrus_ISO2022_stdenc_getops },
    { "HZ", "stdenc", (GetOps)_citrus_HZ_stdenc_getops },
    { "VIQR", "stdenc", (GetOps)_citrus_VIQR_stdenc_getops },
    { "MSKanji", "stdenc", (GetOps)_citrus_MSKanji_stdenc_getops },
    { "ZW", "stdenc", (GetOps)_citrus_ZW_stdenc_getops },
    { "BIG5", "stdenc", (GetOps)_citrus_BIG5_stdenc_getops },
    { "UES", "stdenc", (GetOps)_citrus_UES_stdenc_getops },
    { "GBK2K", "stdenc", (GetOps)_citrus_GBK2K_stdenc_getops },
    { "DECHanyu", "stdenc", (GetOps)_citrus_DECHanyu_stdenc_getops },
    { "EUC", "stdenc", (GetOps)_citrus_EUC_stdenc_getops },
    { "EUCTW", "stdenc", (GetOps)_citrus_EUCTW_stdenc_getops },
    { "JOHAB", "stdenc", (GetOps)_citrus_JOHAB_stdenc_getops },
    { "DECKanji", "stdenc", (GetOps)_citrus_DECKanji_stdenc_getops },

    { "mapper_std", "mapper", (GetOps)_citrus_mapper_std_mapper_getops },
    { "mapper_serial", "mapper", (GetOps)_citrus_mapper_serial_mapper_getops },
    { "mapper_parallel", "mapper", (GetOps)_citrus_mapper_parallel_mapper_getops },
    { "mapper_zone", "mapper", (GetOps)_citrus_mapper_zone_mapper_getops },
    { "mapper_646", "mapper", (GetOps)_citrus_mapper_646_mapper_getops },
    { "mapper_none", "mapper", (GetOps)_citrus_mapper_none_mapper_getops },
};

constexpr usize MODULE_N = sizeof(MODULES) / sizeof(MODULES[0]);

} // namespace

// The handle is the table row, cast the way dlopen's was.
extern "C" int _citrus_load_module(_citrus_module_t *__restrict rhandle,
                                   const char *__restrict encname)
{
    for (usize i = 0; i < MODULE_N; i++)
        if (strcmp(MODULES[i].name, encname) == 0) {
            *rhandle = (_citrus_module_t)&MODULES[i];
            return 0;
        }
    return EINVAL;
}

extern "C" void *_citrus_find_getops(_citrus_module_t __restrict handle,
                                     const char *__restrict modname, const char *__restrict ifname)
{
    const ModuleEntry *e = (const ModuleEntry *)handle;
    if (!e || strcmp(e->name, modname) != 0 || strcmp(e->iface, ifname) != 0)
        return nullptr;
    return (void *)e->getops;
}

// Nothing was loaded, so nothing is unloaded.
extern "C" void _citrus_unload_module(_citrus_module_t)
{
}
