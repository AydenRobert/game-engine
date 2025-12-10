#include "test_manager.h"

#include "core/kmemory.h"

#include "containers/bitarray.h"
#include "containers/bitarray_tests.h"
#include "containers/freelist_tests.h"
#include "containers/hastable_tests.h"
#include "containers/linkedlist_tests.h"

#include "memory/dynamic_allocator_test.h"
#include "memory/linear_allocator_test.h"

#include "systems/vmm_tests.h"

#include <core/logger.h>

int main() {
    // memory
    memory_system_configuration memory_system_config = {};
    memory_system_config.total_alloc_count = GIBIBYTES(1);
    if (!memory_system_initialize(memory_system_config)) {
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
    vmm_register_tests();
    bitarray_register_tests();

    KDEBUG("Starting tests...");

    test_manager_run_tests();
}
