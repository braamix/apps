#include <coro.h>
#include <stdio.h>

struct node { int key; struct node *left, *right; };

coro(int) int walk(struct node *t)
{
    if (!t)
        return 0;
    int n = await walk(t->left);
    yield t->key;
    return n + 1 + await walk(t->right);
}

static struct node n1 = { 1 }, n3 = { 3 }, n5 = { 5 }, n7 = { 7 };
static struct node n2 = { 2, &n1, &n3 }, n6 = { 6, &n5, &n7 };
static struct node n4 = { 4, &n2, &n6 };

coro(braam_call *) int main(void)
{
    static _Alignas(16) char storage[1024];
    for (int k = 0; k < 2; k++) {
        co_frame(int, int) *w = co_init(storage, sizeof storage, walk, &n4);
        while (co_resume(w) == CO_SUSPENDED)
            await printf("%d ", co_value(w));
        await printf("= %d\n", co_result(w));
    }
    return 0;
}

