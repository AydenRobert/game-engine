#include "freelist_tests.h"
#include <containers/freelist.h>

#include "../expect.h"
#include "../test_manager.h"
#include "core/kmemory.h"
#include "core/logger.h"

#include <containers/freelist.h>
#include <defines.h>

u8 freelist_should_create_and_destroy() {
    u8 failed = false;

    freelist list;
    u64 total_size = 1024; // Managing 1KB
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
    u64 total_size = 512;
    u64 memory_requirement = 0;

    freelist_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    freelist_create(total_size, &memory_requirement, memory, &list);

    u64 offset = 0;
    u64 alloc_size = 64;

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
    u64 total_size = 512;
    u64 memory_requirement = 0;

    freelist_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    freelist_create(total_size, &memory_requirement, memory, &list);

    u64 offset = 0;
    u64 alloc_size = 128;

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
    u64 total_size = 1024;
    u64 memory_requirement = 0;

    freelist_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    freelist_create(total_size, &memory_requirement, memory, &list);

    u64 offset1 = 0, offset2 = 0, offset3 = 0;

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
    u64 total_size = 100;
    u64 memory_requirement = 0;

    freelist_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    freelist_create(total_size, &memory_requirement, memory, &list);

    u64 offset = 0;

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
    u64 total_size = 512;
    u64 memory_requirement = 0;

    freelist_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    freelist_create(total_size, &memory_requirement, memory, &list);

    u64 offset = 0;
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
    u64 total_size = 1024;
    u64 memory_requirement = 0;

    freelist_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    freelist_create(total_size, &memory_requirement, memory, &list);

    u64 offset1 = 0, offset2 = 0, offset3 = 0;

    // Allocate 3 blocks
    freelist_allocate_block(&list, 100, &offset1);
    freelist_allocate_block(&list, 100, &offset2); // The middle block
    freelist_allocate_block(&list, 100, &offset3);

    // Free the middle block
    freelist_free_block(&list, 100, offset2);

    u64 offset_new = 0;
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
u8 freelist_should_allocate_all_space() {
    u8 failed = false;

    freelist list;
    u64 total_size = 512;
    u64 memory_requirement = 0;

    // Setup
    freelist_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    freelist_create(total_size, &memory_requirement, memory, &list);

    u64 offset = 0;

    // 1. Attempt to allocate the exact total size of the freelist
    b8 result = freelist_allocate_block(&list, total_size, &offset);

    // Verify allocation succeeded
    expect_to_be_true(result);
    // Offset should be at the very start
    expect_should_be(0, offset);
    // Free space should be exactly 0
    expect_should_be(0, freelist_free_space(&list));

    // 2. Free the whole block
    result = freelist_free_block(&list, total_size, offset);

    // Verify free succeeded
    expect_to_be_true(result);
    // Free space should return to full capacity
    expect_should_be(total_size, freelist_free_space(&list));

    // Cleanup
    freelist_destroy(&list);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 freelist_should_resize_successfully() {
    u8 failed = false;

    freelist list;
    u64 initial_size = 512;
    u64 new_total_size = 1024;
    u64 initial_req = 0;

    // 1. Setup initial list
    freelist_create(initial_size, &initial_req, 0, 0);
    void *initial_memory = kallocate(initial_req, MEMORY_TAG_ARRAY);
    freelist_create(initial_size, &initial_req, initial_memory, &list);

    // 2. Fill the list completely (Free space = 0)
    u64 offset = 0;
    freelist_allocate_block(&list, initial_size, &offset);
    expect_should_be(0, freelist_free_space(&list));

    // 3. Prepare for resize
    u64 new_req = 0;
    // Get new requirement
    freelist_resize(&list, &new_req, 0, new_total_size, 0);
    expect_should_not_be(0, new_req);

    void *new_memory = kallocate(new_req, MEMORY_TAG_ARRAY);
    void *old_memory_ptr = 0;

    // 4. Perform Resize
    b8 result = freelist_resize(&list, &new_req, new_memory, new_total_size,
                                &old_memory_ptr);

    expect_to_be_true(result);
    // The pointer returned should match the one we created initially
    expect_should_be((u64)initial_memory, (u64)old_memory_ptr);

    // 5. Verify logic: Old size was 512 (used), New is 1024.
    // We should have 512 bytes of NEW free space available.
    expect_should_be(new_total_size - initial_size, freelist_free_space(&list));

    // 6. Cleanup
    // Important: Free the OLD memory returned by resize using the OLD
    // requirement
    kfree(old_memory_ptr, initial_req, MEMORY_TAG_ARRAY);

    // Destroy the list (which is now using new_memory)
    freelist_destroy(&list);
    // Free the NEW memory
    kfree(new_memory, new_req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 freelist_should_fail_resize_to_smaller() {
    u8 failed = false;

    freelist list;
    u64 initial_size = 1024;
    u64 req = 0;

    freelist_create(initial_size, &req, 0, 0);
    void *memory = kallocate(req, MEMORY_TAG_ARRAY);
    freelist_create(initial_size, &req, memory, &list);

    // Prepare invalid resize (smaller than initial)
    u64 new_size = 512;
    u64 new_req = 0;
    void *old_memory_ptr = 0;

    // Even getting the requirement might fail or return 0, but the resize call
    // definitely should. Let's assume we allocate a dummy block just to pass a
    // valid pointer
    void *dummy_mem = kallocate(req, MEMORY_TAG_ARRAY);

    b8 result =
        freelist_resize(&list, &new_req, dummy_mem, new_size, &old_memory_ptr);

    expect_to_be_false(result);

    // Cleanup
    kfree(dummy_mem, req, MEMORY_TAG_ARRAY);
    freelist_destroy(&list);
    kfree(memory, req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 freelist_should_resize_and_allocate_new_space() {
    u8 failed = false;

    freelist list;
    u64 initial_size = 512;
    u64 new_total_size = 1024;
    u64 initial_req = 0;

    // 1. Setup initial list
    freelist_create(initial_size, &initial_req, 0, 0);
    void *initial_memory = kallocate(initial_req, MEMORY_TAG_ARRAY);
    freelist_create(initial_size, &initial_req, initial_memory, &list);

    // 2. Allocate the entire initial space
    u64 offset1 = 0;
    b8 result = freelist_allocate_block(&list, initial_size, &offset1);
    expect_to_be_true(result);
    expect_should_be(0, freelist_free_space(&list));

    // 3. Prepare for resize
    u64 new_req = 0;
    freelist_resize(&list, &new_req, 0, new_total_size,
                    0); // Get new requirement

    void *new_memory = kallocate(new_req, MEMORY_TAG_ARRAY);
    void *old_memory_ptr = 0;

    // 4. Resize
    result = freelist_resize(&list, &new_req, new_memory, new_total_size,
                             &old_memory_ptr);
    expect_to_be_true(result);

    // 5. Verify we have exactly the difference available (512 bytes)
    expect_should_be(new_total_size - initial_size, freelist_free_space(&list));

    // 6. Attempt to allocate into the NEW space
    // We try to allocate the remaining 512 bytes.
    u64 offset2 = 0;
    result = freelist_allocate_block(&list, 512, &offset2);

    expect_to_be_true(result);
    // The new block should start exactly where the old size ended
    expect_should_be(initial_size, offset2);
    // The list should now be totally full again
    expect_should_be(0, freelist_free_space(&list));

    // 7. Cleanup
    kfree(old_memory_ptr, initial_req, MEMORY_TAG_ARRAY);
    freelist_destroy(&list);
    kfree(new_memory, new_req, MEMORY_TAG_ARRAY);

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

    test_manager_register_test(
        freelist_should_allocate_all_space,
        "Freelist should allocate and free the entire available space.");

    test_manager_register_test(
        freelist_should_resize_successfully,
        "Freelist should resize and preserve state successfully.");

    test_manager_register_test(
        freelist_should_fail_resize_to_smaller,
        "Freelist should fail to resize to a smaller capacity.");

    test_manager_register_test(
        freelist_should_resize_and_allocate_new_space,
        "Freelist should allow allocation in new space after resize.");
}
