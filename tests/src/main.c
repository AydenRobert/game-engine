#include "test_manager.h"

#include "memory/linear_allocator_test.h"

#include <core/logger.h>

int main() {
    test_manager_init();

    // register tests
    linear_allocator_register_tests();

    KDEBUG("Starting tests...");

    test_manager_run_tests();
}
