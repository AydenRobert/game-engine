#include "linear_allocator_test.h"

#include "../expect.h"
#include "../test_manager.h"

#include <defines.h>
#include <memory/linear_allocator.h>

u8 linear_allocator_should_create_and_destroy() {
    linear_allocator alloc;
    linear_allocator_create(sizeof(u64), 0, &alloc);

    u8 failed = 0;

    expect_should_not_be(0, alloc.memory);
    expect_should_be(sizeof(u64), alloc.total_size);
    expect_should_be(0, alloc.allocated);
    expect_should_be(true, alloc.owns_memory);

    linear_allocator_destroy(&alloc);

    expect_should_be(0, alloc.memory);
    expect_should_be(0, alloc.total_size);
    expect_should_be(0, alloc.allocated);
    expect_should_be(0, alloc.owns_memory);

    return failed ? 0 : 1;
}

void linear_allocator_register_tests() {
    test_manager_register_test(linear_allocator_should_create_and_destroy,
                               "Linear allocator should create and destroy.");
}
