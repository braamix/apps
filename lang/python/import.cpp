// The loader: find a module's file, compile it, run its body, cache it.
#include "import.h"

#include "builtin.h"
#include "call.h"
#include "compile.h"
#include "fs/path.h"
#include "gc.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "ops.h"
#include "parse.h"
#include "type.h"

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
};

Home *home;

void home_mark()
{
    if (!home)
        return;
    gc_mark(home->modules);
    gc_mark(home->path);
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
    u32 at;         // parts already loaded
    u32 cand;       // candidates already tried
    u32 from_at;    // fromlist names already looked at
    u32 stage;      // 0 walking the parts, 1 walking the fromlist
    u32 state;
    bool optional; // a fromlist name that need not be a module
};

enum : u32 { ST_START, ST_TRY, ST_BODY };

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
    j->cands = j->mod = j->leaf = j->target = Value();
    j->at = j->cand = j->from_at = j->stage = 0;
    j->state                                = ST_START;
    j->optional                             = false;
    return obj_value(j);
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
    return err_set("ModuleNotFoundError", b.str());
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

    // A module written in C++ needs no file at all.
    Value made = builtin_module(str_of(rf.v)->str());
    if (!made.is_nil()) {
        Root rm{ made };
        if (!module_register(str_of(rf.v)->str(), rm.v))
            return R::Err;
        job_of(k->s[0])->mod = rm.v;
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
    return loaded(k);
}

// The source of `path` became a module. Register it, then run its body.
R run_body(ContObj *k, Value source, Str path)
{
    Root rs{ source };
    Job *j = job_of(k->s[0]);
    Root name{ j->target }, parent{ j->parent };

    Ast ast;
    if (!ast.parse(str_of(rs.v)->str()))
        return R::Err;
    Root code{ py_compile(ast, path) };
    if (code.v.is_nil())
        return R::Err;

    Root m{ module_new(str_of(name.v)->str()) };
    if (m.v.is_nil())
        return R::Err;
    DictObj *d = module_dict(m.v);
    Root file{ str_new(path) };
    if (file.v.is_nil() || !put(d, "__file__", file.v))
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
    if (!module_register(str_of(name.v)->str(), m.v))
        return R::Err;

    Root fn{ func_new(code.v, mdict(m.v)) };
    if (fn.v.is_nil())
        return R::Err;
    j         = job_of(k->s[0]);
    j->mod    = m.v;
    j->state  = ST_BODY;
    k->locals = mdict(m.v);
    return cont_call(k, fn.v, Value(), 0);
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
    if (j->state != ST_BODY || j->mod.is_nil())
        return;
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

void sys_set_path(Str script_dir, Str library_dir)
{
    Root p{ sys_path() };
    if (p.v.is_nil())
        return;
    Str dirs[2] = { script_dir, library_dir };
    for (Str d : dirs) {
        if (d.empty())
            continue;
        Value v = str_new(d);
        if (v.is_nil() || !list_push(list_of(p.v), v))
            return;
    }
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
    Value v[4] = { a.nargs > 0 ? a.args[0] : Value(), a.nargs > 4 ? a.args[4] : Value::of_int(0),
                   a.nargs > 3 ? a.args[3] : Value(), a.nargs > 1 ? a.args[1] : Value() };
    CallArgs b;
    b.args  = v;
    b.nargs = 4;
    return py_import(b, out);
}

R import_missing(Value m, StrObj *name)
{
    Buf<192> b;
    b.put("cannot import name '").put(name->str()).put("'");
    if (is_module(m)) {
        Root rm{ m };
        b.put(" from '").put(str_of(static_cast<ModuleObj *>(rm.v.obj())->name)->str()).put("'");
        Value file;
        if (get(module_dict(rm.v), "__file__", file) == R::Ok && is_str(file))
            b.put(" (").put(str_of(file)->str()).put(")");
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
