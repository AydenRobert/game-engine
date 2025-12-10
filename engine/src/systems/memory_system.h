#pragma once

#include "containers/freelist.h"
#include "defines.h"

typedef struct memory_system_config {
    // Use for 'main memory pool'
    u64 initial_allocated;
    u64 max_memory;
} memory_system_config;

typedef struct allocation {
    u64 size;
    void *base_ptr;

    u64 freelist_size;
    void *freelist_memory;
    freelist commit_tracker;
} allocation;

b8 memory_system_initialise(memory_system_config config);

// void memory_system_shutdown();

allocation *allocate_reserved(u64 size); // won't garuentee commited

allocation *allocate_commited(u64 size); // garuentees commited

b8 allocation_ensure_commited(allocation *alloc, u64 start_index, u64 size);

void alloc_free(allocation *alloc);
