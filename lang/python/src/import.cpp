// The loader: find a module's file, compile it, run its body, cache it.
#include "import.h"

#include "builtin.h"
#include "call.h"
#include "codec.h"
#include "compile.h"
#include "exc.h"
#include "fs/path.h"
#include "gc.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "module.h"
#include "ops.h"
#include "parse.h"
#include "proc/rt.h"
#include "type.h"
#include "vm.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// sys.modules and sys.path outlive every operation, so they live in one block
// the collector is told about.
struct Home {
    Value modules;
    Value path;
    Value busy; // the modules whose bodies are running
    Value meta_path;
    Value path_hooks;
    Value path_cache;
    Value stdlib;   // sys._stdlib_dir
    Value defaults; // (meta_path, path_hooks) as importlib left them, or Nil
    Value spec_fn;  // what gives a module loaded here its __spec__
    Value find_fn;  // importlib's own import, for finders the program added
};

Home *home;

void home_mark()
{
    if (!home)
        return;
    gc_mark(home->modules);
    gc_mark(home->path);
    gc_mark(home->busy);
    gc_mark(home->meta_path);
    gc_mark(home->path_hooks);
    gc_mark(home->path_cache);
    gc_mark(home->stdlib);
    gc_mark(home->defaults);
    gc_mark(home->spec_fn);
    gc_mark(home->find_fn);
}

Home *here()
{
    if (!home) {
        home = heap_new<Home>();
        if (home)
            gc_root_hook(home_mark);
    }
    return home;
}

// Whether `m`'s body is running, or say it is (1) or is not (0).
bool busy(Value m, int set = -1)
{
    Home *h = here();
    if (!h)
        return false;
    Root rm{ m };
    if (h->busy.is_nil()) {
        if (set <= 0)
            return false;
        SetObj *s = set_new();
        if (!s)
            return false;
        h->busy = obj_value(s);
    }
    bool has = false;
    if (set > 0)
        return set_add(set_at(h->busy), rm.v) == R::Ok;
    if (set == 0)
        set_discard(set_at(h->busy), rm.v, has);
    else if (set_has(set_at(h->busy), rm.v, has) != R::Ok)
        err_clear();
    return has;
}

DictObj *dict_at(Value v)
{
    return static_cast<DictObj *>(v.obj());
}

bool put(DictObj *d, Str key, Value v)
{
    Root rd{ obj_value(d) }, rv{ v };
    StrObj *k = str_intern(key);
    return k && dict_set(dict_at(rd.v), obj_value(k), rv.v) == R::Ok;
}

R get(DictObj *d, Str key, Value &out)
{
    StrObj *k = str_intern(key);
    return k ? dict_get(d, obj_value(k), out) : oom();
}

// A module's namespace as a value, which is what a frame's globals must be.
Value mdict(Value m)
{
    return obj_value(module_dict(m));
}

// ---------------------------------------------------------------- the job

// One import in flight. The continuation's six slots are not enough and would
// not be named; this is the heap block ground rule 5 asks for.
struct Job : Obj {
    Value name;     // the absolute dotted name asked for
    Value fromlist; // a tuple, or Nil
    Value parts;    // the dotted name split up
    Value parent;   // the package the next part goes under, or Nil
    Value cands;    // the paths still to try for the part being loaded
    Value mod;      // the module being loaded
    Value leaf;     // the deepest module, once the parts are done
    Value target;   // the dotted name of the module being loaded
    Value body;     // the module's body, while its spec is being made
    u32 at;         // parts already loaded
    u32 cand;       // candidates already tried
    u32 from_at;    // fromlist names already looked at
    u32 stage;      // 0 walking the parts, 1 walking the fromlist
    u32 state;
    bool optional; // a fromlist name that need not be a module
};

enum : u32 { ST_START, ST_TRY, ST_BODY, ST_DECODE, ST_SPEC, ST_SPECD, ST_INSTALL, ST_SLOW };

void job_trace(Obj *o)
{
    Job *j = static_cast<Job *>(o);
    gc_mark(j->name);
    gc_mark(j->fromlist);
    gc_mark(j->parts);
    gc_mark(j->parent);
    gc_mark(j->cands);
    gc_mark(j->mod);
    gc_mark(j->leaf);
    gc_mark(j->target);
    gc_mark(j->body);
}

R job_repr(Value, String &out)
{
    return out.append("<import>") ? R::Ok : oom();
}

constexpr Type job_type{ .name = "import", .trace = job_trace, .repr = job_repr };

Job *job_of(Value v)
{
    return static_cast<Job *>(v.obj());
}

Value job_new()
{
    Job *j = static_cast<Job *>(obj_alloc(&job_type, sizeof(Job)));
    if (!j)
        return oom(), Value();
    j->name = j->fromlist = j->parts = j->parent = Value();
    j->cands = j->mod = j->leaf = j->target = j->body = Value();
    j->at = j->cand = j->from_at = j->stage = 0;
    j->state                                = ST_START;
    j->optional                             = false;
    return obj_value(j);
}

