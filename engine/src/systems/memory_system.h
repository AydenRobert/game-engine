#pragma once

#include "defines.h"

typedef struct memory_system_config {
    // Use for 'main memory pool'
    u64 initial_allocated;
    u64 max_memory;
} memory_system_config;

b8 memory_system_initialise(memory_system_config config);

void memory_system_shutdown();
