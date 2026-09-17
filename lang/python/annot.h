// PEP 649: an annotation is evaluated when something asks for it.
//
// The compiler makes an __annotate__ out of the annotations of a def, a class
// body or a module. __annotations__ is what calling it with Format.VALUE
// answers, kept once it has. The three owners keep the pair differently: a
// function in fields of its own, a class in __annotate_func__ and
// __annotations_cache__, a module under the two plain names in its dict.
#pragma once

#include "type.h"

// The format numbers annotationlib.Format spells out. The compiler's own
// __annotate__ answers VALUE and nothing else.
enum : i32 {
    ANN_VALUE      = 1,
    ANN_FAKEGLOBAL = 2,
    ANN_FORWARDREF = 3,
    ANN_STRING     = 4,
};

// `v.__annotations__` and `v.__annotate__`, for a function, a class or a
// module. Got::Call hands back the call that evaluates them; Got::Missing
// means the name is not one of these two.
Got annot_lazy(Value v, StrObj *name, Value &out, Value &args);

// The same without the call: the cache if there is one, an empty dict where
// there is nothing to evaluate, and Nil with no error pending when only a
// call would answer.
Value annot_cached(Value v);

// The __annotate__ of `v`, or None. Nil leaves an error pending.
Value annot_func(Value v);

// `v.__annotations__ = d` and `v.__annotate__ = f`. R::NotImpl where the name
// is neither.
R annot_store(Value v, StrObj *name, Value val);

// The descriptors `type` keeps in its own namespace. annotationlib reaches
// for `type.__dict__["__annotations__"].__get__`, not for the attribute.
bool annot_install();