// ------------------------------------------------------------ the tracing

// -v and -X importtime, both to stderr. A body in progress is a frame here:
// when it started, and how long the imports it made took between them.
struct Timing {
    u32 start;
    u32 children;
};

constexpr u32 TIMING_MAX = 64;
Timing timing[TIMING_MAX];
u32 timing_depth;
bool timing_header;

void trace_line(Str text)
{
    String *e = vm_errout();
    if (e)
        (void)(e->append(text) && e->push('\n'));
}

// A module is done: its line, indented by how deep it was imported.
void time_line(Str name, u32 self_ms, u32 cum_ms, u32 depth)
{
    if (!timing_header) {
        timing_header = true;
        trace_line("import time: self [us] | cumulative | imported package");
    }
    char t[24];
    Buf<192> b;
    b.put("import time: ");
    Str self = int_text(t, sizeof t, i64(self_ms) * 1000);
    for (usize i = self.size(); i < 9; i++)
        b.put(' ');
    b.put(self).put(" | ");
    Str cum = int_text(t, sizeof t, i64(cum_ms) * 1000);
    for (usize i = cum.size(); i < 10; i++)
        b.put(' ');
    b.put(cum).put(" | ");
    for (u32 i = 0; i < depth; i++)
        b.put("  ");
    b.put(name);
    trace_line(b.str());
}

void trace_native(Str name)
{
    const PyConfig &c = py_config();
    if (c.verbose) {
        Buf<160> b;
        b.put("import '").put(name).put("' # built-in");
        trace_line(b.str());
    }
    if (c.import_time)
        time_line(name, 0, 0, timing_depth);
}

void trace_begin(Str path)
{
    const PyConfig &c = py_config();
    if (c.verbose) {
        Buf<560> b;
        b.put("# code object from '").put(path).put("'");
        trace_line(b.str());
    }
    if (c.import_time && timing_depth < TIMING_MAX)
        timing[timing_depth] = Timing{ proc_now(), 0 };
    if (c.import_time)
        timing_depth++;
}

// `ok` false for a body that raised, which gets no line.
void trace_end(Str name, Str path, bool ok)
{
    const PyConfig &c = py_config();
    if (ok && c.verbose) {
        Buf<560> b;
        b.put("import '").put(name).put("' # from '").put(path).put("'");
        trace_line(b.str());
    }
    if (!c.import_time || !timing_depth)
        return;
    u32 d = --timing_depth;
    if (d >= TIMING_MAX)
        return;
    u32 cum  = proc_now() - timing[d].start;
    u32 self = cum > timing[d].children ? cum - timing[d].children : 0;
    if (d && d - 1 < TIMING_MAX)
        timing[d - 1].children += cum;
    if (ok)
        time_line(name, self, cum, d);
}

// The file a module was run from, for its trace line.
Str module_file(Value m)
{
    Value f;
    if (!is_module(m) || get(module_dict(m), "__file__", f) != R::Ok || !is_str(f))
        return Str();
    return str_of(f)->str();
}

// ------------------------------------------------------------ the searching

// The first `n` parts of a dotted name, joined again.
Value dotted(Value parts, usize n)
{
    Root rp{ parts };
    String b;
    for (usize i = 0; i < n; i++) {
        if (i && !b.push('.'))
            return oom(), Value();
        if (!b.append(str_of(list_of(rp.v)->items[i])->str()))
            return oom(), Value();
    }
    return str_new(b.str());
}

// Where a module's file may be. A package's `__path__` when there is a parent,
// sys.path otherwise.
Value roots_for(Value parent)
{
    if (parent.is_nil())
        return sys_path();
    Root rp{ parent };
    Value p;
    if (get(module_dict(rp.v), "__path__", p) != R::Ok)
        return err_pending() ? Value() : (err_set("ImportError", "not a package"), Value());
    return p;
}

// Three per root, in the order CPython prefers them: a package, a module,
// then the bare directory, which is a namespace package. A path ending in `/`
// is the driver's cue to answer "is this a directory" rather than to read.
Value candidates(Value roots, Str leaf)
{
    Root rr{ roots };
    ListObj *out = list_new();
    if (!out)
        return oom(), Value();
    Root ro{ obj_value(out) };
    usize n = 0;
    if (py_len(rr.v, n) != R::Ok)
        return Value();
    for (usize i = 0; i < n; i++) {
        Value root;
        if (py_getitem(rr.v, Value::of_int(i32(i)), root) != R::Ok)
            return Value();
        if (!is_str(root))
            continue;
        Str dir      = str_of(root)->str();
        Str tails[3] = { Str("/__init__.py"), Str(".py"), Str("/") };
        for (Str tail : tails) {
            String p;
            if (!dir.empty() && (!p.append(dir) || !p.push('/')))
                return oom(), Value();
            if (!p.append(leaf) || !p.append(tail))
                return oom(), Value();
            Value v = str_new(p.str());
            if (v.is_nil() || !list_push(list_of(ro.v), v))
                return Value();
        }
    }
    return ro.v;
}

