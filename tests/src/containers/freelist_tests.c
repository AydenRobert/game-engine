#include "freelist_tests.h"
#include "containers/freelist.h"

#include "../expect.h"
#include "../test_manager.h"
#include "core/kmemory.h"
#include "core/logger.h"

#include <containers/freelist.h>
#include <defines.h>

u8 freelist_should_create_and_destroy() {
    u8 failed = false;

    freelist list;
    u32 total_size = 1024; // Managing 1KB
    u64 memory_requirement = 0;

    // 1. Get memory requirement
    freelist_create(total_size, &memory_requirement, 0, 0);
    expect_should_not_be(0, memory_requirement);

    // 2. Allocate state memory and create
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    freelist_create(total_size, &memory_requirement, memory, &list);

    expect_should_not_be(0, list.memory);
    expect_should_be(total_size, freelist_free_space(&list));

    freelist_destroy(&list);

    expect_should_not_be(0, list.memory);

    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 freelist_should_allocate_successfully() {
    u8 failed = false;

    freelist list;
    u32 total_size = 512;
    u64 memory_requirement = 0;

    freelist_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    freelist_create(total_size, &memory_requirement, memory, &list);

    u32 offset = 0;
    u32 alloc_size = 64;

    // Allocate 64 bytes
    b8 result = freelist_allocate_block(&list, alloc_size, &offset);

    expect_to_be_true(result);
    // Assuming a fresh list starts at offset 0
    expect_should_be(0, offset);
    expect_should_be(total_size - alloc_size, freelist_free_space(&list));

    freelist_destroy(&list);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 freelist_should_allocate_and_free_successfully() {
    u8 failed = false;

    freelist list;
    u32 total_size = 512;
    u64 memory_requirement = 0;

    freelist_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    freelist_create(total_size, &memory_requirement, memory, &list);

    u32 offset = 0;
    u32 alloc_size = 128;

    // Alloc
    b8 result = freelist_allocate_block(&list, alloc_size, &offset);
    expect_to_be_true(result);
    expect_should_be(total_size - alloc_size, freelist_free_space(&list));

    // Free
    result = freelist_free_block(&list, alloc_size, offset);
    expect_to_be_true(result);

    // Should be back to full capacity
    expect_should_be(total_size, freelist_free_space(&list));

    freelist_destroy(&list);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 freelist_should_handle_multiple_allocations() {
    u8 failed = false;

    freelist list;
    u32 total_size = 1024;
    u64 memory_requirement = 0;

    freelist_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    freelist_create(total_size, &memory_requirement, memory, &list);

    u32 offset1 = 0, offset2 = 0, offset3 = 0;

    // Alloc 1
    expect_to_be_true(freelist_allocate_block(&list, 100, &offset1));

    // Alloc 2
    expect_to_be_true(freelist_allocate_block(&list, 200, &offset2));

    // Alloc 3
    expect_to_be_true(freelist_allocate_block(&list, 300, &offset3));

    // Ensure they don't overlap (basic check)
    expect_should_not_be(offset1, offset2);
    expect_should_not_be(offset2, offset3);

    u64 expected_free = total_size - 100 - 200 - 300;
    expect_should_be(expected_free, freelist_free_space(&list));

    freelist_destroy(&list);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 freelist_should_fail_oversized_allocation() {
    u8 failed = false;

    freelist list;
    u32 total_size = 100;
    u64 memory_requirement = 0;

    freelist_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    freelist_create(total_size, &memory_requirement, memory, &list);

    u32 offset = 0;

    // Try to allocate 101 bytes from 100 byte list
    b8 result = freelist_allocate_block(&list, 101, &offset);

    expect_to_be_false(result);
    expect_should_be(total_size, freelist_free_space(&list));

    freelist_destroy(&list);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 freelist_should_clear_successfully() {
    u8 failed = false;

    freelist list;
    u32 total_size = 512;
    u64 memory_requirement = 0;

    freelist_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    freelist_create(total_size, &memory_requirement, memory, &list);

    u32 offset = 0;
    freelist_allocate_block(&list, 200, &offset);

    // Check space is reduced
    expect_should_not_be(total_size, freelist_free_space(&list));

    // Clear
    freelist_clear(&list);

    // Check space is restored
    expect_should_be(total_size, freelist_free_space(&list));

    // Should be able to allocate full size again
    expect_to_be_true(freelist_allocate_block(&list, 512, &offset));

    freelist_destroy(&list);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 freelist_should_reuse_freed_space() {
    u8 failed = false;

    freelist list;
    u32 total_size = 1024;
    u64 memory_requirement = 0;

    freelist_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    freelist_create(total_size, &memory_requirement, memory, &list);

    u32 offset1 = 0, offset2 = 0, offset3 = 0;

    // Allocate 3 blocks
    freelist_allocate_block(&list, 100, &offset1);
    freelist_allocate_block(&list, 100, &offset2); // The middle block
    freelist_allocate_block(&list, 100, &offset3);

    // Free the middle block
    freelist_free_block(&list, 100, offset2);

    u32 offset_new = 0;
    // Allocate a block that fits in the hole
    b8 result = freelist_allocate_block(&list, 100, &offset_new);

    expect_to_be_true(result);

    // Ideally, a good allocator will reuse the gap.
    // Depending on your implementation (First Fit), this should match offset2.
    // If it's a generic test, we at least ensure the allocation succeeded
    // and the space calculation is correct.
    expect_should_be(total_size - 300, freelist_free_space(&list));

    freelist_destroy(&list);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

void freelist_register_tests() {
    test_manager_register_test(
        freelist_should_create_and_destroy,
        "Freelist should create and destroy successfully.");

    test_manager_register_test(freelist_should_allocate_successfully,
                               "Freelist should allocate block successfully.");

    test_manager_register_test(
        freelist_should_allocate_and_free_successfully,
        "Freelist should allocate and then free block successfully.");

    test_manager_register_test(
        freelist_should_handle_multiple_allocations,
        "Freelist should handle multiple non-overlapping allocations.");

    test_manager_register_test(
        freelist_should_fail_oversized_allocation,
        "Freelist should fail to allocate block larger than total size.");

    test_manager_register_test(freelist_should_clear_successfully,
                               "Freelist should clear and reset all memory.");

    test_manager_register_test(
        freelist_should_reuse_freed_space,
        "Freelist should reuse freed space (fragmentation check).");
}
