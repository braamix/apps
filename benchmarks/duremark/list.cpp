/*
 * DureMark Linked List Workload
 * Simplified linked list operations: find, reverse, bubble sort
 */
#include "duremark.h"

static du_list_node mem_list[32];

/* Function: du_list_find
 * Find a node in the list by index
 */
static du_list_node *du_list_find(du_list_node *list, uint16_t idx)
{
    while (list != NULL) {
        if (list->idx == idx) {
            return list;
        }
        list = list->next;
    }
    return NULL;
}

/* Function: du_list_reverse
 * Reverse the linked list
 */
static du_list_node *du_list_reverse(du_list_node *list)
{
    du_list_node *prev = NULL;

    while (list != NULL) {
        du_list_node *tmp = list->next;
        list->next        = prev;
        prev              = list;
        list              = tmp;
    }
    return prev;
}

/* Function: du_list_bubble_sort
 * Simple bubble sort by data value
 */
static du_list_node *du_list_bubble_sort(du_list_node *list)
{
    int swapped;

    if (list == NULL || list->next == NULL) {
        return list;
    }
    swapped = 1;
    while (swapped) {
        du_list_node *current = list;
        swapped               = 0;

        while (current != NULL && current->next != NULL) {
            du_list_node *next = current->next;

            if (current->data > next->data) {
                /* Swap data */
                uint16_t tmp_data = current->data;
                uint16_t tmp_idx  = current->idx;
                current->data     = next->data;
                current->idx      = next->idx;
                next->data        = tmp_data;
                next->idx         = tmp_idx;
                swapped           = 1;
            }
            current = next;
        }
    }
    return list;
}

/* Function: du_list_sort_by_idx
 * Sort list back to original order by index
 */
static du_list_node *du_list_sort_by_idx(du_list_node *list)
{
    int swapped;

    if (list == NULL || list->next == NULL) {
        return list;
    }
    swapped = 1;
    while (swapped) {
        du_list_node *current = list;
        swapped               = 0;

        while (current != NULL && current->next != NULL) {
            du_list_node *next = current->next;

            if (current->idx > next->idx) {
                /* Swap data */
                uint16_t tmp_data = current->data;
                uint16_t tmp_idx  = current->idx;
                current->data     = next->data;
                current->idx      = next->idx;
                next->data        = tmp_data;
                next->idx         = tmp_idx;
                swapped           = 1;
            }
            current = next;
        }
    }
    return list;
}

/* Function: du_list_init
 * Initialize a linked list from memory block
 */
du_list_node *du_list_init()
{
    unsigned size = sizeof(mem_list) / sizeof(mem_list[0]);
    unsigned i;
    du_list_node *prev = NULL;

    /* Initialize list */
    for (i = 0; i < size; i++) {
        du_list_node *current = &mem_list[i];

        current->data = ((~i & 0xF) << 4) | (i & 0xF);
        current->idx  = i;
        current->next = NULL;

        if (prev != NULL) {
            prev->next = current;
        }
        prev = current;
    }
    return mem_list;
}

/* Function: du_bench_list
 * Benchmark linked list operations
 */
void du_bench_list(du_results *res, int16_t finder_idx)
{
    du_list_node *list = res->list;
    unsigned find_num  = 20;
    unsigned i;

    /* Find operations */
    for (i = 0; i < find_num; i++) {
        (void)du_list_find(list, (uint16_t)(finder_idx + i));
        list = du_list_reverse(list);
    }

    /* Sort by data */
    list = du_list_bubble_sort(list);

    /* Restore original order */
    res->list = du_list_sort_by_idx(list);
}