// ------------------------------------------------------------ importlib

// Run once importlib has loaded: the finders CPython starts with, a spec for
// every module already here, and two functions for the loader to call.
constexpr Str INSTALL = R"PY(import sys, _imp
from importlib import _bootstrap as _b, _bootstrap_external as _e
sys.meta_path.append(_b.BuiltinImporter)
sys.meta_path.append(_b.FrozenImporter)
_e._install(_b)

def _spec(module, path):
    name = module.__name__
    if path is False:
        spec = _b.spec_from_loader(name, _b.BuiltinImporter, origin='built-in')
    elif path is None:
        loader = _e.NamespaceLoader(name, module.__path__, _e.PathFinder._get_spec)
        spec = _b.ModuleSpec(name, loader, is_package=True)
        spec.submodule_search_locations = loader._path
    else:
        spec = _e.spec_from_file_location(
            name, path, submodule_search_locations=module.__dict__.get('__path__'))
    _b._init_module_attrs(spec, module)

def _find(name, import_):
    return _b._find_and_load(name, import_)

for _m in list(sys.modules.values()):
    if (isinstance(_m, type(sys)) and _m.__spec__ is None
            and _m.__name__ != '__main__'):
        if _m.__dict__.get('__file__') is not None:
            _spec(_m, _m.__file__)
        elif '__path__' in _m.__dict__:
            _spec(_m, None)
)PY";

bool installed()
{
    return home && !home->defaults.is_nil();
}

Value sys_attr(Str name)
{
    Value m, v;
    DictObj *d = sys_modules();
    if (!d || get(d, "sys", m) != R::Ok || !is_module(m))
        return Value();
    return get(module_dict(m), name, v) == R::Ok ? v : Value();
}

bool same_items(Value list, Value tuple)
{
    if (!is_list(list))
        return false;
    ListObj *l  = list_of(list);
    TupleObj *t = static_cast<TupleObj *>(tuple.obj());
    if (l->items.size() != t->len)
        return false;
    for (u32 i = 0; i < t->len; i++)
        if (l->items[i].w != t->items()[i].w)
            return false;
    return true;
}

// Whether a finder or a path hook the program added has to be asked, which
// only importlib's own import knows how to do.
bool custom()
{
    if (!installed())
        return false;
    TupleObj *d = static_cast<TupleObj *>(home->defaults.obj());
    return !same_items(sys_attr("meta_path"), d->items()[0]) ||
           !same_items(sys_attr("path_hooks"), d->items()[1]);
}

Value snapshot(Value list)
{
    if (!is_list(list))
        return obj_value(tuple_new(0));
    Root rl{ list };
    TupleObj *t = tuple_new(list_of(rl.v)->items.size());
    if (!t)
        return Value();
    for (usize i = 0; i < list_of(rl.v)->items.size(); i++)
        t->items()[i] = list_of(rl.v)->items[i];
    return obj_value(t);
}

// Ask for the spec of `m`, whose file is `path` (None for a namespace
// package, False for a native), then come back in state `next`.
R ask_spec(ContObj *k, Value m, Value path, u32 next)
{
    job_of(k->s[0])->state = next;
    k->locals              = Value();
    return cont_call(k, home->spec_fn, m, 2, path);
}

R install(ContObj *k)
{
    Ast ast;
    if (!ast.parse(INSTALL, true))
        return R::Err;
    Root code{ py_compile(ast, "<frozen importlib._bootstrap>") };
    if (code.v.is_nil())
        return R::Err;
    DictObj *g = dict_new();
    if (!g)
        return oom();
    Root rg{ obj_value(g) };
    Root nm{ str_new("_braam_importlib") };
    if (nm.v.is_nil() || !put(dict_at(rg.v), "__name__", nm.v) || !put_builtins(dict_at(rg.v)))
        return R::Err;
    Root fn{ func_new(code.v, rg.v) };
    if (fn.v.is_nil())
        return R::Err;
    Job *j    = job_of(k->s[0]);
    j->body   = rg.v;
    j->state  = ST_INSTALL;
    k->locals = rg.v;
    return cont_call(k, fn.v, Value(), 0);
}

R installed_now(ContObj *k)
{
    Job *j = job_of(k->s[0]);
    Root g{ j->body };
    j->body   = Value();
    k->locals = Value();
    Value spec, find;
    if (get(dict_at(g.v), "_spec", spec) != R::Ok || get(dict_at(g.v), "_find", find) != R::Ok)
        return err_pending() ? R::Err : err_set("ImportError", "importlib did not install");
    home->spec_fn = spec;
    home->find_fn = find;
    Root mp{ snapshot(sys_attr("meta_path")) };
    Root ph{ snapshot(sys_attr("path_hooks")) };
    TupleObj *t = mp.v.is_nil() || ph.v.is_nil() ? nullptr : tuple_new(2);
    if (!t)
        return oom();
    t->items()[0]  = mp.v;
    t->items()[1]  = ph.v;
    home->defaults = obj_value(t);
    return R::Ok;
}

