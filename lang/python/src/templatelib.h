// PEP 750's t-strings: what `t"..."` makes, and the Interpolations in it.
//
// Both are `string.templatelib`'s, whose Python half takes them as
// `type(t"{0}")`; that module is library, and arrives with it.
#pragma once

#include "obj.h"

extern const Type template_type;
extern const Type interpolation_type;

// BuildTemplate: a Template over a tuple of strings, one more than the tuple
// of Interpolations.
Value template_build(Value strings, Value interps);

// BuildInterpolation: `conv` is 0 or 's', 'r' or 'a', and `spec` a str.
Value interp_build(Value value, Value expr, u32 conv, Value spec);

// The constructors, the methods and __match_args__, from methods_install().
bool templatelib_methods();
