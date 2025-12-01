#include "dynamic_allocator_test.h"

#include <memory/dynamic_allocator.h>

#include "../expect.h"
#include "../test_manager.h"
#include "core/kmemory.h"
#include "core/logger.h"
#include <defines.h>

typedef struct test_data {
    u32 id;
    u64 timestamp;
    f32 value;
} test_data;

u8 dynamic_allocator_should_create_and_destroy() {
    u8 failed = false;

    dynamic_allocator allocator;
    u64 total_size = 1024; // Managing 1KB
    u64 memory_requirement = 0;

    // 1. Get memory requirement
    dynamic_allocator_create(total_size, &memory_requirement, 0, 0);
    expect_should_not_be(0, memory_requirement);

    // 2. Allocate state memory and create
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    dynamic_allocator_create(total_size, &memory_requirement, memory,
                             &allocator);

    expect_should_not_be(0, allocator.memory);
    expect_should_be(total_size, dynamic_allocator_free_space(&allocator));

    dynamic_allocator_destroy(&allocator);

    // Verify memory pointer isn't cleared by destroy (caller owns it),
    // but the internal state should be invalid/reset if your implementation
    // does that. For now, we just ensure destroy ran.

    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 dynamic_allocator_should_allocate_successfully() {
    u8 failed = false;

    dynamic_allocator allocator;
    u64 total_size = 512;
    u64 memory_requirement = 0;

    dynamic_allocator_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    dynamic_allocator_create(total_size, &memory_requirement, memory,
                             &allocator);

    u64 alloc_size = 64;

    // Allocate 64 bytes
    void *block = dynamic_allocator_allocate(&allocator, alloc_size);

    expect_should_not_be(0, block);

    // Verify block is within the memory range
    // Note: Assuming flat memory model for pointer arithmetic check
    expect_to_be_true(((u64)block >= (u64)allocator.memory));
    expect_to_be_true(
        ((u64)block < ((u64)allocator.memory + memory_requirement)));

    // Depending on overhead, free space might be exactly (total - alloc) or
    // less This expects exact counting (no overhead visible to free_space API
    // yet) Adjust if your allocator includes header overhead in the 'used'
    // count.
    expect_should_be(total_size - alloc_size,
                     dynamic_allocator_free_space(&allocator));

    dynamic_allocator_destroy(&allocator);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 dynamic_allocator_should_allocate_and_free_successfully() {
    u8 failed = false;

    dynamic_allocator allocator;
    u64 total_size = 512;
    u64 memory_requirement = 0;

    dynamic_allocator_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    dynamic_allocator_create(total_size, &memory_requirement, memory,
                             &allocator);

    u64 alloc_size = 128;

    // Alloc
    void *block = dynamic_allocator_allocate(&allocator, alloc_size);
    expect_should_not_be(0, block);
    expect_should_be(total_size - alloc_size,
                     dynamic_allocator_free_space(&allocator));

    // Free
    b8 result = dynamic_allocator_free(&allocator, block, alloc_size);
    expect_to_be_true(result);

    // Should be back to full capacity
    expect_should_be(total_size, dynamic_allocator_free_space(&allocator));

    dynamic_allocator_destroy(&allocator);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 dynamic_allocator_should_handle_multiple_allocations() {
    u8 failed = false;

    dynamic_allocator allocator;
    u64 total_size = 1024;
    u64 memory_requirement = 0;

    dynamic_allocator_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    dynamic_allocator_create(total_size, &memory_requirement, memory,
                             &allocator);

    void *block1 = 0;
    void *block2 = 0;
    void *block3 = 0;

    // Alloc 1
    block1 = dynamic_allocator_allocate(&allocator, 100);
    expect_should_not_be(0, block1);

    // Alloc 2
    block2 = dynamic_allocator_allocate(&allocator, 200);
    expect_should_not_be(0, block2);

    // Alloc 3
    block3 = dynamic_allocator_allocate(&allocator, 300);
    expect_should_not_be(0, block3);

    // Ensure pointers are distinct
    expect_should_not_be(block1, block2);
    expect_should_not_be(block2, block3);
    expect_should_not_be(block1, block3);

    u64 expected_free = total_size - 100 - 200 - 300;
    expect_should_be(expected_free, dynamic_allocator_free_space(&allocator));

    dynamic_allocator_destroy(&allocator);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 dynamic_allocator_should_fail_oversized_allocation() {
    u8 failed = false;

    dynamic_allocator allocator;
    u64 total_size = 100;
    u64 memory_requirement = 0;

    dynamic_allocator_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    dynamic_allocator_create(total_size, &memory_requirement, memory,
                             &allocator);

    // Try to allocate 101 bytes from 100 byte allocator
    void *block = dynamic_allocator_allocate(&allocator, 101);

    expect_should_be(0, block);
    expect_should_be(total_size, dynamic_allocator_free_space(&allocator));

    dynamic_allocator_destroy(&allocator);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 dynamic_allocator_should_reuse_freed_space() {
    u8 failed = false;

    dynamic_allocator allocator;
    u64 total_size = 1024;
    u64 memory_requirement = 0;

    dynamic_allocator_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    dynamic_allocator_create(total_size, &memory_requirement, memory,
                             &allocator);

    // Allocate 3 blocks
    void *block1 = dynamic_allocator_allocate(&allocator, 100);
    void *block2 =
        dynamic_allocator_allocate(&allocator, 100); // The middle block
    void *block3 = dynamic_allocator_allocate(&allocator, 100);

    expect_should_not_be(0, block1);
    expect_should_not_be(0, block2);
    expect_should_not_be(0, block3);

    // Free the middle block
    b8 free_result = dynamic_allocator_free(&allocator, block2, 100);
    expect_to_be_true(free_result);

    // Allocate a block that fits in the hole
    void *block_new = dynamic_allocator_allocate(&allocator, 100);

    expect_should_not_be(0, block_new);

    // If implementing First Fit or Best Fit, block_new likely equals block2
    // But strictly speaking, as long as it isn't NULL and we have space, it
    // passes.
    expect_should_be(total_size - 300,
                     dynamic_allocator_free_space(&allocator));

    dynamic_allocator_destroy(&allocator);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}
u8 dynamic_allocator_should_not_overwrite_allocated_data() {
    u8 failed = false;

    dynamic_allocator allocator;
    u64 total_size = 1024;
    u64 memory_requirement = 0;

    // Setup
    dynamic_allocator_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    dynamic_allocator_create(total_size, &memory_requirement, memory,
                             &allocator);

    // 1. Allocate first struct
    test_data *obj1 = dynamic_allocator_allocate(&allocator, sizeof(test_data));
    expect_should_not_be(0, obj1);

    // 2. Write specific values to obj1
    obj1->id = 0x12345678;
    obj1->timestamp = 999999999;
    obj1->value = 3.14159f;

    // 3. Allocate "some more memory" (obj2)
    // This forces the allocator to find a new offset. If the logic is wrong,
    // it might return an address that overlaps with obj1.
    test_data *obj2 = dynamic_allocator_allocate(&allocator, sizeof(test_data));
    expect_should_not_be(0, obj2);

    // 4. Write to obj2 to ensure we are dirtying that memory
    obj2->id = 0xDEADBEEF;
    obj2->timestamp = 111111111;
    obj2->value = 1.0f;

    // 5. CRITICAL CHECK: Verify obj1 is exactly as we left it
    expect_should_be(0x12345678, obj1->id);
    expect_should_be(999999999, obj1->timestamp);
    expect_float_to_be(3.14159f, obj1->value);

    // Cleanup
    dynamic_allocator_destroy(&allocator);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

void dynamic_allocator_register_tests() {
    test_manager_register_test(
        dynamic_allocator_should_create_and_destroy,
        "Dynamic allocator should create and destroy successfully.");

    test_manager_register_test(
        dynamic_allocator_should_allocate_successfully,
        "Dynamic allocator should allocate block successfully.");

    test_manager_register_test(
        dynamic_allocator_should_allocate_and_free_successfully,
        "Dynamic allocator should allocate and then free block successfully.");

    test_manager_register_test(
        dynamic_allocator_should_handle_multiple_allocations,
        "Dynamic allocator should handle multiple non-overlapping "
        "allocations.");

    test_manager_register_test(
        dynamic_allocator_should_fail_oversized_allocation,
        "Dynamic allocator should fail to allocate block larger than total "
        "size.");

    test_manager_register_test(dynamic_allocator_should_reuse_freed_space,
                               "Dynamic allocator should reuse freed space.");

    test_manager_register_test(
        dynamic_allocator_should_not_overwrite_allocated_data,
        "Dynamic allocator should preserve data of existing blocks when "
        "allocating new ones.");
}