// ------------------------------------------------------------- the machine

R walk(ContObj *k);
R from_step(ContObj *k);
R begin_load(ContObj *k, Value full, Value parent, bool optional);

// The last leaf of a dotted name, or empty when there is no dot in it.
Str leaf_of(Str full)
{
    usize at = full.size();
    while (at > 0 && full[at - 1] != '.')
        at--;
    return at ? full.substr(at) : Str();
}

// Hang the module on its package, which is what makes `pkg.sub` an attribute
// of `pkg`. After the body, as in CPython: a module that raised is not one.
bool bind_to_parent(Job *j)
{
    Value parent = j->stage == 0 ? j->parent : j->leaf;
    if (j->mod.is_nil() || parent.is_nil() || !is_module(parent) || j->target.is_nil())
        return true;
    Str leaf = leaf_of(str_of(j->target)->str());
    return leaf.empty() || put(module_dict(parent), leaf, j->mod);
}

// A part is in place: on to the next one, or back to the fromlist.
R loaded(ContObj *k)
{
    Job *j = job_of(k->s[0]);
    if (j->state == ST_BODY) {
        busy(j->mod, 0);
        j = job_of(k->s[0]);
        trace_end(str_of(j->target)->str(), module_file(j->mod), true);
        if (!installed() && str_of(j->target)->str() == "importlib")
            return install(k);
    }
    j->state = ST_START;
    if (!bind_to_parent(j))
        return R::Err;
    j = job_of(k->s[0]);
    if (j->stage == 0) {
        j->parent = j->mod;
        j->at++;
        return walk(k);
    }
    return from_step(k);
}

R no_module(Value name)
{
    Buf<128> b;
    b.put("No module named '").put(is_str(name) ? str_of(name)->str() : Str("?")).put("'");
    Root rn{ name };
    Root msg{ str_new(b.str()) };
    if (msg.v.is_nil())
        return R::Err;
    return exc_raise_import("ModuleNotFoundError", msg.v, rn.v, Value());
}

// Every candidate missed. For a fromlist name that is only maybe a module,
// that is not an error.
R missed(ContObj *k)
{
    Job *j = job_of(k->s[0]);
    if (!j->optional)
        return no_module(j->target);
    return loaded(k);
}

R begin_load(ContObj *k, Value full, Value parent, bool optional)
{
    Root rf{ full }, rp{ parent };
    Job *j      = job_of(k->s[0]);
    j->target   = rf.v;
    j->optional = optional;
    j->mod      = Value();
    j->cand     = 0;

    // A finder the program added: importlib does the whole of this one.
    if (custom()) {
        Value imp;
        if (get(module_dict(builtins_module()), "__import__", imp) != R::Ok)
            return err_pending() ? R::Err : no_module(rf.v);
        j->state    = ST_SLOW;
        k->locals   = Value();
        k->catching = optional ? CATCH_ANY : CATCH_NONE;
        return cont_call(k, home->find_fn, rf.v, 2, imp);
    }

    // A module written in C++ needs no file at all.
    Value made = builtin_module(str_of(rf.v)->str());
    if (!made.is_nil()) {
        Root rm{ made };
        if (!module_register(str_of(rf.v)->str(), rm.v))
            return R::Err;
        trace_native(str_of(rf.v)->str());
        job_of(k->s[0])->mod = rm.v;
        Value spec;
        if (installed() && get(module_dict(rm.v), "__spec__", spec) == R::Ok && is_none(spec))
            return ask_spec(k, rm.v, value_bool(false), ST_SPECD);
        return loaded(k);
    }
    if (err_pending())
        return R::Err;

    Root roots{ roots_for(rp.v) };
    if (roots.v.is_nil()) {
        if (!optional)
            return err_pending() ? R::Err : no_module(rf.v);
        err_clear();
        return loaded(k);
    }
    Str dotted_name = str_of(rf.v)->str();
    Str leaf        = leaf_of(dotted_name);
    Value cands     = candidates(roots.v, leaf.empty() ? dotted_name : leaf);
    if (cands.is_nil())
        return R::Err;
    j        = job_of(k->s[0]);
    j->cands = cands;
    j->state = ST_TRY;
    if (list_of(cands)->items.empty())
        return missed(k);
    return cont_read(k, str_of(list_of(cands)->items[j->cand++])->str());
}

// A directory with no __init__.py is a namespace package: a module with a
// __path__ and nothing else, and no body to run.
R make_namespace(ContObj *k, Str path)
{
    Job *j = job_of(k->s[0]);
    Root name{ j->target };
    Root m{ module_new(str_of(name.v)->str()) };
    if (m.v.is_nil())
        return R::Err;
    ListObj *p = list_new();
    if (!p)
        return oom();
    Root rp{ obj_value(p) };
    Value dir = str_new(path.substr(0, path.size() - 1));
    if (dir.is_nil() || !list_push(list_of(rp.v), dir))
        return R::Err;
    if (!put(module_dict(m.v), "__path__", rp.v))
        return R::Err;
    if (!module_register(str_of(name.v)->str(), m.v))
        return R::Err;
    job_of(k->s[0])->mod = m.v;
    if (installed())
        return ask_spec(k, m.v, value_none(), ST_SPECD);
    return loaded(k);
}

