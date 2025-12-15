#include "binarytree_tests.h"

#include "../expect.h"
#include "../test_manager.h"

#include "core/kmemory.h"
#include "core/logger.h"

#include <containers/binarytree.h>
#include <defines.h>

static u8 binarytree_should_create_and_destroy() {
    u8 failed = false;

    binarytree tree;
    u64 memory_requirement = 0;
    u32 max_nodes = 16;

    // 1. Request memory requirement
    b8 result = binarytree_create(max_nodes, &memory_requirement, 0, 0);
    expect_to_be_true(result);
    expect_should_not_be(0, memory_requirement);

    // 2. Allocate memory and create
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    result = binarytree_create(max_nodes, &memory_requirement, memory, &tree);
    expect_to_be_true(result);
    expect_should_not_be(0, tree.internal_state);

    // 3. Destroy tree
    void *returned_memory = binarytree_destroy(&tree);
    expect_should_be((u64)returned_memory, (u64)memory);

    // Clean up
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

static u8 binarytree_should_insert_and_search_successfully() {
    u8 failed = false;

    binarytree tree;
    u64 req = 0;
    u32 max_nodes = 8;

    binarytree_create(max_nodes, &req, 0, 0);
    void *mem = kallocate(req, MEMORY_TAG_ARRAY);
    binarytree_create(max_nodes, &req, mem, &tree);

    // Insert nodes
    expect_to_be_true(binarytree_insert(&tree, 50, 99));
    expect_to_be_true(binarytree_insert(&tree, 25, 11));
    expect_to_be_true(binarytree_insert(&tree, 75, 22));

    // Search them
    u32 found_index = 0;
    expect_to_be_true(binarytree_search(&tree, 50, &found_index));
    expect_should_be(99, found_index);
    expect_to_be_true(binarytree_search(&tree, 25, &found_index));
    expect_should_be(11, found_index);
    expect_to_be_true(binarytree_search(&tree, 75, &found_index));
    expect_should_be(22, found_index);

    // Destroy
    void *returned_mem = binarytree_destroy(&tree);
    expect_should_be((u64)returned_mem, (u64)mem);
    kfree(mem, req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

static u8 binarytree_should_refuse_duplicate_inserts() {
    u8 failed = false;

    binarytree tree;
    u64 req = 0;
    binarytree_create(4, &req, 0, 0);
    void *mem = kallocate(req, MEMORY_TAG_ARRAY);
    binarytree_create(4, &req, mem, &tree);

    expect_to_be_true(binarytree_insert(&tree, 10, 1));

    // Duplicate insert should fail
    b8 result = binarytree_insert(&tree, 10, 2);
    expect_to_be_false(result);

    // Ensure found index still matches the original
    u32 index = 0;
    expect_to_be_true(binarytree_search(&tree, 10, &index));
    expect_should_be(1, index);

    binarytree_destroy(&tree);
    kfree(mem, req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

static u8 binarytree_should_delete_leaf_nodes() {
    u8 failed = false;

    binarytree tree;
    u64 req = 0;
    binarytree_create(6, &req, 0, 0);
    void *mem = kallocate(req, MEMORY_TAG_ARRAY);
    binarytree_create(6, &req, mem, &tree);

    // Basic balanced tree
    binarytree_insert(&tree, 50, 5);
    binarytree_insert(&tree, 25, 2);
    binarytree_insert(&tree, 75, 8);

    u32 out_index = 0;

    // Delete a leaf
    expect_to_be_true(binarytree_delete(&tree, 25, &out_index));
    expect_should_be(2, out_index);

    // Searching deleted key must fail
    expect_to_be_false(binarytree_search(&tree, 25, &out_index));

    // Remaining keys must still exist
    expect_to_be_true(binarytree_search(&tree, 50, &out_index));
    expect_to_be_true(binarytree_search(&tree, 75, &out_index));

    binarytree_destroy(&tree);
    kfree(mem, req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

static u8 binarytree_should_delete_root_and_rebalance() {
    u8 failed = false;

    binarytree tree;
    u64 req = 0;
    binarytree_create(8, &req, 0, 0);
    void *mem = kallocate(req, MEMORY_TAG_ARRAY);
    binarytree_create(8, &req, mem, &tree);

    // Build non-trivial tree
    binarytree_insert(&tree, 50, 500);
    binarytree_insert(&tree, 25, 250);
    binarytree_insert(&tree, 75, 750);
    binarytree_insert(&tree, 10, 100);
    binarytree_insert(&tree, 30, 300);

    // Delete root node
    u32 deleted_val = 0;
    expect_to_be_true(binarytree_delete(&tree, 50, &deleted_val));
    expect_should_be(500, deleted_val);

    // Root replacement check: 50 should not exist anymore
    u32 find_val = 0;
    expect_to_be_false(binarytree_search(&tree, 50, &find_val));

    // Children still intact
    expect_to_be_true(binarytree_search(&tree, 25, &find_val));
    expect_to_be_true(binarytree_search(&tree, 75, &find_val));

    binarytree_destroy(&tree);
    kfree(mem, req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

static u8 binarytree_should_fail_when_exceeding_max_nodes() {
    u8 failed = false;

    binarytree tree;
    u64 req = 0;
    binarytree_create(3, &req, 0, 0);
    void *mem = kallocate(req, MEMORY_TAG_ARRAY);
    binarytree_create(3, &req, mem, &tree);

    expect_to_be_true(binarytree_insert(&tree, 10, 1));
    expect_to_be_true(binarytree_insert(&tree, 5, 2));
    expect_to_be_true(binarytree_insert(&tree, 15, 3));

    // Exceed capacity
    b8 result = binarytree_insert(&tree, 20, 4);
    expect_to_be_false(result);

    binarytree_destroy(&tree);
    kfree(mem, req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

static u8 binarytree_should_handle_search_on_empty_tree() {
    u8 failed = false;

    binarytree tree;
    u64 req = 0;
    binarytree_create(8, &req, 0, 0);
    void *mem = kallocate(req, MEMORY_TAG_ARRAY);
    binarytree_create(8, &req, mem, &tree);

    u32 out = 0;
    expect_to_be_false(binarytree_search(&tree, 1234, &out));

    binarytree_destroy(&tree);
    kfree(mem, req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

static u8 binarytree_should_handle_delete_on_nonexistent_key() {
    u8 failed = false;

    binarytree tree;
    u64 req = 0;
    binarytree_create(8, &req, 0, 0);
    void *mem = kallocate(req, MEMORY_TAG_ARRAY);
    binarytree_create(8, &req, mem, &tree);

    binarytree_insert(&tree, 42, 4242);

    u32 out_index = 0;
    b8 result = binarytree_delete(&tree, 999, &out_index);

    expect_to_be_false(result);

    // Still contain the original
    expect_to_be_true(binarytree_search(&tree, 42, &out_index));
    expect_should_be(4242, out_index);

    binarytree_destroy(&tree);
    kfree(mem, req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

void binarytree_register_tests() {
    test_manager_register_test(
        binarytree_should_create_and_destroy,
        "BinaryTree should create and destroy properly.");

    test_manager_register_test(binarytree_should_insert_and_search_successfully,
                               "BinaryTree should insert and search nodes.");

    test_manager_register_test(
        binarytree_should_refuse_duplicate_inserts,
        "BinaryTree should reject duplicate identifiers.");

    test_manager_register_test(
        binarytree_should_delete_leaf_nodes,
        "BinaryTree should delete leaf nodes correctly.");

    test_manager_register_test(
        binarytree_should_delete_root_and_rebalance,
        "BinaryTree should handle root deletions correctly.");

    test_manager_register_test(
        binarytree_should_fail_when_exceeding_max_nodes,
        "BinaryTree should fail inserts beyond capacity.");

    test_manager_register_test(
        binarytree_should_handle_search_on_empty_tree,
        "BinaryTree should return false on empty-tree search.");

    test_manager_register_test(
        binarytree_should_handle_delete_on_nonexistent_key,
        "BinaryTree should handle deletion of nonexistent key.");
}
