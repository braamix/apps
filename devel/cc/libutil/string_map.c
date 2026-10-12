//
// Implementation of string-to-int map using an AVL tree, with self-balancing.
//
// Each `StringNode` includes a `height` field to track subtree height,
// used to compute balance factors.
//
// Balancing:
//  - The `balance_factor` is calculated as `height(left) - height(right)`.
//  - After insertions and deletions, the `balance` function checks
//    if the balance factor is outside [-1, 1].
//  - Four cases are handled: Left-Left, Left-Right, Right-Right,
//    Right-Left, using left and right rotations.
//  - Rotations (`rotate_left`, `rotate_right`) adjust the tree structure
//    while preserving BST properties.
//
// Copyright (c) 2025 Serge Vakulenko
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#include "string_map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xalloc.h"

// Get the height of a node (0 for NULL)
int map_node_height(const StringNode *node)
{
    return node ? node->height : 0;
}

// Update the height of a node
static void update_height(StringNode *node)
{
    if (node) {
        int left_height  = map_node_height(node->left);
        int right_height = map_node_height(node->right);

        node->height = 1 + (left_height > right_height ? left_height : right_height);
    }
}

// Get the balance factor of a node
static int balance_factor(const StringNode *node)
{
    return node ? map_node_height(node->left) - map_node_height(node->right) : 0;
}

// Right rotation
static StringNode *rotate_right(StringNode *y)
{
    StringNode *x  = y->left;
    StringNode *T2 = x->right;
    x->right       = y;
    y->left        = T2;
    update_height(y);
    update_height(x);
    return x;
}

// Left rotation
static StringNode *rotate_left(StringNode *x)
{
    StringNode *y  = x->right;
    StringNode *T2 = y->left;
    y->left        = x;
    x->right       = T2;
    update_height(x);
    update_height(y);
    return y;
}

// Re-balance a node
static StringNode *rebalance(StringNode *node)
{
    if (!node)
        return node;

    update_height(node);
    int bf = balance_factor(node);

    // Left heavy
    if (bf > 1) {
        if (balance_factor(node->left) < 0) {
            node->left = rotate_left(node->left); // Left-Right case
        }
        return rotate_right(node); // Left-Left case
    }
    // Right heavy
    if (bf < -1) {
        if (balance_factor(node->right) > 0) {
            node->right = rotate_right(node->right); // Right-Left case
        }
        return rotate_left(node); // Right-Right case
    }
    return node;
}

// Helper function to rebalance the tree
static StringNode *rebalance_tree(StringNode *node)
{
    if (!node) {
        return NULL;
    }

    // Post-order: rebalance children first
    node->left  = rebalance_tree(node->left);
    node->right = rebalance_tree(node->right);

    // rebalance() may rotate, changing the subtree root, so return its result
    // rather than the original node (otherwise the new root is orphaned).
    return rebalance(node);
}

// Initialize the map
void map_init(StringMap *map)
{
    map->root = NULL;
}

// Create a new node
static StringNode *create_node(const char *key, intptr_t value, int level)
{
    StringNode *node =
        (StringNode *)xalloc(sizeof(StringNode) + strlen(key), __func__, __FILE__, __LINE__);
    if (!node)
        return NULL;
    strcpy(node->key, key);
    node->value  = value;
    node->level  = level;
    node->left   = NULL;
    node->right  = NULL;
    node->height = 1;
    return node;
}

// Insert or update a key-value pair
static StringNode *insert_node(StringNode *node, const char *key, intptr_t value, int level,
                               void (*dealloc)(intptr_t value))
{
    if (!node) {
        return create_node(key, value, level);
    }

    int cmp = strcmp(key, node->key);
    if (cmp == 0) {
        if (dealloc) {
            dealloc(node->value);
        }
        node->value = value; // Update value
        node->level = level; // Update level
        return node;
    } else if (cmp < 0) {
        node->left = insert_node(node->left, key, value, level, dealloc);
    } else {
        node->right = insert_node(node->right, key, value, level, dealloc);
    }

    return rebalance(node);
}

void map_insert(StringMap *map, const char *key, intptr_t value, int level)
{
    if (!map || !key)
        return;
    map->root = insert_node(map->root, key, value, level, NULL);
}

void map_insert_free(StringMap *map, const char *key, intptr_t value, int level,
                     void (*dealloc)(intptr_t value))
{
    if (!map || !key)
        return;
    map->root = insert_node(map->root, key, value, level, dealloc);
}

// Get value by key, returns 1 if found, 0 if not
bool map_get_level(const StringMap *map, const char *key, int *level)
{
    const StringNode *current = map && key ? map->root : NULL;
    while (current) {
        int cmp = strcmp(key, current->key);
        if (cmp == 0) {
            *level = current->level;
            return true;
        }
        current = cmp < 0 ? current->left : current->right;
    }
    return false;
}

