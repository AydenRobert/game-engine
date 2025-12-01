/**
 * @file linear_allocator.h
 * @brief An implementation of a linear allocator
 * @version 1.1
 * @date 2025-12-01
 */

#pragma once

#include "defines.h"

typedef struct linear_allocator {
    void *memory;
} linear_allocator;

/**
 * @brief Creates a linear_allocator struct. Use twice; first to get memory
 * allocation requirement, second to create struct.
 *
 * @param total_size Size of memory to manage.
 * @param memory_requirment A pointer to store the total memory needed.
 * @param memory A pointer to the memory, or 0.
 * @param out_allocator A pointer to the allocator.
 */
KAPI void linear_allocator_create(u64 total_size, u64 *memory_requirement,
                                  void *memory,
                                  linear_allocator *out_allocator);

/**
 * @brief Destroys the linear allocator struct. Does not own memory, and does
 * not destroy memory.
 *
 * @param allocator A pointer to the allocator struct.
 */
KAPI void linear_allocator_destroy(linear_allocator *allocator);

/**
 * @brief Allocates memory within the allocator's block of memory with padding
 * to allow for alignment.
 *
 * @param allocator A pointer to the allocator struct.
 * @param size The size of the memory wanting to be allocated.
 * @param alignment What to align on, has to be a power of 2. Cannot be zero.
 */
KAPI void *linear_allocator_allocate(linear_allocator *allocator, u64 size,
                                     u64 alignment);

/**
 * @brief Resets the linear_allocator struct and zeros out memory.
 *
 * @param allocator A pointer to the allocator struct.
 */
KAPI void linear_allocator_free_all(linear_allocator *allocator);
