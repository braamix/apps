// `_sysconfig` and the build's own data module, which `sysconfig` reads.
//
// CPython generates `_sysconfigdata_<abiflags>_<platform>_<multiarch>.py` at
// build time and installs it beside the library. There is no build directory
// here and nothing to generate into, so the same module is native and the
// name is the one sysconfig.py derives: `_sysconfigdata__braam_wasm32-braam`.
//
// What it holds is only what this system can answer truthfully. There is no
// compiler, no shared library and no extension module, so the variables that
// describe those are absent rather than invented, and sysconfig's `get` then
// answers None for them the way it does on a build that had none.
#include "builtin.h"
#include "func.h"
#include "gc.h"
#include "intern.h"
#include "module.h"
#include "obj.h"
#include "ops.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// The variables `sysconfig` merges into its own. Strings and numbers only:
// the module is data.
struct Var {
    Str name;
    Str text; // empty for a number
    i64 n;
};

constexpr Var BUILD_VARS[] = {
    { "ABIFLAGS", "", 0 },
    { "abiflags", "", 0 },
    { "prefix", "/pkg", 0 },
    { "exec_prefix", "/pkg", 0 },
    { "BINDIR", "/pkg/bin", 0 },
    { "LIBDIR", "/pkg/lib", 0 },
    { "LIBDEST", "/pkg/lib/python3.14", 0 },
    { "BINLIBDEST", "/pkg/lib/python3.14", 0 },
    { "INCLUDEPY", "/pkg/include/python3.14", 0 },
    { "VERSION", "3.14", 0 },
    { "LDVERSION", "3.14", 0 },
    { "MULTIARCH", "wasm32-braam", 0 },
    { "SOABI", "braam-314-wasm32-braam", 0 },
    { "EXT_SUFFIX", ".braam-314-wasm32-braam.so", 0 },
    { "SHLIB_SUFFIX", ".so", 0 },
    { "EXE", "", 0 },
    { "HOST_GNU_TYPE", "wasm32-unknown-unknown", 0 },
    { "MACHDEP", "braam", 0 },
    { "SIZEOF_VOID_P", "", 4 },
    { "SIZEOF_LONG", "", 4 },
    { "Py_DEBUG", "", 0 },
    { "Py_GIL_DISABLED", "", 0 },
    { "Py_ENABLE_SHARED", "", 0 },
    { "WITH_PYMALLOC", "", 0 },
    { "HAVE_FORK", "", 0 },
    { "HAVE_VFORK", "", 0 },
    { "HAVE_DYNAMIC_LOADING", "", 0 },
};

// _sysconfig.config_vars(): what CPython's non-posix branch takes from the
// running interpreter rather than from the data module. It is the same
// handful either way.
constexpr Var CONFIG_VARS[] = {
    { "ABIFLAGS", "", 0 },
    { "EXT_SUFFIX", ".braam-314-wasm32-braam.so", 0 },
    { "SOABI", "braam-314-wasm32-braam", 0 },
    { "Py_DEBUG", "", 0 },
    { "Py_GIL_DISABLED", "", 0 },
};

Value vars_dict(const Var *tab, usize n)
{
    DictObj *d = dict_new();
    if (!d)
        return oom(), Value();
    Root rd{ obj_value(d) };
    for (usize i = 0; i < n; i++) {
        DictObj *at = static_cast<DictObj *>(rd.v.obj());
        bool ok     = tab[i].text.empty() ? mod_int(at, tab[i].name, tab[i].n)
                                          : mod_str(at, tab[i].name, tab[i].text);
        if (!ok)
            return Value();
    }
    return rd.v;
}

R s_config_vars(const CallArgs &a, Value &out)
{
    if (!args_only(a, "config_vars", 0, 0))
        return R::Err;
    out = vars_dict(CONFIG_VARS, sizeof CONFIG_VARS / sizeof CONFIG_VARS[0]);
    return out.is_nil() ? R::Err : R::Ok;
}

// Only the Windows branch of sysconfig asks, and the answer there is what
// sys.platform says. It is here so the module is whole.
R s_get_platform(const CallArgs &a, Value &out)
{
    if (!args_only(a, "get_platform", 0, 0))
        return R::Err;
    out = str_new("braam");
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr ModDef DEFS[] = {
    { "config_vars", s_config_vars },
    { "get_platform", s_get_platform },
};

} // namespace

bool sysconfig_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    return mod_defs(static_cast<DictObj *>(rd.v.obj()), DEFS);
}

bool sysconfigdata_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    Root v{ vars_dict(BUILD_VARS, sizeof BUILD_VARS / sizeof BUILD_VARS[0]) };
    return !v.v.is_nil() && mod_put(static_cast<DictObj *>(rd.v.obj()), "build_time_vars", v.v);
}
