#include "containers/bitarray.h"
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

b8 alter_pages(memory_pool *pool, u32 start_index, u32 count, b8 commit,
               b8 *changed);
void recalc_mapped_size(memory_pool *pool);
u32 bytes_to_page(u64 bytes);
u64 page_to_bytes(u32 pages);

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

    state->max_pages_reserved = bytes_to_page(config.max_memory_reserved);
    state->max_pages_mapped = bytes_to_page(config.max_memory_mapped);
    state->pages_reserved = 0;
    state->pages_mapped = 0;

    // setup array
    state->pool_array = (void *)((u64)base_address + state_size);
    kzero_memory(state->pool_array, array_size);

    return true;
}

void vmm_shutdown() {
    // TODO: go through array, free all pools
    platform_memory_release(state, page_to_bytes(state->system_page_amount));
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
    u64 page_amount = bytes_to_page(size);
    u32 new_pages_reserved = state->pages_reserved + page_amount;
    if (new_pages_reserved > state->max_pages_reserved) {
        return 0;
    }
    state->pages_reserved = new_pages_reserved;

    // calculate system page size
    u32 array_size = (page_amount + 7) / 8;
    u32 system_page_amount = bytes_to_page(array_size);

    // reserve space
    void *base_address;
    if (!platform_memory_reserve(
            0, page_to_bytes(page_amount + system_page_amount),
            &base_address)) {
        return 0;
    }
    if (!platform_memory_commit(base_address, page_to_bytes(page_amount))) {
        return 0;
    }
    memory_pool *new_pool = new_array_address;

    // setup pool state
    new_pool->base_address =
        (void *)((u64)base_address + system_page_amount * state->page_size);
    new_pool->pages_reserved = page_amount;
    new_pool->memory_reserved = page_to_bytes(page_amount);
    new_pool->pages_mapped = 0;
    new_pool->memory_mapped = 0;

    new_pool->system_pages = system_page_amount;
    new_pool->array.length = page_amount;
    new_pool->array.array = base_address;

    return new_pool;
}

b8 vmm_commit_pages(memory_pool *pool, u64 start_index, u64 size,
                    commit_info *info) {
    if (!pool) {
        return false;
    }

    // calculate sizes
    u64 page_amount = bytes_to_page(size);
    u32 new_pages_mapped = state->pages_mapped + page_amount;
    if (new_pages_mapped > state->max_pages_mapped) {
        return 0;
    }

    // Currently rounds up the start_page_index;
    u64 start_page_index = bytes_to_page(start_index);

    u64 end_page_index = start_page_index + page_amount;
    if (end_page_index > pool->pages_reserved) {
        return false;
    }

    // commit memory
    b8 changed = false;
    b8 result =
        alter_pages(pool, start_page_index, page_amount, true, &changed);

    // update out state
    if (changed) {
        info->start_index = page_to_bytes(start_page_index);
        info->size = page_to_bytes(page_amount);
        recalc_mapped_size(pool);
    }

    return result;
}

b8 vmm_decommit_pages(memory_pool *pool, u64 start_index, u64 size,
                      commit_info *info) {
    if (!pool) {
        return false;
    }

    // calculate size
    u64 page_amount = bytes_to_page(size);

    // Currently rounds up the start_page_index;
    u64 start_page_index = bytes_to_page(start_index);

    u64 end_page_index = start_page_index + page_amount;
    if (end_page_index > pool->pages_reserved) {
        return false;
    }

    // de-commit memory
    b8 changed = false;
    b8 result =
        alter_pages(pool, start_page_index, page_amount, false, &changed);

    // update out state
    if (changed) {
        info->start_index = page_to_bytes(start_page_index);
        info->size = page_to_bytes(page_amount);
        recalc_mapped_size(pool);
    }

    return result;
}

b8 vmm_release_page_pool(memory_pool *pool) {
    if (!pool) {
        return false;
    }

    // calculate size
    u64 size = page_to_bytes(pool->pages_reserved);

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

b8 alter_pages(memory_pool *pool, u32 start_index, u32 count, b8 commit,
               b8 *changed) {
    u64 range_end = start_index + count;
    u64 current = start_index;

    // If we are committing, we are targetting non_committed
    b8 target_val = !commit;

    while (current < range_end) {
        u64 batch_start =
            bitarray_find_first(&pool->array, current, range_end, target_val);

        if (batch_start >= range_end) {
            break;
        }

        u64 batch_end = bitarray_find_first(&pool->array, batch_start + 1,
                                            range_end, commit);

        u64 batch_count = batch_end - batch_start;
        void *batch_address =
            (void *)((u64)pool->base_address + page_to_bytes(batch_start));
        u64 batch_size = page_to_bytes(batch_count);

        b8 result = commit
                        ? platform_memory_commit(batch_address, batch_size)
                        : platform_memory_decommit(batch_address, batch_size);

        if (!result) {
            return false;
        }

        *changed = true;

        bitarray_fill_range(&pool->array, commit, batch_start, batch_count);

        current = batch_end;
    }

    return true;
}

void recalc_mapped_size(memory_pool *pool) {
    u32 pages_mapped = (u32)bitarray_count_set(&pool->array);

    u32 diff = pages_mapped - pool->pages_mapped;

    pool->pages_mapped = pages_mapped;
    pool->memory_mapped = page_to_bytes(pages_mapped);
    state->pages_mapped += diff;
}

u32 bytes_to_page(u64 bytes) {
    return (bytes + state->page_size - 1) / state->page_size;
}

u64 page_to_bytes(u32 pages) { return pages * state->page_size; }
