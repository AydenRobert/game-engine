#include "memory/linear_allocator.h"

#include "core/kmemory.h"
#include "core/logger.h"

typedef struct internal_state {
    u64 total_size;
    u64 allocated;
    void *memory;
} internal_state;

void linear_allocator_create(u64 total_size, u64 *memory_requirement,
                             void *memory, linear_allocator *out_allocator) {
    if (!memory_requirement) {
        KERROR(
            "linear_allocator_create - memory_requirement not passed through.");
        return;
    }

    *memory_requirement = sizeof(internal_state) + total_size;

    if (!memory) {
        return;
    }

    out_allocator->memory = memory;

    internal_state *state = (internal_state *)out_allocator->memory;
    state->total_size = total_size;
    state->allocated = 0;
    state->memory = (void *)(out_allocator->memory + sizeof(internal_state));
}

void linear_allocator_destroy(linear_allocator *allocator) {
    if (!allocator || !allocator->memory) {
        return;
    }

    internal_state *state = (internal_state *)allocator->memory;
    state->total_size = 0;
    state->allocated = 0;
    state->memory = 0;
}

void *linear_allocator_allocate(linear_allocator *allocator, u64 size,
                                u64 alignment) {
    if (!allocator || !allocator->memory) {
        KERROR(
            "linear_allocator_allocate - Provided allocator not initialized");
        return 0;
    }

    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        KERROR("linear_allocator_allocate - Alignment must be power of 2. Got: "
               "%llu",
               alignment);
        return 0;
    }

    internal_state *state = (internal_state *)allocator->memory;

    u64 raw_address = (u64)state->memory + state->allocated;

    u64 mask = alignment - 1;
    u64 aligned_address = (raw_address + mask) & ~mask;
    u64 padding = aligned_address - raw_address;

    u64 total_size = size + padding;

    if ((state->allocated + total_size) > state->total_size) {
        u64 remaining = state->total_size - state->allocated;
        KERROR("linear_allocator_allocate - Tried to allocate %lluB (padding: "
               "%lluB), only %lluB remaining",
               total_size, padding, remaining);
        return 0;
    }

    state->allocated += total_size;
    return (void *)aligned_address;
}

void linear_allocator_free_all(linear_allocator *allocator) {
    if (!allocator || !allocator->memory) {
        KERROR(
            "linear_allocator_allocate - Provided allocator not initialized");
        return;
    }

    internal_state *state = (internal_state *)allocator->memory;
    state->allocated = 0;
    kzero_memory(state->memory, state->total_size);
}