// The text of a source, which is bytes until a codec has made it a str.
Str source_bytes(Value v)
{
    return is_str(v) ? str_of(v)->str() : static_cast<BytesObj *>(v.obj())->str();
}

R start_body(ContObj *k, Value fn)
{
    Job *j    = job_of(k->s[0]);
    j->state  = ST_BODY;
    j->body   = Value();
    k->locals = mdict(j->mod);
    return cont_call(k, fn, Value(), 0);
}

// The source of `path` became a module. Register it, then run its body.
R run_body(ContObj *k, Value source, Str path)
{
    Root rs{ source };
    Job *j = job_of(k->s[0]);
    Root name{ j->target }, parent{ j->parent };

    Ast ast;
    if (!ast.parse(source_bytes(rs.v), is_str(rs.v))) {
        // A cookie naming a codec written in Python: decode, then come back.
        if (!ast.lex.codec.empty()) {
            err_clear();
            Root enc{ str_new(ast.lex.codec.str()) };
            Root fn{ native_new("decode", lex_source_decode) };
            if (enc.v.is_nil() || fn.v.is_nil())
                return R::Err;
            job_of(k->s[0])->state = ST_DECODE;
            return cont_call(k, fn.v, rs.v, 2, enc.v);
        }
        return err_set_file(path, source_bytes(rs.v)), R::Err;
    }
    Root code{ py_compile(ast, path) };
    if (code.v.is_nil())
        return err_set_file(path, source_bytes(rs.v)), R::Err;

    Root m{ module_new(str_of(name.v)->str()) };
    if (m.v.is_nil())
        return R::Err;
    DictObj *d = module_dict(m.v);
    Root file{ str_new(path) };
    if (file.v.is_nil() || !put(d, "__file__", file.v) || !put_builtins(d))
        return R::Err;
    // A package is a directory with an __init__.py, and its __path__ is what
    // its own submodules are searched along.
    if (path.ends_with("/__init__.py")) {
        ListObj *p = list_new();
        if (!p)
            return oom();
        Root rp{ obj_value(p) };
        Value dir = str_new(path_dirname(path));
        if (dir.is_nil() || !list_push(list_of(rp.v), dir))
            return R::Err;
        if (!put(module_dict(m.v), "__path__", rp.v))
            return R::Err;
    }
    // In the cache before the body runs: that is what makes a cycle stop.
    if (!module_register(str_of(name.v)->str(), m.v) || !busy(m.v, 1))
        return R::Err;
    trace_begin(path);

    Root fn{ func_new(code.v, mdict(m.v)) };
    if (fn.v.is_nil())
        return R::Err;
    j      = job_of(k->s[0]);
    j->mod = m.v;
    if (installed()) {
        j->body = fn.v;
        return ask_spec(k, m.v, file.v, ST_SPEC);
    }
    return start_body(k, fn.v);
}

// Every part of the dotted name, innermost last.
R walk(ContObj *k)
{
    Job *j  = job_of(k->s[0]);
    usize n = list_of(j->parts)->items.size();
    while (j->at < n) {
        Root full{ dotted(j->parts, j->at + 1) };
        if (full.v.is_nil())
            return R::Err;
        Value got;
        R r = dict_get(sys_modules(), full.v, got);
        if (r == R::Err)
            return R::Err;
        if (r == R::Ok && is_none(got)) {
            // A module blocked on purpose, as test.support does it.
            Buf<160> b;
            b.put("import of ").put(str_of(full.v)->str()).put(" halted; None in sys.modules");
            return err_set("ModuleNotFoundError", b.str());
        }
        if (r == R::Ok) {
            j         = job_of(k->s[0]);
            j->parent = got;
            j->at++;
            continue;
        }
        j = job_of(k->s[0]);
        return begin_load(k, full.v, j->parent, false);
    }

    Value leaf;
    if (dict_get(sys_modules(), j->name, leaf) != R::Ok)
        return err_pending() ? R::Err : no_module(j->name);
    j          = job_of(k->s[0]);
    j->leaf    = leaf;
    j->stage   = 1;
    j->from_at = 0;
    return from_step(k);
}

