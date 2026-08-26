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
#include <sys/cdefs.h>
#include <sys/types.h>

#include "citrus_namespace.h"

extern "C" {

// The two iconv drivers.
void *_citrus_iconv_std_iconv_getops(void);
void *_citrus_iconv_none_iconv_getops(void);

// The stdenc modules. "NONE" is not among them: citrus_stdenc.c reaches
// citrus_none.cpp directly, without a module at all.
void *_citrus_UTF8_stdenc_getops(void);
void *_citrus_UTF8MAC_stdenc_getops(void);
void *_citrus_UTF7_stdenc_getops(void);
void *_citrus_UTF1632_stdenc_getops(void);
void *_citrus_ISO2022_stdenc_getops(void);
void *_citrus_HZ_stdenc_getops(void);
void *_citrus_VIQR_stdenc_getops(void);
void *_citrus_MSKanji_stdenc_getops(void);
void *_citrus_ZW_stdenc_getops(void);
void *_citrus_BIG5_stdenc_getops(void);
void *_citrus_UES_stdenc_getops(void);
void *_citrus_GBK2K_stdenc_getops(void);
void *_citrus_DECHanyu_stdenc_getops(void);
void *_citrus_EUC_stdenc_getops(void);
void *_citrus_EUCTW_stdenc_getops(void);
void *_citrus_JOHAB_stdenc_getops(void);
void *_citrus_DECKanji_stdenc_getops(void);

// The mappers. mapper_parallel is a second entry point in mapper_serial's
// source, as it is upstream — two targets, one file.
void *_citrus_mapper_std_mapper_getops(void);
void *_citrus_mapper_serial_mapper_getops(void);
void *_citrus_mapper_parallel_mapper_getops(void);
void *_citrus_mapper_zone_mapper_getops(void);
void *_citrus_mapper_646_mapper_getops(void);
void *_citrus_mapper_none_mapper_getops(void);
}

namespace {

struct ModuleEntry {
    const char *name;
    const char *iface;
    void *(*getops)(void);
};

// The names are the ones esdb's ENCODING fields and mapper.dir's second column
// hold, which is what citrus passes to _citrus_load_module.
constexpr ModuleEntry MODULES[] = {
    { "iconv_std", "iconv", _citrus_iconv_std_iconv_getops },
    { "iconv_none", "iconv", _citrus_iconv_none_iconv_getops },

    { "UTF8", "stdenc", _citrus_UTF8_stdenc_getops },
    { "UTF8MAC", "stdenc", _citrus_UTF8MAC_stdenc_getops },
    { "UTF7", "stdenc", _citrus_UTF7_stdenc_getops },
    { "UTF1632", "stdenc", _citrus_UTF1632_stdenc_getops },
    { "ISO2022", "stdenc", _citrus_ISO2022_stdenc_getops },
    { "HZ", "stdenc", _citrus_HZ_stdenc_getops },
    { "VIQR", "stdenc", _citrus_VIQR_stdenc_getops },
    { "MSKanji", "stdenc", _citrus_MSKanji_stdenc_getops },
    { "ZW", "stdenc", _citrus_ZW_stdenc_getops },
    { "BIG5", "stdenc", _citrus_BIG5_stdenc_getops },
    { "UES", "stdenc", _citrus_UES_stdenc_getops },
    { "GBK2K", "stdenc", _citrus_GBK2K_stdenc_getops },
    { "DECHanyu", "stdenc", _citrus_DECHanyu_stdenc_getops },
    { "EUC", "stdenc", _citrus_EUC_stdenc_getops },
    { "EUCTW", "stdenc", _citrus_EUCTW_stdenc_getops },
    { "JOHAB", "stdenc", _citrus_JOHAB_stdenc_getops },
    { "DECKanji", "stdenc", _citrus_DECKanji_stdenc_getops },

    { "mapper_std", "mapper", _citrus_mapper_std_mapper_getops },
    { "mapper_serial", "mapper", _citrus_mapper_serial_mapper_getops },
    { "mapper_parallel", "mapper", _citrus_mapper_parallel_mapper_getops },
    { "mapper_zone", "mapper", _citrus_mapper_zone_mapper_getops },
    { "mapper_646", "mapper", _citrus_mapper_646_mapper_getops },
    { "mapper_none", "mapper", _citrus_mapper_none_mapper_getops },
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
