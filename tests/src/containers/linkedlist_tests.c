#include "linkedlist_tests.h"
#include <containers/linkedlist.h>

#include "../expect.h"
#include "../test_manager.h"
#include "core/kmemory.h"
#include "core/logger.h"

#include <defines.h>

// Helper to handle the common setup logic to reduce repetition
// Returns the allocated memory block so it can be freed later
static void *setup_list(linkedlist *list, u64 max_nodes, u64 *mem_req_out) {
    u64 memory_requirement = 0;
    linkedlist_create(max_nodes, &memory_requirement, 0, 0);
    
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    linkedlist_create(max_nodes, &memory_requirement, memory, list);
    
    if (mem_req_out) {
        *mem_req_out = memory_requirement;
    }
    return memory;
}

u8 linkedlist_should_create_and_destroy() {
    u8 failed = false;

    linkedlist list;
    u64 max_nodes = 10;
    u64 memory_requirement = 0;

    // 1. Get memory requirement
    linkedlist_create(max_nodes, &memory_requirement, 0, 0);
    expect_should_not_be(0, memory_requirement);

    // 2. Allocate and create
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    b8 result = linkedlist_create(max_nodes, &memory_requirement, memory, &list);
    
    expect_to_be_true(result);
    expect_should_be(0, linkedlist_length(&list));

    // 3. Destroy
    // The API returns the memory block, we verify it matches what we allocated
    void *returned_mem = linkedlist_destroy(&list);
    expect_should_be((u64)memory, (u64)returned_mem);

    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 linkedlist_should_push_and_pop_tail() {
    u8 failed = false;
    linkedlist list;
    u64 mem_req = 0;
    void *memory = setup_list(&list, 5, &mem_req);

    u64 val1 = 100;
    u64 val2 = 200;

    // Push Tail
    expect_to_be_true(linkedlist_push_tail(&list, &val1));
    expect_should_be(1, linkedlist_length(&list));
    
    expect_to_be_true(linkedlist_push_tail(&list, &val2));
    expect_should_be(2, linkedlist_length(&list));

    // Pop Tail (LIFO from the tail end, effectively stack behavior if only using tail)
    void *popped = linkedlist_pop_tail(&list);
    expect_should_be((u64)&val2, (u64)popped);
    expect_should_be(1, linkedlist_length(&list));

    popped = linkedlist_pop_tail(&list);
    expect_should_be((u64)&val1, (u64)popped);
    expect_should_be(0, linkedlist_length(&list));

    linkedlist_destroy(&list);
    kfree(memory, mem_req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 linkedlist_should_push_and_pop_head() {
    u8 failed = false;
    linkedlist list;
    u64 mem_req = 0;
    void *memory = setup_list(&list, 5, &mem_req);

    u64 val1 = 10;
    u64 val2 = 20;

    // Push Head
    expect_to_be_true(linkedlist_push_head(&list, &val1)); // [10]
    expect_to_be_true(linkedlist_push_head(&list, &val2)); // [20, 10]

    expect_should_be(2, linkedlist_length(&list));

    // Check index 0 (Head)
    void *head_val = linkedlist_get_at(&list, 0);
    expect_should_be((u64)&val2, (u64)head_val);

    // Pop Head
    void *popped = linkedlist_pop_head(&list); // Should be 20
    expect_should_be((u64)&val2, (u64)popped);
    expect_should_be(1, linkedlist_length(&list));

    popped = linkedlist_pop_head(&list); // Should be 10
    expect_should_be((u64)&val1, (u64)popped);
    expect_should_be(0, linkedlist_length(&list));

    linkedlist_destroy(&list);
    kfree(memory, mem_req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 linkedlist_should_insert_and_remove_at_index() {
    u8 failed = false;
    linkedlist list;
    u64 mem_req = 0;
    void *memory = setup_list(&list, 10, &mem_req);

    u64 v1 = 1, v2 = 2, v3 = 3;

    // Setup: [1, 3]
    linkedlist_push_tail(&list, &v1);
    linkedlist_push_tail(&list, &v3);

    // Insert at index 1: [1, 2, 3]
    expect_to_be_true(linkedlist_insert_at(&list, 1, &v2));
    expect_should_be(3, linkedlist_length(&list));

    // Verify order
    expect_should_be((u64)&v1, (u64)linkedlist_get_at(&list, 0));
    expect_should_be((u64)&v2, (u64)linkedlist_get_at(&list, 1));
    expect_should_be((u64)&v3, (u64)linkedlist_get_at(&list, 2));

    // Remove at index 1 (Middle): [1, 3]
    void *removed = linkedlist_remove_at(&list, 1);
    expect_should_be((u64)&v2, (u64)removed);
    expect_should_be(2, linkedlist_length(&list));

    // Verify remaining
    expect_should_be((u64)&v1, (u64)linkedlist_get_at(&list, 0));
    expect_should_be((u64)&v3, (u64)linkedlist_get_at(&list, 1));

    linkedlist_destroy(&list);
    kfree(memory, mem_req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 linkedlist_should_fail_when_full() {
    u8 failed = false;
    linkedlist list;
    u64 mem_req = 0;
    u64 max_nodes = 3;
    void *memory = setup_list(&list, max_nodes, &mem_req);

    u64 v = 0;
    // Fill list
    expect_to_be_true(linkedlist_push_tail(&list, &v));
    expect_to_be_true(linkedlist_push_tail(&list, &v));
    expect_to_be_true(linkedlist_push_tail(&list, &v));
    
    expect_should_be(max_nodes, linkedlist_length(&list));

    // Attempt push on full list
    expect_to_be_false(linkedlist_push_tail(&list, &v));
    expect_to_be_false(linkedlist_push_head(&list, &v));
    expect_to_be_false(linkedlist_insert_at(&list, 1, &v));

    expect_should_be(max_nodes, linkedlist_length(&list));

    linkedlist_destroy(&list);
    kfree(memory, mem_req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 linkedlist_should_handle_out_of_bounds() {
    u8 failed = false;
    linkedlist list;
    u64 mem_req = 0;
    void *memory = setup_list(&list, 5, &mem_req);

    u64 v = 10;
    linkedlist_push_tail(&list, &v); // Length 1

    // Get out of bounds
    expect_should_be(0, (u64)linkedlist_get_at(&list, 1));
    expect_should_be(0, (u64)linkedlist_get_at(&list, 99));

    // Remove out of bounds
    expect_should_be(0, (u64)linkedlist_remove_at(&list, 1));

    // Insert out of bounds (assuming API denies gap creation)
    expect_to_be_false(linkedlist_insert_at(&list, 5, &v));

    linkedlist_destroy(&list);
    kfree(memory, mem_req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 linkedlist_should_reset_successfully() {
    u8 failed = false;
    linkedlist list;
    u64 mem_req = 0;
    void *memory = setup_list(&list, 5, &mem_req);

    u64 v = 1;
    linkedlist_push_tail(&list, &v);
    linkedlist_push_tail(&list, &v);
    
    expect_should_be(2, linkedlist_length(&list));

    // Reset
    linkedlist_reset(&list);

    expect_should_be(0, linkedlist_length(&list));

    // Ensure we can use it again
    expect_to_be_true(linkedlist_push_tail(&list, &v));
    expect_should_be(1, linkedlist_length(&list));

    linkedlist_destroy(&list);
    kfree(memory, mem_req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 linkedlist_should_iterate_correctly() {
    u8 failed = false;
    linkedlist list;
    u64 mem_req = 0;
    void *memory = setup_list(&list, 10, &mem_req);

    u64 values[] = {10, 20, 30};
    linkedlist_push_tail(&list, &values[0]);
    linkedlist_push_tail(&list, &values[1]);
    linkedlist_push_tail(&list, &values[2]);

    linkedlist_iterator it;
    
    // 1. Begin
    b8 has_items = linkedlist_iterator_begin(&list, &it);
    expect_to_be_true(has_items);
    
    // Check first item
    void *data = linkedlist_iterator_get(&it);
    expect_should_be((u64)&values[0], (u64)data);

    // 2. Next
    b8 has_next = linkedlist_iterator_next(&it);
    expect_to_be_true(has_next);
    data = linkedlist_iterator_get(&it);
    expect_should_be((u64)&values[1], (u64)data);

    // 3. Next
    has_next = linkedlist_iterator_next(&it);
    expect_to_be_true(has_next);
    data = linkedlist_iterator_get(&it);
    expect_should_be((u64)&values[2], (u64)data);

    // 4. End of list
    has_next = linkedlist_iterator_next(&it);
    expect_to_be_false(has_next); // Should return false at end

    linkedlist_destroy(&list);
    kfree(memory, mem_req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

void linkedlist_register_tests() {
    test_manager_register_test(
        linkedlist_should_create_and_destroy,
        "Linkedlist should create and destroy successfully.");

    test_manager_register_test(
        linkedlist_should_push_and_pop_tail,
        "Linkedlist should push and pop from tail (queue/stack check).");

    test_manager_register_test(
        linkedlist_should_push_and_pop_head,
        "Linkedlist should push and pop from head.");

    test_manager_register_test(
        linkedlist_should_insert_and_remove_at_index,
        "Linkedlist should insert and remove at specific indices.");

    test_manager_register_test(
        linkedlist_should_fail_when_full,
        "Linkedlist should fail allocation when max nodes reached.");
    
    test_manager_register_test(
        linkedlist_should_handle_out_of_bounds,
        "Linkedlist should fail gracefully on out of bounds access.");

    test_manager_register_test(
        linkedlist_should_reset_successfully,
        "Linkedlist should reset and allow reuse.");

    test_manager_register_test(
        linkedlist_should_iterate_correctly,
        "Linkedlist iterator should traverse all nodes in order.");
}
