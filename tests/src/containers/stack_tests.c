#include "stack_tests.h"

#include "../expect.h"
#include "../test_manager.h"
#include "core/kmemory.h"
#include "core/logger.h"

#include <containers/stack.h>
#include <defines.h>

u8 stack_should_create_and_destroy() {
    u8 failed = false;

    stack s;
    // Allocate space for 10 u64 items
    u64 capacity = 10 * sizeof(u64);
    void *memory = kallocate(capacity, MEMORY_TAG_ARRAY);

    // 1. Create
    b8 result = stack_create(capacity, memory, &s);
    expect_to_be_true(result);

    // Verify internal state
    expect_should_not_be(0, s.memory);
    expect_should_be(capacity, s.memory_size);
    expect_should_be(0, s.allocated);

    // 2. Destroy
    stack_destroy(&s);

    // Cleanup test memory
    kfree(memory, capacity, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 stack_should_push_successfully() {
    u8 failed = false;

    stack s;
    u64 capacity = 5 * sizeof(u64);
    void *memory = kallocate(capacity, MEMORY_TAG_ARRAY);
    stack_create(capacity, memory, &s);

    // 1. Push a value
    b8 result = stack_push(&s, 100);

    expect_to_be_true(result);
    // Allocated size should have increased by sizeof(u64)
    expect_should_be(sizeof(u64), s.allocated);

    stack_destroy(&s);
    kfree(memory, capacity, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 stack_should_push_and_pop_successfully() {
    u8 failed = false;

    stack s;
    u64 capacity = 5 * sizeof(u64);
    void *memory = kallocate(capacity, MEMORY_TAG_ARRAY);
    stack_create(capacity, memory, &s);

    u64 val_to_push = 500;
    u64 popped_val = 0;

    // 1. Push
    expect_to_be_true(stack_push(&s, val_to_push));

    // 2. Pop
    b8 result = stack_pop(&s, &popped_val);

    expect_to_be_true(result);
    expect_should_be(val_to_push, popped_val);

    // Stack should be empty again
    expect_should_be(0, s.allocated);

    stack_destroy(&s);
    kfree(memory, capacity, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 stack_should_handle_multiple_pushes() {
    u8 failed = false;

    stack s;
    u64 capacity = 5 * sizeof(u64);
    void *memory = kallocate(capacity, MEMORY_TAG_ARRAY);
    stack_create(capacity, memory, &s);

    u64 val1 = 10;
    u64 val2 = 20;
    u64 val3 = 30;
    u64 popped = 0;

    // 1. Push in order 10 -> 20 -> 30
    expect_to_be_true(stack_push(&s, val1));
    expect_to_be_true(stack_push(&s, val2));
    expect_to_be_true(stack_push(&s, val3));

    expect_should_be(3 * sizeof(u64), s.allocated);

    // 2. Pop (LIFO - Last In First Out)

    // Expect 30
    stack_pop(&s, &popped);
    expect_should_be(val3, popped);

    // Expect 20
    stack_pop(&s, &popped);
    expect_should_be(val2, popped);

    // Expect 10
    stack_pop(&s, &popped);
    expect_should_be(val1, popped);

    // 3. Ensure empty
    expect_should_be(0, s.allocated);

    stack_destroy(&s);
    kfree(memory, capacity, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 stack_should_fail_overpushed() {
    u8 failed = false;

    stack s;
    // Create a stack that can only hold EXACTLY 1 item
    u64 capacity = 1 * sizeof(u64);
    void *memory = kallocate(capacity, MEMORY_TAG_ARRAY);
    stack_create(capacity, memory, &s);

    // 1. Push first item (Full)
    expect_to_be_true(stack_push(&s, 100));

    // 2. Attempt push second item (Overflow)
    b8 result = stack_push(&s, 200);

    expect_to_be_false(result);

    // Size should still be 1 item, not 2
    expect_should_be(sizeof(u64), s.allocated);

    stack_destroy(&s);
    kfree(memory, capacity, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 stack_should_clear_successfully() {
    u8 failed = false;

    stack s;
    u64 capacity = 5 * sizeof(u64);
    void *memory = kallocate(capacity, MEMORY_TAG_ARRAY);
    stack_create(capacity, memory, &s);

    // 1. Push some data
    stack_push(&s, 11);
    stack_push(&s, 22);

    expect_should_not_be(0, s.allocated);

    // 2. Clear
    stack_clear(&s);

    // 3. Verify reset
    expect_should_be(0, s.allocated);

    // 4. Verify we can push again
    expect_to_be_true(stack_push(&s, 33));
    expect_should_be(sizeof(u64), s.allocated);

    stack_destroy(&s);
    kfree(memory, capacity, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

void stack_register_tests() {
    test_manager_register_test(stack_should_create_and_destroy,
                               "Stack should create and destroy successfully.");

    test_manager_register_test(stack_should_push_successfully,
                               "Stack should push value successfully.");

    test_manager_register_test(
        stack_should_push_and_pop_successfully,
        "Stack should push and then pop value successfully.");

    test_manager_register_test(stack_should_handle_multiple_pushes,
                               "Stack should handle multiple pushes.");

    test_manager_register_test(
        stack_should_fail_overpushed,
        "Stack should fail to push more values than has size.");

    test_manager_register_test(stack_should_clear_successfully,
                               "Stack should clear and reset all memory.");
}