// `from pkg import a, b`: a name in the list may be a submodule rather than an
// attribute, and importing it is what makes it one.
R from_step(ContObj *k)
{
    Job *j  = job_of(k->s[0]);
    usize n = j->fromlist.is_nil() ? 0 : static_cast<TupleObj *>(j->fromlist.obj())->len;
    while (j->from_at < n) {
        Value nm = static_cast<TupleObj *>(j->fromlist.obj())->items()[j->from_at++];
        if (!is_str(nm) || str_of(nm)->str() == "*")
            continue;
        Value ignored;
        if (get(module_dict(j->leaf), "__path__", ignored) != R::Ok)
            break; // not a package: nothing to import under it
        R has = dict_get(module_dict(j->leaf), nm, ignored);
        if (has == R::Err)
            return R::Err;
        if (has == R::Ok)
            continue;

        String b;
        if (!b.append(str_of(static_cast<ModuleObj *>(j->leaf.obj())->name)->str()) ||
            !b.push('.') || !b.append(str_of(nm)->str()))
            return oom();
        Root sub{ str_new(b.str()) };
        if (sub.v.is_nil())
            return R::Err;
        Value already;
        R cached = dict_get(sys_modules(), sub.v, already);
        if (cached == R::Err)
            return R::Err;
        if (cached == R::Ok)
            continue;
        j = job_of(k->s[0]);
        return begin_load(k, sub.v, j->leaf, true);
    }

    // `import a.b` binds `a`; `from a.b import c` works on `a.b`.
    j = job_of(k->s[0]);
    if (!j->fromlist.is_nil() && static_cast<TupleObj *>(j->fromlist.obj())->len)
        return cont_done(k, j->leaf);
    Value top;
    if (dict_get(sys_modules(), list_of(j->parts)->items[0], top) != R::Ok)
        return err_pending() ? R::Err : no_module(j->name);
    return cont_done(k, top);
}

// The body raised, so what is in the cache is not a module. CPython takes it
// back out and so does this.
void import_failed(ContObj *k)
{
    Job *j = job_of(k->s[0]);
    if ((j->state != ST_BODY && j->state != ST_SPEC) || j->mod.is_nil())
        return;
    trace_end(str_of(j->target)->str(), Str(), false);
    busy(j->mod, 0);
    DictObj *d = sys_modules();
    if (d)
        dict_del(d, static_cast<ModuleObj *>(j->mod.obj())->name);
    j->mod = Value();
    err_clear();
}

R import_step(ContObj *k, Value in)
{
    Job *j = job_of(k->s[0]);
    switch (j->state) {
    case ST_START:
        return walk(k);
    case ST_TRY: {
        if (in.is_nil() || is_none(in)) {
            j = job_of(k->s[0]);
            if (j->cand < list_of(j->cands)->items.size())
                return cont_read(k, str_of(list_of(j->cands)->items[j->cand++])->str());
            return missed(k);
        }
        Str path = str_of(list_of(j->cands)->items[j->cand - 1])->str();
        if (is_bool(in))
            return make_namespace(k, path);
        return run_body(k, in, path);
    }
    case ST_DECODE: {
        Str path = str_of(list_of(j->cands)->items[j->cand - 1])->str();
        return run_body(k, in, path);
    }
    case ST_SPEC:
        return start_body(k, j->body);
    case ST_SPECD:
        return loaded(k);
    case ST_INSTALL:
        if (installed_now(k) != R::Ok)
            return R::Err;
        return loaded(k);
    case ST_SLOW: {
        k->catching = CATCH_NONE;
        if (!k->caught.is_nil()) {
            // A fromlist name that is not a module is not an error.
            Root c{ k->caught };
            k->caught = Value();
            Value nm;
            bool mine = exc_is(exc_type_of(c.v), exc_find("ModuleNotFoundError")) &&
                        py_getattr(c.v, str_intern("name"), nm) == R::Ok && is_str(nm) &&
                        str_of(nm)->str() == str_of(job_of(k->s[0])->target)->str();
            err_clear();
            if (!mine)
                return err_set_value(c.v);
            job_of(k->s[0])->mod = Value();
            return loaded(k);
        }
        j->mod = in;
        return loaded(k);
    }
    default:
        return loaded(k);
    }
}

// The dotted name, split on the dots.
Value split_parts(Str name)
{
    ListObj *l = list_new();
    if (!l)
        return oom(), Value();
    Root rl{ obj_value(l) };
    usize at = 0;
    for (usize i = 0; i <= name.size(); i++) {
        if (i < name.size() && name[i] != '.')
            continue;
        Value part = str_new(name.substr(at, i - at));
        if (part.is_nil() || !list_push(list_of(rl.v), part))
            return Value();
        at = i + 1;
    }
    return rl.v;
}

// Drop the last dotted component of `s`. Empty when there is none left.
Str drop_last(Str s)
{
    usize at = s.size();
    while (at > 0 && s[at - 1] != '.')
        at--;
    return at ? s.substr(0, at - 1) : Str();
}

