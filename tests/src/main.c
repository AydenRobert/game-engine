#include "systems/memory_system.h"
#include "systems/vmm_system.h"
#include "test_manager.h"

#include "core/kmemory.h"

#include "containers/binarytree_tests.h"
#include "containers/bitarray_tests.h"
#include "containers/freelist_tests.h"
#include "containers/hastable_tests.h"
#include "containers/linkedlist_tests.h"
#include "containers/stack_tests.h"

#include "memory/dynamic_allocator_test.h"
#include "memory/linear_allocator_test.h"

#include "systems/memory_system_tests.h"
#include "systems/vmm_tests.h"

#include <core/logger.h>

int main() {
    // memory
    vmm_config vmm_conf = {};
    vmm_conf.max_memory_reserved = GIBIBYTES(1024ULL);
    vmm_conf.max_memory_mapped = GIBIBYTES(2ULL);
    vmm_conf.max_pool_amount = 100;

    if (!vmm_initialise(vmm_conf)) {
        return false;
    }

    memory_system_config mem_sys_config = {};
    mem_sys_config.max_memory = GIBIBYTES(2ULL);
    mem_sys_config.initial_allocated = MEBIBYTES(128ULL);
    mem_sys_config.max_allocations = 255;
    if (!memory_system_initialize(mem_sys_config)) {
        return false;
    }

    // memory
    memory_system_configuration memory_system_config = {};
    memory_system_config.total_alloc_count = GIBIBYTES(1);
    if (!main_memory_initialize(memory_system_config)) {
        KERROR("Failed to initialize memory system, shutting down.");
        return false;
    }

    test_manager_init();

    // register tests
    linear_allocator_register_tests();
    hashtable_register_tests();
    freelist_register_tests();
    dynamic_allocator_register_tests();
    linkedlist_register_tests();
    bitarray_register_tests();
    binarytree_register_tests();
    stack_register_tests();
    // vmm_register_tests();
    // memory_system_register_tests();

    KDEBUG("Starting tests...");

    test_manager_run_tests();
}
