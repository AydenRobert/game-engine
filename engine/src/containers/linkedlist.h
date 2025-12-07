#pragma once

#include "defines.h"

typedef struct linkedlist {
    void *state;
} linkedlist;

typedef struct linkedlist_iterator {
    u64 index;
    void *current_node;
    linkedlist *list;
} linkedlist_iterator;

/**
 * @brief Creates a linked list struct. Each node stores a void pointer.
 *
 * @param max_nodes The maximum number of nodes for the list.
 * @param memory_requirement A pointer to store the memory requirement of the
 * linked list in bytes.
 * @param memory A pointer to the memory to store the linked list data.
 * @param out_list A pointer to the linkedlist struct.
 * @return True if successfully; otherwise False.
 */
b8 linkedlist_create(u64 max_nodes, u64 *memory_requirement, void *memory,
                     linkedlist *out_list);

/**
 * @brief Destroys a given linked list.
 *
 * @param list The linked list to destroy.
 * @return The block of memory that was for the linked list.
 */
void *linkedlist_destroy(linkedlist *list);

/**
 * @brief Resets the given linked list. Allows memory to easily be reused by a
 * 'new' linked list.
 *
 * @param list The list to reset.
 */
void linkedlist_reset(linkedlist *list);

/**
 * @brief Pushes a new node to the tail of the linked list.
 *
 * @param list The list to append.
 * @param data The pointer to hold in the node.
 * @return True if successfully; otherwise False.
 */
b8 linkedlist_push_tail(linkedlist *list, void *data);
/**
 * @brief Pushes a new node to the head of the linked list.
 *
 * @param list The list to append.
 * @param data The pointer to hold in the node.
 * @return True if successfully; otherwise False.
 */
b8 linkedlist_push_head(linkedlist *list, void *data);
/**
 * @brief Pops of a value off the end of the linked list.
 * NOTE: O(n)
 *
 * @param list The list to pop.
 * @return The pointer stored in the node.
 */
void *linkedlist_pop_tail(linkedlist *list);
/**
 * @brief Pops of a value off the start of the linked list.
 *
 * @param list The list to pop.
 * @return The pointer stored in the node.
 */
void *linkedlist_pop_head(linkedlist *list);

/**
 * @brief Inserts a node at a specific index.
 *
 * @param list The list to append.
 * @param index The index to add.
 * @param data The pointer to store in the node.
 * @return True if successfully; otherwise False.
 */
b8 linkedlist_insert_at(linkedlist *list, u64 index, void *data);
/**
 * @brief Removes a node at a specific index.
 *
 * @param list The list to use.
 * @param index The index to remove.
 * @return The pointer stored in the node.
 */
void *linkedlist_remove_at(linkedlist *list, u64 index);
/**
 * @brief Gets a value at a specific index.
 *
 * @param list The list to use.
 * @param index The index to remove.
 * @return The pointer stored in the node.
 */
void *linkedlist_get_at(linkedlist *list, u64 index);
/**
 * @brief Returns the current length of the list.
 *
 * @param list The list to use.
 * @return The current length.
 */
u64 linkedlist_length(linkedlist *list);

/**
 * @brief Populates an out_iterator struct.
 *
 * @param list The list to use.
 * @param out_iterator A pointer to an iterator struct.
 * @return True if successfully; otherwise False.
 */
b8 linkedlist_iterator_begin(linkedlist *list,
                             linkedlist_iterator *out_iterator);

/**
 * @brief Populates an out_iterator struct.
 *
 * @param list The list to use.
 * @param index The index to start the iterator at.
 * @param out_iterator A pointer to an iterator struct.
 * @return True if successfully; otherwise False.
 */
b8 linkedlist_iterator_begin_at(linkedlist *list, u64 index,
                                linkedlist_iterator *out_iterator);

/**
 * @brief Updates the iterator to get the node.
 *
 * @param iterator The iterator struct.
 * @return False if at the end of the list or the iterator is invalid; otherwise
 * True.
 */
b8 linkedlist_iterator_next(linkedlist_iterator *iterator);

/**
 * @brief Returns 0 or the value stored at a node.
 *
 * @param iterator The iterator struct.
 * @return The value stored in the node.
 */
void *linkedlist_iterator_get(linkedlist_iterator *iterator);

/**
 * @brief Returns 0 or the value stored at the current node. It then removes the
 * current node.
 *
 * @param iterator The iterator struct.
 * @return The value stored in the node.
 */
void *linkedlist_iterator_remove(linkedlist_iterator *iterator);
