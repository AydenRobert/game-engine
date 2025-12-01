#include "memory/dynamic_allocator.h"
#include "containers/freelist.h"
#include "core/kmemory.h"
#include "core/logger.h"

// TODO: support memory alignment

typedef struct internal_state {
    freelist freelist;
    void *memory;
} internal_state;

b8 dynamic_allocator_create(u64 total_size, u64 *memory_requirement,
                            void *memory, dynamic_allocator *out_allocator) {
    // Get memory requirement:
    // dynamic allocator -> internal_state
    // freelist -> internal_state
    // memory pool
    u64 freelist_memory_requirement = 0;
    freelist_create(total_size, &freelist_memory_requirement, 0, 0);
    *memory_requirement =
        sizeof(internal_state) + freelist_memory_requirement + total_size;

    if (!memory) {
        return true;
    }

    if (!out_allocator) {
        KERROR("dynamic_allocator_create - Passed in null out_allocator.");
        return false;
    }

    out_allocator->memory = memory;

    internal_state *state = (internal_state *)out_allocator->memory;
    // Setup freelist
    freelist_create(total_size, &freelist_memory_requirement,
                    (void *)(out_allocator->memory + sizeof(internal_state)),
                    &state->freelist);
    // Initialise the rest of the memory
    state->memory = (void *)(out_allocator->memory + sizeof(internal_state) +
                             freelist_memory_requirement);
    return true;
}

b8 dynamic_allocator_destroy(dynamic_allocator *allocator) {
    if (!allocator) {
        KERROR("dynamic_allocator_destroy - Passed in null allocator.");
        return false;
    }

    // Zero out structs, destroy freelist
    internal_state *state = (internal_state *)allocator->memory;
    freelist_destroy(&state->freelist);
    state->memory = 0;
    allocator->memory = 0;
    return true;
}

void *dynamic_allocator_allocate(dynamic_allocator *allocator, u64 size) {
    if (!allocator) {
        KERROR("dynamic_allocator_allocate - Passed in null allocator.");
        return false;
    }

    internal_state *state = (internal_state *)allocator->memory;

    u64 offset = 0;

    if (!freelist_allocate_block(&state->freelist, size, &offset)) {
        KERROR("dynamic_allocator_allocate - cannot allocate block, returning "
               "nullptr.");
        return 0;
    }

    void *block_to_return = (void *)(state->memory + offset);

    return block_to_return;
}

b8 dynamic_allocator_free(dynamic_allocator *allocator, void *block, u64 size) {
    if (!allocator) {
        KERROR("dynamic_allocator_free - Passed in null allocator.");
        return false;
    }

    internal_state *state = (internal_state *)allocator->memory;

    u32 offset = (u32)(block - state->memory);

    if (!freelist_free_block(&state->freelist, size, offset)) {
        KERROR("dynamic_allocator_free - cannot free block.");
        return false;
    }

    return true;
}

u64 dynamic_allocator_free_space(dynamic_allocator *allocator) {
    if (!allocator) {
        KERROR("dynamic_allocator_free_space - Passed in null allocator.");
        return false;
    }

    internal_state *state = (internal_state *)allocator->memory;
    return freelist_free_space(&state->freelist);
}
