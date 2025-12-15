#pragma once

#include "containers/bitarray.h"
#include "containers/freelist.h"
#include "defines.h"

typedef struct memory_system_config {
    // Use for 'main memory pool'
    u64 initial_allocated;
    u64 max_memory;
    u32 max_allocations;
} memory_system_config;

b8 memory_system_initialise(memory_system_config config);

// void memory_system_shutdown();

void *allocate_reserved(u64 size); // won't garuentee commited

void *allocate_commited(u64 size); // garuentees commited

b8 allocation_get_bitarray(void *block, bitarray *out_array);

b8 allocation_ensure_commited_pages(void *block, u64 start_page_index,
                                    u64 page_amount);

b8 allocation_ensure_commited(void *block, u64 byte_offset, u64 size);

void allocation_free(void *block);
