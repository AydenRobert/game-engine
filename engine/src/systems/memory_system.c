#include "systems/memory_system.h"

#include "core/asserts.h"

#include "core/kmemory.h"
#include "defines.h"
#include "systems/vmm_system.h"

#include "containers/binarytree.h"
#include "containers/bitarray.h"
#include "containers/freelist.h"
#include "containers/stack.h"

typedef struct internal_state {
    memory_system_config config;

    u64 system_memory_allocated;
    memory_pool *system_pool;
    memory_pool *main_pool;

    u64 alloc_tree_size;
    binarytree alloc_tree;

    u64 alloc_stack_size;
    stack alloc_indexes;

    u64 alloc_array_size;
    allocation *allocations;

    u64 freelist_size;
    freelist alloc_freelist;
} internal_state;

static internal_state *state = 0;

b8 memory_system_initialize(memory_system_config config) {
    KASSERT_DEBUG(vmm_is_initialised());

    // Calculate sizes
    u64 binarytree_size = 0;
    binarytree_create(config.max_allocations, &binarytree_size, 0, 0);

    u64 stack_size = sizeof(u64) * config.max_allocations;
    u64 allocation_array_size = sizeof(allocation) * config.max_allocations;

    u64 freelist_size = 0;
    freelist_create(config.max_memory, &freelist_size, 0, 0);

    KASSERT_DEBUG(binarytree_size);
    KASSERT_DEBUG(freelist_size);

    u64 system_size = sizeof(internal_state) + binarytree_size + stack_size +
                      allocation_array_size + freelist_size;

    u64 system_size_round =
        (system_size + MEBIBYTES(1ULL) - 1) & ~(MEBIBYTES(1) - 1);

    // Get the system pool
    memory_pool *system_pool = vmm_new_page_pool((system_size_round));

    KASSERT_DEBUG(system_pool);
    KASSERT_DEBUG(system_pool->base_address);

    // Commit what's needed for memory system.
    commit_info system_info = {0};
    b8 result = vmm_commit_pages(system_pool, 0, system_size, &system_info);
    if (!result) {
        return false;
    }

    KASSERT_DEBUG(system_info.start_index == 0);
    KASSERT_DEBUG(system_info.size >= system_size);

    // Setup state
    state = (internal_state *)system_pool->base_address;
    state->system_pool = system_pool;
    state->system_memory_allocated = system_size;
    state->config = config;

    // Assign addresses
    void *binarytree_address = (void *)((u64)state + sizeof(internal_state));
    binarytree_create(config.max_allocations, &state->alloc_tree_size,
                      binarytree_address, &state->alloc_tree);
    state->alloc_stack_size = stack_size;
    void *stack_address =
        (void *)((u64)binarytree_address + state->alloc_tree_size);
    stack_create(state->alloc_stack_size, stack_address, &state->alloc_indexes);

    state->alloc_array_size = allocation_array_size;
    void *array_address =
        (void *)((u64)stack_address + state->alloc_stack_size);
    kzero_memory(array_address, state->alloc_array_size);
    state->allocations = array_address;

    void *freelist_address =
        (void *)((u64)array_address + state->alloc_array_size);
    freelist_create(config.max_memory, &state->freelist_size,
                    freelist_address, &state->alloc_freelist);

    KASSERT_DEBUG(state->alloc_tree.internal_state != 0);
    KASSERT_DEBUG(state->alloc_indexes.memory != 0);
    KASSERT_DEBUG(state->allocations != 0);
    KASSERT_DEBUG(state->alloc_freelist.memory != 0);

    // Setup indexes
    for (u32 i = 0; i < config.max_allocations; i++) {
        stack_push(&state->alloc_indexes, config.max_allocations - (i + 1));
    }

    // Get the main pool
    memory_pool *main_pool = vmm_new_page_pool(config.max_memory);

    KASSERT_DEBUG(main_pool);
    KASSERT_DEBUG(main_pool->base_address);

    // Commit what is asked of the main pool
    commit_info main_info;
    result =
        vmm_commit_pages(main_pool, 0, config.initial_allocated, &main_info);
    if (!result) {
        return false;
    }

    KASSERT_DEBUG(main_info.start_index == 0);
    KASSERT_DEBUG(main_info.size >= config.initial_allocated);

    state->main_pool = main_pool;

    return true;
}

void memory_system_shutdown_tempname() {
    if (!state) {
        return;
    }

    vmm_release_page_pool(state->main_pool);
    vmm_release_page_pool(state->system_pool);
    state = 0;
}

