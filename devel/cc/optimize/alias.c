// ============================================================================
// alias.c — alias pre-analysis shared by copy propagation and dead-store
// elimination.
//
// Both passes propagate facts about a variable's value, which is only sound
// while the variable cannot be changed by something we are not tracking. Two
// categories of variables can be, and so must be handled conservatively:
//
//   - Observable variables: anything with static storage duration (file-scope
//     globals, `extern`s, local `static`s). Any function call might read or
//     write them, and their final value is visible after we return.
//   - Address-taken variables: any variable whose address is taken with
//     GET_ADDRESS may be read or written through the resulting pointer.
//
// We do not carry the whole program's global list to each function. Instead we
// classify *locally*: in TAC a local and a global are both a bare name, but a
// name is observable exactly when it is neither a temporary (%N, always
// compiler-generated and private) nor one of this function's parameters or
// automatic locals. The translator records those on the function toplevel; the
// no-shadowing rule makes the classification unambiguous program-wide.
//
// See docs/TAC_Optimization.md §"Conservatism around aliased variables".
// ============================================================================

#include "alias.h"

#include <ctype.h>

#include "optimize.h"

// Compiler temporaries are named "%N" (percent + digit) and are always private
// to the function. Named locals and params are "%name" (percent + letter/
// underscore) and are caught by the params/locals private-set instead.
static bool is_temp_name(const char *n)
{
    return n && n[0] == '%' && isdigit((unsigned char)n[1]);
}

// Insert `name` into `observable` unless it is a temporary or one of the
// function's private (param/local) names. The map copies the key; the value is
// an unused placeholder (see collect_alias_sets).
static void note_name(StringMap *observable, const StringMap *private_set, const char *name)
{
    if (!name || is_temp_name(name))
        return;
    if (map_get((StringMap *)private_set, name, NULL))
        return;
    map_insert(observable, name, 1, 0);
}

// Note every Var in a value list (covers FUN_CALL argument lists too).
static void note_vals(StringMap *observable, const StringMap *private_set, const Tac_Val *v)
{
    for (; v; v = v->next)
        if (v->kind == TAC_VAL_VAR)
            note_name(observable, private_set, v->u.var_name);
}

// Walk every Var operand of one instruction, recording the observable ones.
// COPY_TO_OFFSET.dst and COPY_FROM_OFFSET.src are bare names (char*), not
// Tac_Val*, and may also denote a global struct/array — note them directly.
static void note_instr(StringMap *observable, const StringMap *private_set,
                       const Tac_Instruction *ins)
{
    switch (ins->kind) {
    case TAC_INSTRUCTION_RETURN:
        note_vals(observable, private_set, ins->u.return_.src);
        break;
    // Every type-conversion instruction follows the {src, dst} layout.
    case TAC_INSTRUCTION_SIGN_EXTEND:
    case TAC_INSTRUCTION_TRUNCATE:
    case TAC_INSTRUCTION_ZERO_EXTEND:
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_INT_TO_FLOAT:
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_INT:
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_PTR_TO_CHAR_PTR:
    case TAC_INSTRUCTION_CHAR_PTR_TO_PTR:
        note_vals(observable, private_set, ins->u.sign_extend.src);
        note_vals(observable, private_set, ins->u.sign_extend.dst);
        break;
    case TAC_INSTRUCTION_UNARY:
        note_vals(observable, private_set, ins->u.unary.src);
        note_vals(observable, private_set, ins->u.unary.dst);
        break;
    case TAC_INSTRUCTION_BINARY:
        note_vals(observable, private_set, ins->u.binary.src1);
        note_vals(observable, private_set, ins->u.binary.src2);
        note_vals(observable, private_set, ins->u.binary.dst);
        break;
    case TAC_INSTRUCTION_COPY:
        note_vals(observable, private_set, ins->u.copy.src);
        note_vals(observable, private_set, ins->u.copy.dst);
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        note_vals(observable, private_set, ins->u.get_address.src);
        note_vals(observable, private_set, ins->u.get_address.dst);
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        note_vals(observable, private_set, ins->u.load.src_ptr);
        note_vals(observable, private_set, ins->u.load.dst);
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        note_vals(observable, private_set, ins->u.store.src);
        note_vals(observable, private_set, ins->u.store.dst_ptr);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        note_vals(observable, private_set, ins->u.add_ptr.ptr);
        note_vals(observable, private_set, ins->u.add_ptr.index);
        note_vals(observable, private_set, ins->u.add_ptr.dst);
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        note_vals(observable, private_set, ins->u.ptr_diff.ptr_a);
        note_vals(observable, private_set, ins->u.ptr_diff.ptr_b);
        note_vals(observable, private_set, ins->u.ptr_diff.dst);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        note_vals(observable, private_set, ins->u.copy_to_offset.src);
        note_name(observable, private_set, ins->u.copy_to_offset.dst);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        note_name(observable, private_set, ins->u.copy_from_offset.src);
        note_vals(observable, private_set, ins->u.copy_from_offset.dst);
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
        note_vals(observable, private_set, ins->u.jump_if_zero.condition);
        break;
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        note_vals(observable, private_set, ins->u.jump_if_not_zero.condition);
        break;
    case TAC_INSTRUCTION_JUMP_TABLE:
        note_vals(observable, private_set, ins->u.jump_table.index);
        break;
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        // fun_name is a function symbol, not a data variable — ignore it.
        note_vals(observable, private_set, ins->u.fun_call.args);
        note_vals(observable, private_set, ins->u.fun_call.dst);
        break;
    case TAC_INSTRUCTION_JUMP:
    case TAC_INSTRUCTION_LABEL:
        break;
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        // No Tac_Val operands; the name is a private local, not observable storage.
        break;
    }
}

