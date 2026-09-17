// complex: a pair of f64, and the only type in the tower that has no order.
#pragma once

#include "obj.h"

struct ComplexObj : Obj {
    f64 re, im;
};

extern const Type complex_type;

inline bool is_complex(Value v)
{
    return v.is_obj() && v.obj()->type == &complex_type;
}

inline ComplexObj *complex_of(Value v)
{
    return static_cast<ComplexObj *>(v.obj());
}

Value complex_new(f64 re, f64 im);

R complex_negate(Value v, Value &out);
R complex_abs(Value v, Value &out);

// complex("1+2j"): the repr's own grammar, with no space inside it. False on
// anything else, the caller raising.
bool complex_parse(Str s, f64 &re, f64 &im);

bool complex_methods();