// A relative import: `level` dots counted back from the importing module's
// package, with `name` hung on the end. `where` is the importing frame's
// globals, which is what says where that is.
R absolute(Str name, i64 level, Value where, Value &out)
{
    if (level <= 0) {
        out = str_new(name);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (where.is_nil() || !is_dict(where))
        return err_set("TypeError", "globals must be a dict");

    Root rw{ where };
    Value pkg, own;
    if (get(dict_at(rw.v), "__package__", pkg) == R::Err)
        return R::Err;
    if (!is_str(pkg)) {
        // No __package__: the module's own name, minus the leaf unless the
        // module is itself a package.
        if (get(dict_at(rw.v), "__name__", own) == R::Err)
            return R::Err;
        if (!is_str(own))
            return err_set("ImportError", "a relative import needs a package");
        Value marker;
        R has = get(dict_at(rw.v), "__path__", marker);
        if (has == R::Err)
            return R::Err;
        Str s = str_of(own)->str();
        pkg   = str_new(has == R::Ok ? s : drop_last(s));
        if (pkg.is_nil())
            return R::Err;
    }
    Root rp{ pkg };
    Str base = str_of(rp.v)->str();
    for (i64 i = 1; i < level; i++)
        base = drop_last(base);
    if (base.empty())
        return err_set("ImportError", "attempted a relative import past the top-level package");

    String b;
    if (!b.append(base) || (!name.empty() && (!b.push('.') || !b.append(name))))
        return oom();
    out = str_new(b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

} // namespace

DictObj *sys_modules()
{
    Home *h = here();
    if (!h)
        return oom(), nullptr;
    if (h->modules.is_nil()) {
        DictObj *d = dict_new();
        if (!d)
            return oom(), nullptr;
        h->modules = obj_value(d);
    }
    return dict_at(h->modules);
}

Value sys_path()
{
    Home *h = here();
    if (!h)
        return oom(), Value();
    if (h->path.is_nil()) {
        ListObj *l = list_new();
        if (!l)
            return oom(), Value();
        h->path = obj_value(l);
    }
    return h->path;
}

Value sys_import_state(ImportState which)
{
    Home *h = here();
    if (!h)
        return oom(), Value();
    Value *at = which == IMPORT_META_PATH    ? &h->meta_path
                : which == IMPORT_PATH_HOOKS ? &h->path_hooks
                : which == IMPORT_PATH_CACHE ? &h->path_cache
                                             : &h->stdlib;
    if (at->is_nil()) {
        Obj *o = which == IMPORT_PATH_CACHE ? static_cast<Obj *>(dict_new())
                 : which == IMPORT_STDLIB   ? nullptr
                                            : static_cast<Obj *>(list_new());
        if (which == IMPORT_STDLIB)
            return value_none();
        if (!o)
            return oom(), Value();
        *at = obj_value(o);
    }
    return *at;
}

void sys_set_path(Str extra, Str library_dir)
{
    if (!library_dir.empty() && here()) {
        Value lib = str_new(library_dir);
        if (lib.is_nil())
            return;
        home->stdlib = lib;
        Value m;
        DictObj *d = sys_modules();
        if (d && get(d, "sys", m) == R::Ok && is_module(m) &&
            !put(module_dict(m), "_stdlib_dir", home->stdlib))
            return;
    }
    Root p{ sys_path() };
    if (p.v.is_nil())
        return;
    while (!extra.empty()) {
        usize colon = extra.find(':');
        Str d       = colon == Str::npos ? extra : extra.substr(0, colon);
        extra       = colon == Str::npos ? Str() : extra.substr(colon + 1);
        if (d.empty())
            continue;
        Value v = str_new(d);
        if (v.is_nil() || !list_push(list_of(p.v), v))
            return;
    }
    if (library_dir.empty())
        return;
    Value v = str_new(library_dir);
    if (!v.is_nil())
        list_push(list_of(p.v), v);
}

void sys_path_first(Str dir)
{
    Root p{ sys_path() };
    if (p.v.is_nil())
        return;
    Value v = str_new(dir);
    if (!v.is_nil())
        (void)list_of(p.v)->items.insert(0, v);
}

bool module_register(Str name, Value m)
{
    Root rm{ m };
    DictObj *d = sys_modules();
    if (!d)
        return false;
    return put(d, name, rm.v);
}

R py_import(const CallArgs &a, Value &out)
{
    if (a.nargs < 1 || !is_str(a.args[0]))
        return err_set("TypeError", "__import__() needs a module name");
    Root name{ a.args[0] };
    Root level{ a.nargs > 1 ? a.args[1] : Value::of_int(0) };
    Root from;
    if (a.nargs > 2 && (is_tuple(a.args[2]) || is_list(a.args[2]))) {
        // A list is as good as a tuple here, and asyncio passes one.
        from = a.args[2];
        if (is_list(from.v)) {
            ListObj *l  = list_of(from.v);
            TupleObj *t = tuple_new(l->items.size());
            if (!t)
                return oom();
            for (usize i = 0; i < l->items.size(); i++)
                t->items()[i] = list_of(from.v)->items[i];
            from = obj_value(t);
        }
    }
    Root where{ a.nargs > 3 ? a.args[3] : Value() };

    i64 lv = 0;
    if (!as_index(level.v, lv))
        lv = 0;
    if (lv < 0)
        return err_set("ValueError", "level must be >= 0");
    if (lv > 0 && !where.v.is_nil() && !is_dict(where.v))
        return err_set("TypeError", "globals must be a dict");
    // `from . import x` has no name of its own, so only an absolute one has
    // to have something in it.
    if (!lv && !str_of(name.v)->len)
        return err_set("ValueError", "empty module name");
    Root full;
    if (absolute(str_of(name.v)->str(), lv, where.v, full.v) != R::Ok)
        return R::Err;
    Root parts{ split_parts(str_of(full.v)->str()) };
    if (parts.v.is_nil())
        return R::Err;

    Root jv{ job_new() };
    if (jv.v.is_nil())
        return R::Err;
    Root kv{ cont_new(import_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->fail = import_failed;
    Job *j              = job_of(jv.v);
    j->name             = full.v;
    j->fromlist         = from.v;
    j->parts            = parts.v;
    cont_of(kv.v)->s[0] = jv.v;
    out                 = kv.v;
    return R::Ok;
}

R b_import(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "name", "globals", "locals", "fromlist", "level" };
    Value got[5];
    if (a.nargs > 5)
        return err_set("TypeError", "__import__() takes at most 5 arguments");
    for (u32 i = 0; i < a.nargs; i++)
        got[i] = a.args[i];
    for (u32 k = 0; k < a.nkw; k++) {
        Str nm = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        u32 i  = 0;
        while (i < 5 && NAMES[i] != nm)
            i++;
        if (i == 5 || !got[i].is_nil()) {
            Buf<128> b;
            b.put("__import__() got an unexpected keyword argument '").put(nm).put("'");
            return err_set("TypeError", b.str());
        }
        got[i] = a.kwvals[k];
    }
    if (got[0].is_nil())
        return err_set("TypeError", "__import__() missing required argument 'name' (pos 1)");
    Value v[4] = { got[0], got[4].is_nil() ? Value::of_int(0) : got[4],
                   is_none(got[3]) ? Value() : got[3], is_none(got[1]) ? Value() : got[1] };
    CallArgs b;
    b.args  = v;
    b.nargs = 4;
    return py_import(b, out);
}

R import_absolute(Str name, i64 level, Value where, Value &out)
{
    return absolute(name, level, where, out);
}

Value import_submodule(Value m, StrObj *name)
{
    if (!is_module(m))
        return Value();
    Root rm{ m }, rn{ obj_value(name) };
    Value pkg;
    if (get(module_dict(rm.v), "__name__", pkg) != R::Ok || !is_str(pkg))
        return err_clear(), Value();
    String full;
    if (!full.append(str_of(pkg)->str()) || !full.push('.') || !full.append(str_of(rn.v)->str()))
        return err_set("MemoryError", "out of memory"), Value();
    DictObj *mods = sys_modules();
    StrObj *key   = str_intern(full.str());
    Value found;
    if (!mods || !key)
        return Value();
    if (dict_get(mods, obj_value(key), found) != R::Ok)
        return err_clear(), Value();
    return found;
}

R import_missing(Value m, StrObj *name)
{
    Buf<192> b;
    b.put("cannot import name '").put(name->str()).put("'");
    if (is_module(m)) {
        Root rm{ m };
        bool part = busy(rm.v);
        b.put(" from ");
        if (part)
            b.put("partially initialized module ");
        b.put("'").put(str_of(static_cast<ModuleObj *>(rm.v.obj())->name)->str()).put("'");
        if (part)
            b.put(" (most likely due to a circular import)");
        Value file;
        if (get(module_dict(rm.v), "__file__", file) == R::Ok && is_str(file))
            b.put(" (").put(str_of(file)->str()).put(")");
        else if (!part)
            b.put(" (unknown location)");
        err_clear();
    }
    return err_set("ImportError", b.str());
}

R import_star(Value m, DictObj *into)
{
    if (!is_module(m))
        return err_set2("TypeError", "import * needs a module", type_name(m));
    Root rm{ m }, ri{ obj_value(into) };
    Value all;
    R r = get(module_dict(rm.v), "__all__", all);
    if (r == R::Err)
        return R::Err;

    if (r == R::Ok) {
        Root ra{ all };
        usize n = 0;
        if (py_len(ra.v, n) != R::Ok)
            return R::Err;
        for (usize i = 0; i < n; i++) {
            Root nm;
            if (py_getitem(ra.v, Value::of_int(i32(i)), nm.v) != R::Ok)
                return R::Err;
            Value got;
            if (dict_get(module_dict(rm.v), nm.v, got) != R::Ok)
                return err_pending() ? R::Err
                                     : err_set2("ImportError", "__all__ names nothing",
                                                is_str(nm.v) ? str_of(nm.v)->str() : Str("?"));
            if (dict_set(dict_at(ri.v), nm.v, got) != R::Ok)
                return R::Err;
        }
        return R::Ok;
    }

    usize at = 0;
    Value k, v;
    while (table_next(module_dict(rm.v)->t, at, k, v)) {
        if (is_str(k) && str_of(k)->len && str_of(k)->bytes()[0] == '_')
            continue;
        if (dict_set(dict_at(ri.v), k, v) != R::Ok)
            return R::Err;
    }
    return R::Ok;
}
