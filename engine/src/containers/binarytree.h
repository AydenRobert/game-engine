#pragma once

#include "defines.h"

typedef struct binarytree {
    void *internal_state;
} binarytree;

typedef struct binarytree_config {
    u32 max_nodes;
    u64 value_size;
} binarytree_config;

b8 binarytree_create(binarytree_config config, u64 *memory_requirement, void *memory,
                     binarytree *out_tree);

void *binarytree_destroy(binarytree *tree);

b8 binarytree_insert(binarytree *tree, u64 identifier, void *value);
void *binarytree_search(binarytree *tree, u64 identifier);
void *binarytree_delete(binarytree *tree, u64 identifier);
