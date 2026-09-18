// `select`: the floor under selectors, and so under subprocess.
//
// Braam has no call that waits for a descriptor to be ready, so select() can
// only answer for what is always ready: a regular file. Anything else -- a
// pipe, the terminal -- is an OSError. poll, epoll and kqueue are not here,
// which is what makes selectors choose SelectSelector.
#include "builtin.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "module.h"
#include "ops.h"
#include "posix.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// s[0] the three lists as given, s[1] every object in them in turn, s[2] their
// descriptors so far; j the next object; i 0 while the descriptors are found,
// then 1 while each is asked about.
R select_step(ContObj *k, Value in)
{
    ListObj *all = list_of(k->s[1]);
    ListObj *fds = list_of(k->s[2]);
    u32 phase    = k->i & ~SYS_TURN_BITS;
    if (phase == 0) {
        if (k->x[0]) {
            // Back from fileno().
            k->x[0] = 0;
            i64 n   = 0;
            if (!as_int_arg(in, n))
                return err_set("TypeError", "fileno() returned a non-integer");
            if (n < 0)
                return err_set("ValueError", "file descriptor cannot be a negative integer");
            if (!list_push(list_of(k->s[2]), in))
                return oom();
        }
        while (k->j < all->items.size()) {
            Value o = all->items[k->j++];
            i64 n   = 0;
            if (o.is_int() || is_bool(o)) {
                as_int_arg(o, n);
                if (n < 0)
                    return err_set("ValueError", "file descriptor cannot be a negative integer");
                if (!list_push(list_of(k->s[2]), o))
                    return oom();
                continue;
            }
            k->x[0] = 1;
            return cont_method(k, o, "fileno");
        }
        k->i = 1;
        k->j = 0;
    }
    fds = list_of(k->s[2]);
    while (k->j < fds->items.size()) {
        SysReq q;
        q.op = SysOp::FStat;
        q.fd = fds->items[k->j].as_int();
        R r;
        if (!sys_turn(k, q, r))
            return r;
        k->j++;
    }
    // Every one is a file, and a file is always ready to read and to write.
    TupleObj *lists = static_cast<TupleObj *>(k->s[0].obj());
    TupleObj *t     = tuple_new(3);
    if (!t)
        return oom();
    Root rt{ obj_value(t) };
    for (u32 n = 0; n < 3; n++) {
        ListObj *l = n < 2 ? py_list_of(lists->items()[n]) : list_new();
        if (!l)
            return R::Err;
        lists                                           = static_cast<TupleObj *>(k->s[0].obj());
        static_cast<TupleObj *>(rt.v.obj())->items()[n] = obj_value(l);
    }
    return cont_done(k, rt.v);
}

R s_select(const CallArgs &a, Value &out)
{
    if (!args_only(a, "select", 3, 4))
        return R::Err;
    if (a.nargs == 4 && !is_none(a.args[3])) {
        f64 t = 0;
        if (!as_number(a.args[3], t))
            return err_set("TypeError", "timeout must be an integer or None");
        if (t < 0)
            return err_set("ValueError", "timeout must be non-negative");
    }
    TupleObj *lists = tuple_new(3);
    if (!lists)
        return oom();
    Root rl{ obj_value(lists) };
    ListObj *all = list_new();
    if (!all)
        return oom();
    Root ra{ obj_value(all) };
    for (u32 n = 0; n < 3; n++) {
        ListObj *l = py_list_of(a.args[n]);
        if (!l)
            return R::Err;
        static_cast<TupleObj *>(rl.v.obj())->items()[n] = obj_value(l);
        for (Value o : l->items)
            if (!list_push(list_of(ra.v), o))
                return oom();
    }
    ListObj *fds = list_new();
    if (!fds)
        return oom();
    Root rf{ obj_value(fds) };
    Root kv{ cont_new(select_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rl.v;
    k->s[1]    = ra.v;
    k->s[2]    = rf.v;
    out        = kv.v;
    return R::Ok;
}

constexpr ModDef DEFS[] = {
    { "select", s_select },
};

} // namespace

bool select_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_defs(d, DEFS) || !mod_int(d, "PIPE_BUF", 4096))
        return false;
    // select.error is OSError, as it has been since 3.3.
    Value e;
    StrObj *n = str_intern("OSError");
    if (!n || dict_get(builtins_dict(), obj_value(n), e) != R::Ok)
        return false;
    return mod_put(d, "error", e);
}
