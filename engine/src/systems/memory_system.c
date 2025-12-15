#include "systems/memory_system.h"
#include "containers/binarytree.h"
#include "containers/bitarray.h"
#include "containers/freelist.h"
#include "defines.h"
#include "systems/vmm_system.h"

typedef struct allocation {
    void *ptr;
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
    freelist alloc_feelist;
} internal_state;

static internal_state *state = 0;

b8 memory_system_initialise(memory_system_config config) {
    vmm_config vmm_conf = {};
    vmm_conf.max_memory_reserved = GIBIBYTES(1024ULL);
    vmm_conf.max_memory_mapped = config.max_memory;
    vmm_conf.max_pool_amount = 100;

    b8 result = vmm_initialise(vmm_conf);
    if (!result) {
        return false;
    }

    memory_pool *system_pool = vmm_new_page_pool(MEBIBYTES(1ULL));

    u64 binarytree_size = 0;
    binarytree_create(config.max_allocations, &binarytree_size, 0, 0);
    u64 allocation_array_size = sizeof(allocation) * config.max_allocations;
    u64 freelist_size = 0;
    freelist_create(config.initial_allocated, &freelist_size, 0, 0);

    u64 system_size = sizeof(internal_state) + binarytree_size +
                      allocation_array_size + freelist_size;

    commit_info system_info;
    result = vmm_commit_pages(system_pool, 0, system_size, &system_info);
    if (!result) {
        return false;
    }

    state = system_pool->base_address;
    state->system_pool = system_pool;
    state->system_memory_allocated = system_size;
    state->config = config;

    void *binarytree_address = (void *)((u64)state + sizeof(internal_state));
    binarytree_create(config.max_allocations, &state->alloc_tree_size,
                      binarytree_address, &state->alloc_tree);
    void *freelist_address =
        (void *)((u64)binarytree_address + state->alloc_tree_size);
    freelist_create(config.initial_allocated, &state->freelist_size,
                    freelist_address, &state->alloc_feelist);

    memory_pool *main_pool = vmm_new_page_pool(GIBIBYTES(1ULL));
    commit_info main_info;
    result =
        vmm_commit_pages(main_pool, 0, config.initial_allocated, &main_info);
    if (!result) {
        return false;
    }

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
        if (state->allocations[index].ptr == 0) {
            alloc_info = &state->allocations[index];
        }
    }
    if (alloc_info == 0) {
        return 0;
    }

    u64 offset = 0;
    if (!freelist_allocate_block(&state->alloc_feelist, size, &offset)) {
        return 0;
    }
    void *ptr = (void *)((u64)state->main_pool->base_address + offset);

    alloc_info->ptr = ptr;
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
    // allocation_ensure_commited(ptr, 0, size);
    return ptr;
}

b8 allocation_get_bitarray(void *block, bitarray *out_array) {
    // bitarray_make_subarray(&state->main_pool->array, offset, size);
    return false;
}

b8 allocation_ensure_commited_pages(void *block, u64 start_page_index,
                                    u64 page_amount);

b8 allocation_ensure_commited(void *block, u64 byte_offset, u64 size);

void allocation_free(void *block);
