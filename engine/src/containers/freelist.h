/**
 * @file freelist.h
 * @brief this file contains a free list, used for dynamic allocation tracking.
 * @version 0.1
 * @date 2025-30-11
 */

#pragma once

#include "defines.h"

/**
 * @typedef freelist
 * @brief A data structure to be used alongside an allocator for dynamic memory
 * allocation. Tracks free ranges of memory.
 */
typedef struct freelist {
    /** @brief contains the internal state of the freelist */
    void *memory;
} freelist;

/**
 * @brief Creates a new freelist or gets the memory requirement for one. Call
 * twice; first passing 0 to memory to obtain the memory requirement, second to
 * pass the allocated block.
 *
 * @param total_size Size that the free list should track, in bytes.
 * @param memory_requirement A pointer to get the memory requirement for the
 * free_list structure itself.
 * @param memory 0, or a pre-allocated block of memory for the free-list to use.
 * @param out_list A pointer to hold the free list.
 */
KAPI void freelist_create(u64 total_size, u64 *memory_requirement, void *memory,
                          freelist *out_list);

/**
 * @brief Destroys the provided freelist.
 *
 * @param list The list to be destroyed.
 */
KAPI void freelist_destroy(freelist *list);

/**
 * @brief Attempts to find a allocate block of memory given the size.
 *
 * @param list The freelist struct.
 * @param size The size to allocate.
 * @param out_offset A pointer to write the offset of the allocation.
 * @return True if successful; otherwise False.
 */
KAPI b8 freelist_allocate_block(freelist *list, u64 size, u64 *out_offset);

/**
 * @brief Attempts to find a free block of memory given the size.
 *
 * @param list The freelist struct.
 * @param size The size to free.
 * @param out_offset The offset of the allocation to free.
 * @return True if successful; otherwise False.
 */
KAPI b8 freelist_free_block(freelist *list, u64 size, u64 offset);

/**
 * @brief Clears the provided freelist.
 *
 * @param list The list to be cleared.
 */
KAPI void freelist_clear(freelist *list);

/**
 * @brief Obtains the amount of free space within the freelist.
 *
 * @param list The freelist struct.
 * @return The amount of free space.
 */
KAPI u64 freelist_free_space(freelist *list);