// Populate observable and address_taken (both freshly initialised here). Both
// are key-only sets: membership is the whole fact, and the stored value is an
// unused placeholder. That matters — a caller may free the TAC instructions
// these names came from while still consulting the sets (dead-store elimination
// does exactly that), and StringMap owns its keys, so nothing can dangle.
void collect_alias_sets(const OptCfg *cfg, const Tac_TopLevel *fn, StringMap *observable,
                        StringMap *address_taken)
{
    map_init(observable);
    map_init(address_taken);

    // Build the private set (params ∪ automatic locals) for this function. With
    // no function context (NULL, or a non-function toplevel) we cannot classify,
    // so the observable set is left empty — matching the conservative-free
    // behaviour unit tests rely on.
    StringMap private_set;
    map_init(&private_set);
    if (fn && fn->kind == TAC_TOPLEVEL_FUNCTION) {
        for (const Tac_Param *p = fn->u.function.params; p; p = p->next)
            if (p->name)
                map_insert(&private_set, p->name, (intptr_t)p->name, 0);
        for (const Tac_Param *p = fn->u.function.locals; p; p = p->next)
            if (p->name)
                map_insert(&private_set, p->name, (intptr_t)p->name, 0);

        for (int i = 0; i < cfg->nblocks; i++) {
            const OptBlock *b = cfg->blocks[i];
            for (const Tac_Instruction *ins = b->first; ins; ins = ins->next)
                note_instr(observable, &private_set, ins);
        }
    } else {
        OPT_TRACE("[alias] no function context: observable empty\n");
    }
    map_destroy(&private_set);

    // Address-taken variables: every Var that is the source of a GET_ADDRESS.
    int n_taken = 0;
    for (int i = 0; i < cfg->nblocks; i++) {
        const OptBlock *b = cfg->blocks[i];
        for (const Tac_Instruction *ins = b->first; ins; ins = ins->next) {
            if (ins->kind == TAC_INSTRUCTION_GET_ADDRESS ||
                ins->kind == TAC_INSTRUCTION_GET_ADDRESS_BYTE ||
                ins->kind == TAC_INSTRUCTION_GET_ADDRESS_DECAY) {
                const Tac_Val *src = ins->u.get_address.src;
                if (src->kind == TAC_VAL_VAR) {
                    const char *n = src->u.var_name;
                    map_insert(address_taken, n, 1, 0);
                    OPT_TRACE("[alias] address-taken: %s\n", n);
                    n_taken++;
                }
            }
        }
    }
    OPT_TRACE("[alias] address_taken: %d entries\n", n_taken);
}
