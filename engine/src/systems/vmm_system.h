#pragma once

#include "defines.h"

// memory given here get's rounded to the nearest page.
typedef struct vmm_config {
    u32 max_memory_reserved; // in bytes
    u32 max_memory_mapped;   // in bytes
    u32 max_pool_amount;
} vmm_config;

/**
 * @brief Represents a 'pool' of memory. Memory that is logically contiguous.
 */
typedef struct memory_pool {
    void *base_address;

    u32 array_size_bits;
    void *array;

    u32 system_pages;
    u32 pages_reserved;
    u32 pages_mapped;
    u64 memory_reserved;
    u64 memory_mapped;
} memory_pool;

typedef struct commit_info {
    u64 start_index; // relative index
    u64 size;
} commit_info;

typedef enum page_state { PAGE_STATE_RESERVED, PAGE_STATE_MAPPED } page_state;

/**
 * @brief Initialises the virtual memory manager.
 *
 * @param config The config for the manager.
 * @return True if successful; otherwise False.
 */
b8 vmm_initialise(vmm_config config);

/**
 * @brief Shutdown the system.
 */
void vmm_shutdown();

/**
 * @brief Creates a new 'pool' of memory. Rounds the given size value up to the
 * nearest page.
 *
 * @param size The minimum amount of Virtual Address space to assign the pool.
 * @return Returns the handle to the pool of memory. Or 0.
 */
memory_pool *vmm_new_page_pool(u64 size);

/**
 * @brief Commits the amount of memory (rounded up to the nearest page).
 * NOTE: The Virtual Memory Manager does garuentee committed pages don't get
 * recommited.
 *
 * @param pool The handle for the pool of memory.
 * @param start_index The start index of the memory within the pool wanting to
 * be commited. This index should be relative to the pool.
 * @param size The minimum size to be committed.
 * @param info A pointer to an information struct which will be populated by
 * this function.
 * @return True if successful; otherwise False;
 */
b8 vmm_commit_pages(memory_pool *pool, u64 start_index, u64 size,
                    commit_info *info);

/**
 * @brief De-commits the amount of memory (rounded up to the nearest page).
 * NOTE: The Virtual Memory Manager does garuentee committed pages don't get
 * recommited.
 *
 * @param pool The handle for the pool of memory.
 * @param start_index The start index of the memory within the pool. This index
 * should be relative.
 * @param size The minimum size to be committed.
 * @param info A pointer to an information struct which will be populated by
 * this function.
 * @return True if successful; otherwise False;
 */
b8 vmm_decommit_pages(memory_pool *pool, u64 start_index, u64 size,
                      commit_info *info);

/**
 * @brief Releases all the memory within a pool.
 *
 * @param pool The pool to release.
 * @return True if successful; otherwise False;
 */
b8 vmm_release_page_pool(memory_pool *pool);

u32 vmm_page_size();
