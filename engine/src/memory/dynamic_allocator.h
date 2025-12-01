/**
 * @file dynamic_allocator.h
 * @brief Contains the implementation of the dynamic allocator
 * @version 1.0
 * @date 2025-12-01
 */

#pragma once

#include "defines.h"

/**
 * @brief The dynamic allocator struct.
 */
typedef struct dynamic_allocator {
    /** @brief The allocated memory block for the allocator to use. */
    void *memory;
} dynamic_allocator;

/**
 * @brief Creates a dynamic allocator. Should be called twice; once to get the
 * memory requirement, twice to create the struct.
 *
 * @param total_size The total_size in bytes that the allocator should hold.
 * This does not contain state of allocator.
 * @param memory_requirement A pointer to the amount of memory needed for the
 * struct.
 * @param memory A pointer to the memory for the allocator.
 * @param out_allocator A pointer to hold the allocator.
 * @return True if successful; other False.
 */
KAPI b8 dynamic_allocator_create(u64 total_size, u64 *memory_requirement,
                                 void *memory,
                                 dynamic_allocator *out_allocator);

/**
 * @brief Destroys a dynamic allocator.
 *
 * @param allocator A pointer to the allocator to destroy.
 * @return True if successful; other False.
 */
KAPI b8 dynamic_allocator_destroy(dynamic_allocator *allocator);

/**
 * @brief Allocates an amount of memory and returns the given block.
 *
 * @param allocator A pointer to the allocator struct.
 * @param size The size to allocate.
 * @return The block of memory if successful; otherwise Null;
 */
KAPI void *dynamic_allocator_allocate(dynamic_allocator *allocator, u64 size);

/**
 * @brief Frees a block of memory that was allocated by the allocator.
 *
 * @param allocator The allocator struct.
 * @param block The block to free.
 * @param size The size of the block.
 * @return True if successful; other False.
 */
KAPI b8 dynamic_allocator_free(dynamic_allocator *allocator, void *block,
                               u64 size);

/**
 * @brief Gets the amount of free space left in the provided dynamic allocator.
 *
 * @param allocator A pointer to the allocator struct.
 */
KAPI u64 dynamic_allocator_free_space(dynamic_allocator *allocator);
