#include "systems/memory_system.h"
#include "containers/freelist.h"
#include "defines.h"
#include "systems/vmm_system.h"

typedef struct internal_state {
    memory_system_config config;

    u64 system_memory_allocated;
    memory_pool *system_pool;
    memory_pool *main_pool;

    binarytree alloc_tree;

    u64 freelist_size;
    freelist alloc_feelist;
    void *freelist_memory;
} internal_state;

typedef struct allocation {
    void *ptr;
    u64 size;
    memory_pool *pool;
} allocation;

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

    u64 freelist_size = 0;
    freelist_create(config.initial_allocated, &freelist_size, 0, 0);

    u64 system_size = sizeof(internal_state) + freelist_size;

    commit_info system_info;
    result = vmm_commit_pages(system_pool, 0, system_size, &system_info);
    if (!result) {
        return false;
    }

    state = system_pool->base_address;
    state->system_pool = system_pool;
    state->system_memory_allocated = system_size;

    memory_pool *main_pool = vmm_new_page_pool(GIBIBYTES(1ULL));
    commit_info main_info;
    result =
        vmm_commit_pages(main_pool, 0, config.initial_allocated, &main_info);
    if (!result) {
        return false;
    }

    freelist_create(config.initial_allocated, &state->freelist_size,
                    state->freelist_memory, &state->alloc_feelist);

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
    u64 offset = 0;
    if (!freelist_allocate_block(&state->alloc_feelist, size, &offset)) {
        return 0;
    }
    void *ptr = (void *)((u64)state->main_pool->base_address + offset);
    allocation alloc_info = {};
    alloc_info.ptr = ptr;
    alloc_info.pool = state->main_pool;
    alloc_info.size = size;
    binary_tree_add_node(state->alloc_tree, alloc_info);
}

void *allocate_commited(u64 size) {
    void *ptr = allocate_reserved(size);
    allocation_ensure_commited(ptr, 0, size);
    return ptr;
}

b8 allocation_get_bitarray(void *block, bitarray *out_array) {
    bitarray_make_subarray(&state->main_pool->array, offset, size);
}

b8 allocation_ensure_commited_pages(void *block, u64 start_page_index,
                                    u64 page_amount);

b8 allocation_ensure_commited(void *block, u64 byte_offset, u64 size);

void allocation_free(void *block);
