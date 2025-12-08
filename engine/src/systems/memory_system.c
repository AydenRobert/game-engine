#include "systems/memory_system.h"
#include "platform/platform.h"
#include "systems/vmm_system.h"

typedef struct internal_state {
    memory_system_config config;
} internal_state;

b8 memory_system_initialise(memory_system_config config) {
    return true;
}

void memory_system_shutdown();
