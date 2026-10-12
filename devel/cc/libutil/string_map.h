#pragma once
//
// Implementation of string-to-int map using an AVL tree, with self-balancing.
//
// Time Complexity: All operations (insert, get, remove) are O(log n)
// due to the height balance guarantee (tree height is at most 1.44 log(n)).
//
// Memory: It dynamically allocates memory for keys and nodes, with proper cleanup.
//
// To use this string-to-int map:
//  0. Allocate a map with `StringMap map;`.
//  1. Initialize with `map_init(&map)`.
//  2. Insert key-value pairs with `map_insert(&map, key, value)`.
//  3. Retrieve values with `map_get(&map, key, &value)`.
//  4. Remove entries with `map_remove_key(&map, key)`.
//  5. Free the map with `map_destroy(&map)`.
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
#include <stdbool.h>
#include <stdint.h>

// Structure for each node in the AVL tree
typedef struct StringNode {
    struct StringNode *left;
    struct StringNode *right;
    int height;
    intptr_t value; // large enough to hold a pointer
    int level;
    char key[1]; // Dynamically sized
} StringNode;

typedef struct StringMap {
    StringNode *root;
} StringMap;

//
// Initialize the string-to-int map
//
void map_init(StringMap *map);

//
// Insert: Recursively inserts or updates a key-value pair,
// then balances the tree on the way back up the recursion stack.
//
void map_insert(StringMap *map, const char *key, intptr_t value, int level);
void map_insert_free(StringMap *map, const char *key, intptr_t value, int level,
                     void (*dealloc)(intptr_t value));

//
// Get: Retrieves the value for a given key by traversing the tree
// based on `strcmp`. Returns true if found.
// Value pointer can be NULL if value is not needed.
//
bool map_get(const StringMap *map, const char *key, intptr_t *value);

//
// Get the level a key was inserted at: true if found.
//
bool map_get_level(const StringMap *map, const char *key, int *level);

//
// Remove: Handles three cases: leaf node, node with one child,
// and node with two children (using the minimum key in the right
// subtree as the successor), and balances the tree after deletion.
//
void map_remove_key(StringMap *map, const char *key);

//
// Remove beyond level: remove nodes which exceed given level.
//
void map_remove_level(StringMap *map, int level);
void map_remove_level_free(StringMap *map, int level, void (*dealloc)(intptr_t value));

//
// Free: Recursively frees all nodes and their keys.
//
void map_destroy(StringMap *map);
void map_destroy_free(StringMap *map, void (*dealloc)(intptr_t value));

//
// Get the height of a node (0 for NULL)
//
int map_node_height(const StringNode *node);

//
// Iterate and invoke callback for each node, in ascending key order.
// The key belongs to the map and stays valid until the entry is removed.
//
void map_iterate(StringMap *map, void (*func)(const char *key, intptr_t value, const void *arg),
                 const void *arg);
