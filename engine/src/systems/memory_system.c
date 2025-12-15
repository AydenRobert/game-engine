#include "systems/memory_system.h"
#include "containers/binarytree.h"
#include "containers/bitarray.h"
#include "containers/freelist.h"
#include "core/asserts.h"
#include "defines.h"
#include "systems/vmm_system.h"

// define alignment

typedef struct allocation {
    u64 offset;
    u64 size;
    memory_pool *pool;
    bitarray array;
} allocation;

typedef struct internal_state {
    memory_system_config config;

    u64 system_memory_allocated;
    memory_pool *system_pool;
    memory_pool *main_pool;

    u64 alloc_tree_size;
    binarytree alloc_tree;
    allocation *allocations;

    u64 freelist_size;
    freelist alloc_freelist;
} internal_state;

static internal_state *state = 0;

b8 memory_system_initialise(memory_system_config config) {
    KASSERT(vmm_is_initialised());

    memory_pool *system_pool = vmm_new_page_pool(MEBIBYTES(1ULL));

    KASSERT(system_pool);
    KASSERT(system_pool->base_address);

    u64 binarytree_size = 0;
    binarytree_create(config.max_allocations, &binarytree_size, 0, 0);
    u64 allocation_array_size = sizeof(allocation) * config.max_allocations;
    u64 freelist_size = 0;
    freelist_create(config.initial_allocated, &freelist_size, 0, 0);

    KASSERT(binarytree_size);
    KASSERT(freelist_size);

    u64 system_size = sizeof(internal_state) + binarytree_size +
                      allocation_array_size + freelist_size;

    commit_info system_info = {0};
    b8 result = vmm_commit_pages(system_pool, 0, system_size, &system_info);
    if (!result) {
        return false;
    }

    KASSERT(system_info.start_index == 0);
    KASSERT(system_info.size >= system_size);

    state = (internal_state *)system_pool->base_address;
    state->system_pool = system_pool;
    state->system_memory_allocated = system_size;
    state->config = config;

    void *binarytree_address = (void *)((u64)state + sizeof(internal_state));
    binarytree_create(config.max_allocations, &state->alloc_tree_size,
                      binarytree_address, &state->alloc_tree);
    void *freelist_address =
        (void *)((u64)binarytree_address + state->alloc_tree_size);
    freelist_create(config.initial_allocated, &state->freelist_size,
                    freelist_address, &state->alloc_freelist);

    memory_pool *main_pool = vmm_new_page_pool(GIBIBYTES(1ULL));

    KASSERT(main_pool);
    KASSERT(main_pool->base_address);

    commit_info main_info;
    result =
        vmm_commit_pages(main_pool, 0, config.initial_allocated, &main_info);
    if (!result) {
        return false;
    }

    KASSERT(main_info.start_index == 0);
    KASSERT(main_info.size >= config.initial_allocated);

    state->main_pool = main_pool;

    return true;
}

// void memory_system_shutdown() {
//     if (!state) {
//         return;
//     }
//
//     vmm_release_page_pool(state->main_pool);
//     vmm_release_page_pool(state->system_pool);
//     vmm_shutdown();
//     state = 0;
// }

void *allocate_reserved(u64 size) {
    allocation *alloc_info = 0;
    u32 index;
    for (index = 0; index < state->config.max_allocations; index++) {
        if (state->allocations[index].offset == 0) {
            alloc_info = &state->allocations[index];
        }
    }
    if (alloc_info == 0) {
        return 0;
    }

    u64 offset = 0;
    if (!freelist_allocate_block(&state->alloc_freelist, size, &offset)) {
        return 0;
    }
    void *ptr = (void *)((u64)state->main_pool->base_address + offset);

    alloc_info->offset = offset;
    alloc_info->pool = state->main_pool;
    alloc_info->size = size;
    if (!binarytree_insert(&state->alloc_tree, (u64)ptr, index)) {
        return 0;
    }

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

    KASSERT(index < state->config.max_allocations);

    allocation *alloc_info = &state->allocations[index];
    u32 page_size = alloc_info->pool->page_size;
    // Start from the start of the page
    u64 byte_offset = (start_page_index - 1) * page_size;
    u64 size = page_amount * page_size;
    u64 start_index =
        (u64)alloc_info->pool->base_address + alloc_info->offset + byte_offset;
    if (byte_offset + size > alloc_info->size) {
        return false;
    }

    commit_info commit_info = {0};
    if (!vmm_commit_pages(alloc_info->pool, start_index, size, &commit_info)) {
        return false;
    }

    KASSERT(start_index > commit_info.start_index);
    KASSERT(size < commit_info.size);

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

    KASSERT(index < state->config.max_allocations);

    allocation *alloc_info = &state->allocations[index];
    u64 start_index =
        (u64)alloc_info->pool->base_address + alloc_info->offset + byte_offset;
    if (byte_offset + size > alloc_info->size) {
        return false;
    }

    commit_info commit_info = {0};
    if (!vmm_commit_pages(alloc_info->pool, start_index, size, &commit_info)) {
        return false;
    }

    KASSERT(start_index > commit_info.start_index);
    KASSERT(size < commit_info.size);

    return true;
}

void allocation_free(void *block) {
    if (!block) {
        return;
    }

    u32 index = INVALID_ID;
    b8 result = binarytree_search(&state->alloc_tree, (u64)block, &index);

    KASSERT(result);
    KASSERT(index != INVALID_ID);
    KASSERT(index < state->config.max_allocations);

    allocation *alloc_info = &state->allocations[index];

    freelist_free_block(&state->alloc_freelist, alloc_info->size,
                        alloc_info->offset);

    // TODO: setup decommit tracking and freeing
}
