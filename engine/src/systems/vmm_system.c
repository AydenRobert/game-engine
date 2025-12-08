#include "defines.h"
#include "platform/platform.h"

#include "core/kmemory.h"
#include "systems/vmm_system.h"

typedef struct internal_state {
    vmm_config config;
    u32 page_size;
    u32 system_page_amount;
    u32 pages_reserved;
    u32 pages_mapped;
    u32 max_pages_reserved;
    u32 max_pages_mapped;

    memory_pool *pool_array;
} internal_state;

static internal_state *state = 0;

b8 vmm_initialise(vmm_config config) {
    u64 page_size = platform_get_page_size();

    // state size
    u64 state_size = get_aligned(sizeof(internal_state), 16);

    // array size
    u64 array_size =
        get_aligned(sizeof(memory_pool) * config.max_pool_amount, 16);

    // system size
    u64 system_size = state_size + array_size;
    u32 system_page_amount = (system_size + page_size - 1) / page_size;

    // reserve system space
    void *base_address;

    if (!platform_memory_reserve(0, system_page_amount * page_size,
                                 &base_address)) {
        return false;
    }

    // map system space
    if (!platform_memory_commit(base_address, system_page_amount * page_size)) {
        return false;
    }

    // set state
    state = base_address;
    state->config = config;
    state->page_size = page_size;
    state->system_page_amount = system_page_amount;

    state->max_pages_reserved =
        (config.max_memory_reserved + page_size - 1) / page_size;
    state->max_pages_mapped =
        (config.max_memory_mapped + page_size - 1) / page_size;
    state->pages_reserved = 0;
    state->pages_mapped = 0;

    // setup array
    state->pool_array = (void *)((u64)base_address + state_size);
    kzero_memory(state->pool_array, array_size);

    return true;
}

void vmm_shutdown() {
    platform_memory_release(state,
                            state->system_page_amount * state->page_size);
    state = 0;
}

memory_pool *vmm_new_page_pool(u64 size) {
    // find new pool
    void *new_array_address = 0;
    for (u32 i = 0; i < state->config.max_pool_amount; i++) {
        if (state->pool_array[i].base_address == 0) {
            new_array_address = &state->pool_array[i];
            break;
        }
    }

    if (!new_array_address) {
        return 0;
    }

    // calculate sizes
    u64 page_amount = (size + state->page_size - 1) / state->page_size;
    u32 new_pages_reserved = state->pages_reserved + page_amount;
    if (new_pages_reserved > state->max_pages_reserved) {
        return 0;
    }
    state->pages_reserved = new_pages_reserved;

    // reserve space
    void *base_address;
    if (!platform_memory_reserve(0, page_amount * state->page_size,
                                 &base_address)) {
        return 0;
    }
    memory_pool *new_pool = new_array_address;

    // setup pool state
    new_pool->base_address = base_address;
    new_pool->pages_reserved = page_amount;
    new_pool->memory_reserved = page_amount * state->page_size;
    new_pool->pages_mapped = 0;
    new_pool->memory_mapped = 0;

    return new_pool;
}

b8 vmm_commit_pages(memory_pool *pool, u64 start_index, u64 size,
                    commit_info *info) {
    if (!pool) {
        return false;
    }

    // calculate sizes
    u64 page_amount = (size + state->page_size - 1) / state->page_size;
    u32 new_pages_mapped = state->pages_mapped + page_amount;
    if (new_pages_mapped > state->max_pages_mapped) {
        return 0;
    }

    // Currently rounds up the start_page_index;
    u64 start_page_index =
        (start_index + state->page_size - 1) / state->page_size;

    // calculate start address
    void *commit_ptr =
        (void *)((u64)pool->base_address + start_page_index * state->page_size);

    // commit memory
    b8 result =
        platform_memory_commit(commit_ptr, page_amount * state->page_size);

    // update out state
    if (result) {
        state->pages_mapped = new_pages_mapped;
        pool->pages_mapped += page_amount;
        pool->memory_mapped += page_amount * state->page_size;
        info->start_index = start_page_index * state->page_size;
        info->size = page_amount * state->page_size;
    }

    return result;
}

b8 vmm_decommit_pages(memory_pool *pool, u64 start_index, u64 size,
                      commit_info *info) {
    if (!pool) {
        return false;
    }

    // calculate size
    u64 page_amount = (size + state->page_size - 1) / state->page_size;

    // Currently rounds up the start_page_index;
    u64 start_page_index =
        (start_index + state->page_size - 1) / state->page_size;

    // calculate start_address
    void *decommit_ptr =
        (void *)((u64)pool->base_address + start_index * state->page_size);

    // de-commit memory
    b8 result =
        platform_memory_decommit(decommit_ptr, page_amount * state->page_size);

    // update out state
    if (result) {
        state->pages_mapped -= page_amount;
        pool->pages_mapped -= page_amount;
        pool->memory_mapped -= page_amount * state->page_size;
        info->start_index = start_page_index * state->page_size;
        info->size = page_amount * state->page_size;
    }

    return result;
}

b8 vmm_release_page_pool(memory_pool *pool) {
    if (!pool) {
        return false;
    }

    // calculate size
    u64 size = pool->pages_reserved * state->page_size;

    // release memory
    b8 result = platform_memory_release(pool->base_address, size);

    // destroy pool
    if (result) {
        state->pages_reserved -= pool->pages_reserved;
        state->pages_mapped -= pool->pages_mapped;
        kzero_memory(pool, sizeof(memory_pool));
    }

    return result;
}

u32 vmm_page_size() { return state->page_size; }
