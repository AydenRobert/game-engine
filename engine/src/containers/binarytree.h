#pragma once

#include "defines.h"

/**
 * @typedef binarytree
 * @brief The binary tree struct.
 *
 * NOTE: Does not support duplicate values.
 */
typedef struct binarytree {
    void *internal_state;
} binarytree;

/**
 * @brief Creates a binary tree struct, this function should be called twice,
 * the first time to get the memory requirement.
 *
 * @param max_nodes The maximum number of nodes allowed in the binary tree.
 * @param memory_requirement The memory requirement for allocation purposes.
 * @param memory The block of memory, or 0.
 * @param out_tree The struct to populate.
 * @return True if successful, otherwise False.
 */
b8 binarytree_create(u32 max_nodes, u64 *memory_requirement, void *memory,
                     binarytree *out_tree);

/**
 * @brief Destroys a binary tree struct.
 *
 * @param tree The binary tree struct.
 * @return The block of memory to be freed or reused.
 */
void *binarytree_destroy(binarytree *tree);

/**
 * @brief Inserts a node into the binary tree.
 *
 * @param tree The binary tree struct.
 * @param identifier The identifier.
 * @param index An index into an array, or other value needed.
 * @return True if successful, otherwise False.
 */
b8 binarytree_insert(binarytree *tree, u64 identifier, u32 index);

/**
 * @brief Finds a node from the binary tree.
 *
 * @param tree The binary tree struct.
 * @param identifier The identifier to use.
 * @param out_index The index/value to populate.
 * @return True if successful, otherwise False.
 */
b8 binarytree_search(binarytree *tree, u64 identifier, u32 *out_index);

/**
 * @brief Deletes a node from the binary tree.
 *
 * @param tree The binary tree struct.
 * @param identifier The identifier to use.
 * @param out_index The index/value to populate.
 * @return True if successful, otherwise False.
 */
b8 binarytree_delete(binarytree *tree, u64 identifier, u32 *out_index);