// TODO: align by pages, would need to add to freelist code
void *allocate_reserved(u64 size) {
    // Get a free allocation info
    allocation *alloc_info = 0;
    u64 index = INVALID_ID;
    stack_pop(&state->alloc_indexes, &index);
    if (index == INVALID_ID) {
        return 0;
    }
    alloc_info = &state->allocations[index];

    alloc_info->pool = state->main_pool;

    // allocate in the freelist
    u64 offset = 0;
    if (!freelist_allocate_block_aligned(&state->alloc_freelist, size,
                                         alloc_info->pool->page_size,
                                         &offset)) {
        return 0;
    }
    void *ptr = (void *)((u64)state->main_pool->base_address + offset);

    alloc_info->offset = offset;
    alloc_info->pool = state->main_pool;
    alloc_info->size = size;
    if (!binarytree_insert(&state->alloc_tree, (u64)ptr, index)) {
        // Clean up if goes wrong
        freelist_free_block(&state->alloc_freelist, size, offset);
        alloc_info->offset = 0;
        alloc_info->pool = 0;
        alloc_info->size = 0;
        return 0;
    }

    // Setup sub bitarray
    u64 page_size = state->main_pool->page_size;
    u64 ptr_diff = (u64)ptr - (u64)state->main_pool->base_address;
    u32 page_diff = ptr_diff / page_size;
    u32 page_amount = (size + page_size - 1) / page_size;
    bitarray_create_sub_array(&state->main_pool->array, page_diff, page_amount,
                              &alloc_info->array);

    return ptr;
}

void *allocate_commited(u64 size) {
    void *ptr = allocate_reserved(size);
    if (!ptr) {
        return 0;
    }
    allocation_ensure_commited(ptr, 0, size);
    return ptr;
}

b8 allocation_ensure_commited_pages(void *block, u64 start_page_index,
                                    u64 page_amount) {
    if (!block) {
        return false;
    }

    u32 index = INVALID_ID;
    b8 result = binarytree_search(&state->alloc_tree, (u64)block, &index);
    if (!result || index == INVALID_ID) {
        return false;
    }

    KASSERT_DEBUG(index < state->config.max_allocations);

    allocation *alloc_info = &state->allocations[index];
    u32 page_size = alloc_info->pool->page_size;
    // Start from the start of the page
    u64 byte_offset = (start_page_index)*page_size;
    u64 size = page_amount * page_size;
    if (byte_offset + size > alloc_info->size) {
        return false;
    }

    commit_info commit_info = {0};
    if (!vmm_commit_pages(alloc_info->pool, byte_offset, size, &commit_info)) {
        return false;
    }

    return true;
}

b8 allocation_ensure_commited(void *block, u64 byte_offset, u64 size) {
    if (!block) {
        return false;
    }

    u32 index = INVALID_ID;
    b8 result = binarytree_search(&state->alloc_tree, (u64)block, &index);
    if (!result || index == INVALID_ID) {
        return false;
    }

    KASSERT_DEBUG(index < state->config.max_allocations);

    allocation *alloc_info = &state->allocations[index];
    if (byte_offset + size > alloc_info->size) {
        return false;
    }

    commit_info commit_info = {0};
    if (!vmm_commit_pages(alloc_info->pool, byte_offset, size, &commit_info)) {
        return false;
    }

    return true;
}

b8 allocation_get_struct(void *block, allocation **alloc) {
    if (!block || !alloc) {
        return false;
    }

    u32 index = INVALID_ID;
    b8 result = binarytree_search(&state->alloc_tree, (u64)block, &index);
    if (!result || index == INVALID_ID) {
        return false;
    }

    KASSERT_DEBUG(index < state->config.max_allocations);

    *alloc = &state->allocations[index];

    return true;
}

void allocation_free(void *block) {
    if (!block) {
        return;
    }

    u32 index = INVALID_ID;

    if (!binarytree_delete(&state->alloc_tree, (u64)block, &index)) {
        return;
    }

    KASSERT_DEBUG(index != INVALID_ID);
    KASSERT_DEBUG(index < state->config.max_allocations);

    allocation *alloc_info = &state->allocations[index];

    if (!freelist_free_block(&state->alloc_freelist, alloc_info->size,
                             alloc_info->offset)) {
        return;
    }

    commit_info commit_info;
    if (!vmm_decommit_pages(alloc_info->pool, alloc_info->offset,
                            alloc_info->size, &commit_info)) {
        return;
    }

    KASSERT_DEBUG(commit_info.start_index == alloc_info->offset);

    kzero_memory(alloc_info, sizeof(allocation));

    stack_push(&state->alloc_indexes, index);
}
