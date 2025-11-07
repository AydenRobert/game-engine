#include "memory/linear_allocator.h"

#include "core/kmemory.h"
#include "core/logger.h"

void linear_allocator_create(u64 total_size, void *memory,
                             linear_allocator *out_allocator) {
    if (!out_allocator) {
        return;
    }

    out_allocator->total_size = total_size;
    out_allocator->allocated = 0;
    out_allocator->owns_memory = memory == 0;
    if (memory) {
        out_allocator->memory = memory;
    } else {
        out_allocator->memory =
            kallocate(out_allocator->total_size, MEMORY_TAG_LINEAR_ALLOCATOR);
    }
}

void linear_allocator_destroy(linear_allocator *allocator) {
    if (!allocator) {
        return;
    }

    if (allocator->owns_memory && allocator->memory) {
        kfree(allocator->memory, allocator->total_size,
              MEMORY_TAG_LINEAR_ALLOCATOR);
    }
    allocator->allocated = 0;
    allocator->total_size = 0;
    allocator->memory = 0;
    allocator->owns_memory = false;
}

void *linear_allocator_allocate(linear_allocator *allocator, u64 size) {
    if (!allocator || !allocator->memory) {
        KERROR(
            "linear_allocator_allocate - Provided allocator not initialized");
        return 0;
    }

    if (allocator->allocated + size > allocator->total_size) {
        u64 remaining = allocator->total_size - allocator->allocated;
        KERROR("linear_allocator_allocate - Tried to allocate %lluB, only "
               "%lluB remaining",
               size, remaining);
        return 0;
    }

    void *block = (u8 *)allocator->memory + allocator->allocated;
    allocator->allocated += size;
    return block;
}

void linear_allocator_free_all(linear_allocator *allocator) {
    if (!allocator || !allocator->memory) {
        KERROR(
            "linear_allocator_allocate - Provided allocator not initialized");
        return;
    }

    allocator->allocated = 0;
    kzero_memory(allocator->memory, allocator->total_size);
}
