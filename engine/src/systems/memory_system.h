#pragma once

#include "defines.h"

#include "systems/vmm_system.h"

typedef struct allocation {
    u64 offset;
    u64 size;
    memory_pool *pool;
    bitarray array;
} allocation;

typedef struct memory_system_config {
    // Use for 'main memory pool'
    u64 initial_allocated;
    u64 max_memory;
    u32 max_allocations;
} memory_system_config;

/**
 * @brief Initialises the memory system.
 * NOTE: VMM system must be initialised before this.
 *
 * @param config The config for the memory system.
 * @return True if successful; otherwise False.
 */
b8 memory_system_initialize(memory_system_config config);

/**
 * @brief Shutdowns the memory system.
 */
void memory_system_shutdown_tempname();

/**
 * @brief Reserves a block of address space and returns the pointer.
 * NOTE: atm garuenteed un-commited, might change in future...
 *
 * @param size The size of the block of address space.
 * @return Pointer to memory;
 */
void *allocate_reserved(u64 size);

/**
 * @brief Reserves and commits a block of address space and returns the pointer.
 *
 * @param size The size of the block of address space.
 * @return Pointer to memory;
 */
void *allocate_commited(u64 size);

/**
 * @brief Ensures that the range of pages (relative) is committed, therefore can
 * be used.
 * NOTE: Does not double commit memory.
 *
 * @param block The block of memory to commit address space on.
 * @param start_page_index The start page index.
 * @param page_amount The amount of pages.
 * @return True if successful; otherwise False.
 */
b8 allocation_ensure_commited_pages(void *block, u64 start_page_index,
                                    u64 page_amount);

/**
 * @brief Ensures that the range of address space (relative) is committed,
 * therefore can be used.
 * NOTE: Does not double commit memory.
 *
 * @param block The block of memory to commit address space on.
 * @param byte_offset The start offset from the base pointer.
 * @param size The size of the block to ensure is committed.
 * @return True if successful; otherwise False.
 */
b8 allocation_ensure_commited(void *block, u64 byte_offset, u64 size);

/**
 * @brief Gets the struct with the allocation information for a block of memory.
 *
 * @param block The block of memory.
 * @param alloc The struct which this function populates.
 * @return True if successful; otherwise False.
 */
b8 allocation_get_struct(void *block, allocation **alloc);

/**
 * @brief Frees an allocation block.
 * NOTE: Does not double free, can only free memory it is currently managing.
 *
 * @param block The block to free.
 */
void allocation_free(void *block);
