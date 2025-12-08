#include "defines.h"
#include "platform/platform.h"

#include "containers/linkedlist.h"
#include "core/kmemory.h"
#include "systems/vmm_system.h"

typedef struct internal_state {
    vmm_config config;
    u32 page_size;
    u32 system_page_amount;

    memory_pool *pool_array;
    linkedlist pool_list;
    void *linkedlist_memory;
} internal_state;

static internal_state *state = 0;

b8 vmm_initialise(vmm_config config) {
    u64 page_size = platform_get_page_size();

    // state size
    u64 state_size = get_aligned(sizeof(internal_state), 16);

    // array size
    u64 array_size =
        get_aligned(sizeof(memory_pool) * config.max_pool_amount, 16);

    // linked list size
    u64 linked_list_memory_requirement;
    if (!linkedlist_create(config.max_pool_amount,
                           &linked_list_memory_requirement, 0, 0)) {
        return false;
    }
    u64 linkedlist_size = get_aligned(linked_list_memory_requirement, 16);

    // system size
    u64 system_size = state_size + array_size + linkedlist_size;
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

    // setup array
    state->pool_array = (void *)((u64)base_address + state_size);
    kzero_memory(state->pool_array, array_size);

    // setup linkedlist
    state->linkedlist_memory =
        (void *)((u64)base_address + state_size + array_size);
    linkedlist_create(config.max_pool_amount, &linked_list_memory_requirement,
                      state->linkedlist_memory, &state->pool_list);

    return true;
}

memory_pool *vmm_new_page_pool(u32 size) {
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

    // reserve space
    void *base_address;
    if (!platform_memory_reserve(0, size * state->page_size, &base_address)) {
        return 0;
    }

    linkedlist_push_tail(&state->pool_list, new_array_address);
    memory_pool *new_pool = new_array_address;

    new_pool->base_address =
        (void *)((u64)base_address + size * state->page_size);
    new_pool->system_page_amount = size;
    new_pool->pages_reserved = size;
    new_pool->pages_mapped = 0;

    new_pool->state_array = base_address;

    return new_pool;
}

b8 vmm_commit_pages(void *base_ptr, u32 start_index, u32 size) {
    linkedlist_iterator iter;
    if (!linkedlist_iterator_begin(&state->pool_list, &iter)) {
        return false;
    }

    memory_pool *pool = linkedlist_iterator_get(&iter);
    while (pool) {
        if (pool == base_ptr) {
            break;
        }
        linkedlist_iterator_next(&iter);
        pool = linkedlist_iterator_get(&iter);
    }

    if (!pool) {
        return false;
    }

    void *commit_ptr = (void *)((u64)base_ptr + start_index * state->page_size);
    return platform_memory_commit(commit_ptr, size);
}

b8 vmm_decommit_pages(void *base_ptr, u32 start_index, u32 size) {
    linkedlist_iterator iter;
    if (!linkedlist_iterator_begin(&state->pool_list, &iter)) {
        return false;
    }

    memory_pool *pool = linkedlist_iterator_get(&iter);
    while (pool) {
        if (pool == base_ptr) {
            break;
        }
        linkedlist_iterator_next(&iter);
        pool = linkedlist_iterator_get(&iter);
    }

    if (!pool) {
        return false;
    }

    void *decommit_ptr =
        (void *)((u64)base_ptr + start_index * state->page_size);
    return platform_memory_decommit(decommit_ptr, size);
}

b8 vmm_release_page_pool(void *base_ptr) {
    linkedlist_iterator iter;
    if (!linkedlist_iterator_begin(&state->pool_list, &iter)) {
        return false;
    }

    memory_pool *pool = linkedlist_iterator_get(&iter);
    while (pool) {
        if (pool == base_ptr) {
            break;
        }
        linkedlist_iterator_next(&iter);
        pool = linkedlist_iterator_get(&iter);
    }

    if (!pool) {
        return false;
    }
    void *release_ptr =
        (void *)((u64)base_ptr - pool->system_page_amount * state->page_size);
    u64 size =
        (pool->system_page_amount + pool->pages_reserved) * state->page_size;
    return platform_memory_release(release_ptr, size);
}

u32 vmm_page_size() { return state->page_size; }
