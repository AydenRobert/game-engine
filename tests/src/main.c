#include "containers/freelist_tests.h"
#include "memory/dynamic_allocator_test.h"
#include "test_manager.h"

#include "memory/linear_allocator_test.h"
#include "containers/hastable_tests.h"

#include <core/logger.h>

int main() {
    test_manager_init();

    // register tests
    linear_allocator_register_tests();
    hashtable_register_tests();
    freelist_register_tests();
    dynamic_allocator_register_tests();

    KDEBUG("Starting tests...");

    test_manager_run_tests();
}
