#include "linear_allocator_test.h"

#include <memory/linear_allocator.h>

#include "../expect.h"
#include "../test_manager.h"
#include "core/kmemory.h"
#include "core/logger.h"
#include <defines.h>

// Struct used for data integrity tests
typedef struct test_data {
    u32 id;
    u64 timestamp;
    f32 value;
} test_data;

u8 linear_allocator_should_create_and_destroy() {
    u8 failed = false;

    linear_allocator allocator;
    u64 total_size = 1024;
    u64 memory_requirement = 0;

    // 1. Get memory requirement
    linear_allocator_create(total_size, &memory_requirement, 0, 0);
    expect_should_not_be(0, memory_requirement);

    // 2. Allocate state memory and create
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    linear_allocator_create(total_size, &memory_requirement, memory,
                            &allocator);

    expect_should_not_be(0, allocator.memory);

    linear_allocator_destroy(&allocator);

    // Clean up the backing memory
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 linear_allocator_should_allocate_with_alignment() {
    u8 failed = false;

    linear_allocator allocator;
    u64 total_size = 512;
    u64 memory_requirement = 0;

    linear_allocator_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    linear_allocator_create(total_size, &memory_requirement, memory,
                            &allocator);

    // 1. Allocate a small chunk with 1-byte alignment (essentially no
    // alignment) This pushes the internal offset forward by 1 byte.
    void *block1 = linear_allocator_allocate(&allocator, 1, 1);
    expect_should_not_be(0, block1);

    // 2. Allocate with 8-byte alignment.
    // Since we are currently at offset 1, the allocator must pad bytes 2-7
    // and give us a pointer starting at 8 (relative to start).
    u64 alignment = 8;
    void *block2 = linear_allocator_allocate(&allocator, 64, alignment);

    expect_should_not_be(0, block2);

    // Check that the returned pointer is actually aligned
    expect_should_be(0, (u64)block2 % alignment);

    // Ensure blocks do not overlap (block2 > block1)
    expect_to_be_true(((u64)block2 > (u64)block1));

    linear_allocator_destroy(&allocator);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 linear_allocator_should_handle_multiple_allocations() {
    u8 failed = false;

    linear_allocator allocator;
    u64 total_size = 1024;
    u64 memory_requirement = 0;

    linear_allocator_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    linear_allocator_create(total_size, &memory_requirement, memory,
                            &allocator);

    // Alloc 1
    void *block1 = linear_allocator_allocate(&allocator, 100, 1);
    expect_should_not_be(0, block1);

    // Alloc 2
    void *block2 = linear_allocator_allocate(&allocator, 200, 1);
    expect_should_not_be(0, block2);

    // Alloc 3
    void *block3 = linear_allocator_allocate(&allocator, 300, 1);
    expect_should_not_be(0, block3);

    // Ensure pointers are distinct and sequential (assuming flat memory)
    expect_should_not_be(block1, block2);
    expect_should_not_be(block2, block3);

    // Check strict linear progression
    expect_to_be_true(((u64)block2 >= (u64)block1 + 100));
    expect_to_be_true(((u64)block3 >= (u64)block2 + 200));

    linear_allocator_destroy(&allocator);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 linear_allocator_should_fail_oversized_allocation() {
    u8 failed = false;

    linear_allocator allocator;
    u64 total_size = 100;
    u64 memory_requirement = 0;

    linear_allocator_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    linear_allocator_create(total_size, &memory_requirement, memory,
                            &allocator);

    // 1. Fill most of the buffer
    void *block1 = linear_allocator_allocate(&allocator, 80, 1);
    expect_should_not_be(0, block1);

    // 2. Try to allocate more than remains (Remaining: ~20, Request: 30)
    void *block2 = linear_allocator_allocate(&allocator, 30, 1);
    expect_should_be(0, block2);

    // 3. Try to allocate way more than total size
    void *block3 = linear_allocator_allocate(&allocator, 200, 1);
    expect_should_be(0, block3);

    linear_allocator_destroy(&allocator);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 linear_allocator_should_reset_on_free_all() {
    u8 failed = false;

    linear_allocator allocator;
    u64 total_size = 1024;
    u64 memory_requirement = 0;

    linear_allocator_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    linear_allocator_create(total_size, &memory_requirement, memory,
                            &allocator);

    // 1. Allocate something
    void *block1 = linear_allocator_allocate(&allocator, 512, 1);
    expect_should_not_be(0, block1);

    // 2. Free all (reset)
    linear_allocator_free_all(&allocator);

    // 3. Allocate again
    void *block2 = linear_allocator_allocate(&allocator, 512, 1);
    expect_should_not_be(0, block2);

    // Since we reset, block2 should point to the same location as block1
    // (assuming the implementation resets the internal offset to 0)
    expect_should_be(block1, block2);

    linear_allocator_destroy(&allocator);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 linear_allocator_should_preserve_data() {
    u8 failed = false;

    linear_allocator allocator;
    u64 total_size = 1024;
    u64 memory_requirement = 0;

    linear_allocator_create(total_size, &memory_requirement, 0, 0);
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    linear_allocator_create(total_size, &memory_requirement, memory,
                            &allocator);

    // 1. Allocate struct
    test_data *obj1 =
        linear_allocator_allocate(&allocator, sizeof(test_data), 8);
    expect_should_not_be(0, obj1);

    // 2. Write data
    obj1->id = 0xAA;
    obj1->timestamp = 12345;
    obj1->value = 1.23f;

    // 3. Allocate more memory to advance pointer
    test_data *obj2 =
        linear_allocator_allocate(&allocator, sizeof(test_data), 8);
    expect_should_not_be(0, obj2);

    obj2->id = 0xBB; // Write to new block to ensure no overlap

    // 4. Check integrity of first block
    expect_should_be(0xAA, obj1->id);
    expect_should_be(12345, obj1->timestamp);
    expect_float_to_be(1.23f, obj1->value);

    linear_allocator_destroy(&allocator);
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

void linear_allocator_register_tests() {
    test_manager_register_test(
        linear_allocator_should_create_and_destroy,
        "Linear allocator should create and destroy successfully.");

    test_manager_register_test(
        linear_allocator_should_allocate_with_alignment,
        "Linear allocator should allocate with correct memory alignment.");

    test_manager_register_test(
        linear_allocator_should_handle_multiple_allocations,
        "Linear allocator should allocate multiple contiguous blocks.");

    test_manager_register_test(
        linear_allocator_should_fail_oversized_allocation,
        "Linear allocator should return 0 when out of memory.");

    test_manager_register_test(
        linear_allocator_should_reset_on_free_all,
        "Linear allocator should reuse memory range after free_all.");

    test_manager_register_test(
        linear_allocator_should_preserve_data,
        "Linear allocator should preserve data integrity across allocations.");
}
