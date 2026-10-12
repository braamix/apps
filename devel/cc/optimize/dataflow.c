// ============================================================================
// dataflow.c — scaffolding shared by the forward dataflow passes.
// ============================================================================

#include "dataflow.h"

#include "xalloc.h"

void keybuf_push(KeyBuf *kb, char *key)
{
    if (kb->count == kb->cap) {
        int new_cap     = kb->cap ? kb->cap * 2 : 8;
        char **new_keys = xalloc(new_cap * sizeof(char *), __func__, __FILE__, __LINE__);
        for (int i = 0; i < kb->count; i++)
            new_keys[i] = kb->keys[i];
        xfree(kb->keys);
        kb->keys = new_keys;
        kb->cap  = new_cap;
    }
    kb->keys[kb->count++] = key;
}

void keybuf_flush(KeyBuf *kb, StringMap *map, void (*free_value)(intptr_t))
{
    for (int i = 0; i < kb->count; i++) {
        intptr_t old_val = 0;
        map_get(map, kb->keys[i], &old_val);
        map_remove_key(map, kb->keys[i]);
        if (old_val)
            free_value(old_val);
    }
    xfree(kb->keys);
    kb->keys  = NULL;
    kb->count = kb->cap = 0;
}

void opt_preds_build(OptPreds *p, const OptCfg *cfg)
{
    int n     = cfg->nblocks;
    p->n      = n;
    p->npreds = xalloc(n * sizeof(int), __func__, __FILE__, __LINE__);
    p->preds  = xalloc(n * sizeof(int *), __func__, __FILE__, __LINE__);

    // First count the predecessors, then fill them in.
    for (int i = 0; i < n; i++)
        p->npreds[i] = 0;
    for (int i = 0; i < n; i++) {
        const OptBlock *b = cfg->blocks[i];
        for (int j = 0; j < b->nsucc; j++)
            p->npreds[b->succs[j]->id]++;
    }
    for (int i = 0; i < n; i++) {
        p->preds[i] =
            p->npreds[i] ? xalloc(p->npreds[i] * sizeof(int), __func__, __FILE__, __LINE__) : NULL;
        p->npreds[i] = 0;
    }
    for (int i = 0; i < n; i++) {
        const OptBlock *b = cfg->blocks[i];
        for (int j = 0; j < b->nsucc; j++) {
            int sid                         = b->succs[j]->id;
            p->preds[sid][p->npreds[sid]++] = i;
        }
    }
}

void opt_preds_free(OptPreds *p)
{
    for (int i = 0; i < p->n; i++)
        xfree(p->preds[i]);
    xfree(p->preds);
    xfree(p->npreds);
}

const Tac_Val *opt_defining_dst(const Tac_Instruction *ins)
{
    switch (ins->kind) {
    case TAC_INSTRUCTION_UNARY:
        return ins->u.unary.dst;
    case TAC_INSTRUCTION_BINARY:
        return ins->u.binary.dst;
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
        return ins->u.sign_extend.dst; // every conversion shares this layout
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        return ins->u.get_address.dst;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        return ins->u.load.dst;
    case TAC_INSTRUCTION_ADD_PTR:
        return ins->u.add_ptr.dst;
    case TAC_INSTRUCTION_PTR_DIFF:
        return ins->u.ptr_diff.dst;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        return ins->u.copy_from_offset.dst;
    default:
        return NULL;
    }
}
