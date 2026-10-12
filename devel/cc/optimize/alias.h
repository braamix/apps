#pragma once
#include "cfg.h"
#include "string_map.h"
#include "tac.h"

// Populates two freshly-initialised StringMaps for the function `fn`:
//   observable     — names a caller/callee can observe: every Var operand in the
//                    body that is neither a temporary (%N) nor one of the
//                    function's parameters or automatic locals. These are the
//                    globals / externs / static locals whose stores must be kept
//                    and whose copies a call may invalidate.
//   address_taken  — names of all Var operands of GET_ADDRESS instructions.
// `fn` is the function's own toplevel (for its params + locals); when NULL the
// observable set is left empty (no classification context — used by unit tests
// that build raw instruction lists). Caller must map_destroy() both maps.
void collect_alias_sets(const OptCfg *cfg, const Tac_TopLevel *fn, StringMap *observable,
                        StringMap *address_taken);