bool map_get(const StringMap *map, const char *key, intptr_t *value)
{
    if (!map || !key)
        return false;

    const StringNode *current = map->root;
    while (current) {
        int cmp = strcmp(key, current->key);
        if (cmp == 0) {
            if (value) {
                *value = current->value;
            }
            return true;
        }
        if (cmp < 0) {
            current = current->left;
        } else {
            current = current->right;
        }
    }
    return false;
}

// Helper function to find node with minimum key and its parent
static StringNode *min_node_with_parent(StringNode *node, StringNode **parent)
{
    *parent             = NULL;
    StringNode *current = node;
    while (current->left) {
        *parent = current;
        current = current->left;
    }
    return current;
}

// Helper function to remove a single node without balancing
static StringNode *remove_single_node(StringNode *node)
{
    // Node with only one child or no child
    if (!node->left) {
        StringNode *temp = node->right;
        xfree(node);
        return temp;
    } else if (!node->right) {
        StringNode *temp = node->left;
        xfree(node);
        return temp;
    }

    // Node with two children
    StringNode *parent_of_successor;
    StringNode *successor = min_node_with_parent(node->right, &parent_of_successor);

    // Unlink successor from its current position
    if (parent_of_successor) {
        parent_of_successor->left = successor->right;
    } else {
        // Successor is the right child of node
        node->right = successor->right;
    }

    // Replace node with successor
    successor->left  = node->left;
    successor->right = node->right;
    update_height(successor);

    // Free the node
    xfree(node);
    return successor;
}

// Remove a node, returns the new root of the subtree
static StringNode *remove_node(StringNode *node, const char *key)
{
    if (!node)
        return NULL;

    int cmp = strcmp(key, node->key);
    if (cmp < 0) {
        node->left = remove_node(node->left, key);
    } else if (cmp > 0) {
        node->right = remove_node(node->right, key);
    } else {
        node = remove_single_node(node);
    }

    return rebalance(node);
}

// Helper function for node removal by level
static StringNode *remove_node_level(StringNode *node, int level, void (*dealloc)(intptr_t value))
{
    if (!node) {
        return NULL;
    }

    // Post-order traversal: process left and right subtrees first
    node->left  = remove_node_level(node->left, level, dealloc);
    node->right = remove_node_level(node->right, level, dealloc);

    // Check level for current node
    if (node->level > level) {
        if (dealloc) {
            // Save value before remove_single_node() frees the node struct.
            // In the two-children case the successor is promoted in-place,
            // so node->value is gone after the call.
            intptr_t saved_value = node->value;
            node                 = remove_single_node(node);
            dealloc(saved_value);
        } else {
            node = remove_single_node(node);
        }
        if (!node) {
            return NULL; // Node was removed and had no children
        }
    }

    // Update height but defer balancing
    update_height(node);
    return node;
}

void map_remove_key(StringMap *map, const char *key)
{
    if (map && key) {
        map->root = remove_node(map->root, key);
    }
}

void map_remove_level(StringMap *map, int level)
{
    if (!map) {
        return;
    }
    // Phase 1: Remove nodes without balancing
    map->root = remove_node_level(map->root, level, NULL);

    // Phase 2: Rebalance the entire tree
    map->root = rebalance_tree(map->root);
}

void map_remove_level_free(StringMap *map, int level, void (*dealloc)(intptr_t value))
{
    if (!map) {
        return;
    }
    // Phase 1: Remove nodes without balancing, invoking dealloc on each value
    map->root = remove_node_level(map->root, level, dealloc);

    // Phase 2: Rebalance the entire tree
    map->root = rebalance_tree(map->root);
}

static void free_nodes(StringNode *node, void (*dealloc)(intptr_t value))
{
    if (!node)
        return;
    free_nodes(node->left, dealloc);
    free_nodes(node->right, dealloc);
    if (dealloc) {
        dealloc(node->value);
    }
    xfree(node);
}

// Free the map and all its nodes
void map_destroy(StringMap *map)
{
    free_nodes(map->root, NULL);
    map->root = NULL;
}

// Free the map and all its nodes
void map_destroy_free(StringMap *map, void (*dealloc)(intptr_t value))
{
    free_nodes(map->root, dealloc);
    map->root = NULL;
}

static void iterate_nodes(StringNode *node,
                          void (*func)(const char *key, intptr_t value, const void *arg),
                          const void *arg)
{
    if (!node)
        return;
    iterate_nodes(node->left, func, arg);
    func(node->key, node->value, arg);
    iterate_nodes(node->right, func, arg);
}

//
// Iterate and invoke callback for each node.
//
void map_iterate(StringMap *map, void (*func)(const char *key, intptr_t value, const void *arg),
                 const void *arg)
{
    iterate_nodes(map->root, func, arg);
}
