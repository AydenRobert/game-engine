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

u8 linear_allocator_single_allocation_all_space() {
    linear_allocator alloc;
    u64 max_allocs = 1024;
    linear_allocator_create(sizeof(u64) * max_allocs, 0, &alloc);

    u8 failed = 0;

    void *block;
    block = linear_allocator_allocate(&alloc, sizeof(u64) * max_allocs);
    expect_should_not_be(0, block);
    expect_should_be(sizeof(u64) * max_allocs, alloc.allocated);

    linear_allocator_destroy(&alloc);

    return failed ? 0 : 1;
}

u8 linear_allocator_muli_allocate_all_space() {
    linear_allocator alloc;
    u64 max_allocs = 1024;
    linear_allocator_create(sizeof(u64) * max_allocs, 0, &alloc);

    u8 failed = 0;

    void *block;
    for (u64 i = 0; i < max_allocs; i++) {
        block = linear_allocator_allocate(&alloc, sizeof(u64));

        expect_should_not_be(0, block);
        expect_should_be(sizeof(u64) * (i + 1), alloc.allocated);
    }

    linear_allocator_destroy(&alloc);

    return failed ? 0 : 1;
}

u8 linear_allocator_multi_allocate_over_allocate() {
    linear_allocator alloc;
    u64 max_allocs = 64;
    linear_allocator_create(sizeof(u64) * max_allocs, 0, &alloc);

    u8 failed = 0;

    void *block;
    for (u64 i = 0; i < max_allocs; i++) {
        block = linear_allocator_allocate(&alloc, sizeof(u64));

        expect_should_not_be(0, block);
        expect_should_be(sizeof(u64) * (i + 1), alloc.allocated);
    }

    KDEBUG("Note: the following error is intentially caused by this test.");
    block = linear_allocator_allocate(&alloc, sizeof(u64));

    expect_should_be(0, block);

    linear_allocator_destroy(&alloc);

    return failed ? 0 : 1;
}

u8 linear_allocator_multi_allocate_then_free() {
    linear_allocator alloc;
    u64 max_allocs = 64;
    linear_allocator_create(sizeof(u64) * max_allocs, 0, &alloc);

    u8 failed = 0;

    void *block;
    for (u64 i = 0; i < max_allocs; i++) {
        block = linear_allocator_allocate(&alloc, sizeof(u64));

        expect_should_not_be(0, block);
        expect_should_be(sizeof(u64) * (i + 1), alloc.allocated);
    }

    linear_allocator_free_all(&alloc);
    expect_should_be(0, alloc.allocated);

    linear_allocator_destroy(&alloc);

    return failed ? 0 : 1;
}

// NOTE: stack memory alloc sits on is not zeroed out.
// Need to decide whether to add extra _allocate tests
// u8 linear_allocator_attempt_allocate_without_create() {
//     linear_allocator alloc;
//
//     u8 failed = 0;
//
//     void *block = linear_allocator_allocate(&alloc, sizeof(u64));
//     expect_should_be(0, block);
//
//     linear_allocator_destroy(&alloc);
//
//     return failed ? 0 : 1;
// }

void linear_allocator_register_tests() {
    test_manager_register_test(linear_allocator_should_create_and_destroy,
                               "Linear allocator should create and destroy.");
    test_manager_register_test(
        linear_allocator_single_allocation_all_space,
        "Linear allocator, allocating one time, max size.");
    test_manager_register_test(
        linear_allocator_muli_allocate_all_space,
        "Linear allocator, allocating multiple times, to max size.");
    test_manager_register_test(linear_allocator_multi_allocate_over_allocate,
                               "Linear allocator, multiple allocations, over "
                               "allocation on last allocation.");
    // TODO: decide to delete or keep test
    // test_manager_register_test(
    //     linear_allocator_attempt_allocate_without_create,
    //     "Attempting an allocation without initialising the allocator.");
    test_manager_register_test(
        linear_allocator_multi_allocate_then_free,
        "Linear allocate, multiple allocations followed by a free all.");
}
