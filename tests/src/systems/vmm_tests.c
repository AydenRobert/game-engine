#include "vmm_tests.h"
#include <systems/vmm_system.h>

#include "../expect.h"
#include "../test_manager.h"
#include <defines.h>

// --- Helpers ---

// Helper to start VMM with standard settings for testing
static b8 setup_vmm() {
    vmm_config config = {0};
    config.max_memory_reserved = 1024 * 1024 * 256; // 256 MB
    config.max_memory_mapped = 1024 * 1024 * 64;    // 64 MB
    config.max_pool_amount = 10;

    return vmm_initialise(config);
}

// --- Tests ---

u8 vmm_should_initialise_and_shutdown() {
    u8 failed = false;

    vmm_config config = {0};
    config.max_memory_reserved = 1024 * 1024 * 10;
    config.max_memory_mapped = 1024 * 1024 * 1;
    config.max_pool_amount = 2;

    // 1. Initialise
    b8 result = vmm_initialise(config);
    expect_to_be_true(result);

    // Verify internal state (page size should be set)
    expect_should_not_be(0, vmm_page_size());

    // 2. Shutdown
    // We can't easily verify internal state is cleared without exposing it,
    // but we can verify it doesn't crash and potentially allows re-init.
    vmm_shutdown();

    // 3. Re-initialise (Sanity check that shutdown cleared the state)
    result = vmm_initialise(config);
    expect_to_be_true(result);

    // Clean up
    vmm_shutdown();

    return failed ? false : true;
}

u8 vmm_should_create_and_destroy_pool() {
    u8 failed = false;
    if (!setup_vmm())
        return false; // Fail if setup fails

    u64 page_size = vmm_page_size();
    u64 pool_req_size = page_size * 10;

    // 1. Create
    memory_pool *pool = vmm_new_page_pool(pool_req_size);
    expect_should_not_be(0, (u64)pool);

    if (pool) {
        expect_should_not_be(0, (u64)pool->base_address);
        expect_should_be(pool_req_size, pool->memory_reserved);
    }

    // 2. Release
    b8 result = vmm_release_page_pool(pool);
    expect_to_be_true(result);

    vmm_shutdown(); // Clean up system
    return failed ? false : true;
}

u8 vmm_should_commit_and_decommit_pages() {
    u8 failed = false;
    setup_vmm();

    u64 page_size = vmm_page_size();
    memory_pool *pool = vmm_new_page_pool(page_size * 10);

    commit_info info = {0};

    // 1. Commit
    b8 result = vmm_commit_pages(pool, 0, page_size * 2, &info);
    expect_to_be_true(result);
    expect_should_be(page_size * 2, pool->memory_mapped);

    // 2. Decommit
    result = vmm_decommit_pages(pool, 0, page_size * 2, &info);
    expect_to_be_true(result);
    expect_should_be(0, pool->memory_mapped);

    vmm_release_page_pool(pool);
    vmm_shutdown();
    return failed ? false : true;
}

u8 vmm_should_handle_rounding() {
    u8 failed = false;
    setup_vmm();

    u64 page_size = vmm_page_size();
    u64 odd_size = page_size + 128;

    // 1. Pool creation rounding
    memory_pool *pool = vmm_new_page_pool(odd_size);
    expect_should_be(page_size * 2, pool->memory_reserved);

    // 2. Commit rounding (Request 100 bytes -> get 1 page)
    commit_info info = {0};
    vmm_commit_pages(pool, 0, 100, &info);
    expect_should_be(page_size, info.size);

    vmm_release_page_pool(pool);
    vmm_shutdown();
    return failed ? false : true;
}

u8 vmm_should_fail_invalid_commit() {
    u8 failed = false;
    setup_vmm();

    u64 page_size = vmm_page_size();
    memory_pool *pool = vmm_new_page_pool(page_size * 2);
    commit_info info = {0};

    // Out of bounds checks
    expect_to_be_false(vmm_commit_pages(pool, page_size * 5, page_size, &info));
    expect_to_be_false(vmm_commit_pages(pool, page_size, page_size * 5, &info));

    vmm_release_page_pool(pool);
    vmm_shutdown();
    return failed ? false : true;
}

u8 vmm_should_handle_swiss_cheese_memory() {
    u8 failed = false;
    setup_vmm();

    u64 page_size = vmm_page_size();
    // Create a pool of 5 pages
    memory_pool *pool = vmm_new_page_pool(page_size * 5);
    commit_info info = {0};

    // 1. Commit all 5 pages
    vmm_commit_pages(pool, 0, page_size * 5, &info);

    // 2. Decommit the even pages (0, 2, 4) - poking holes
    vmm_decommit_pages(pool, 0, page_size, &info);             // Page 0
    vmm_decommit_pages(pool, page_size * 2, page_size, &info); // Page 2
    vmm_decommit_pages(pool, page_size * 4, page_size, &info); // Page 4

    // Expected mapped: Page 1 and 3 remain.
    // Total mapped size should be 2 pages.
    expect_should_be(page_size * 2, pool->memory_mapped);

    // 3. Re-commit the middle hole (Page 2)
    b8 result = vmm_commit_pages(pool, page_size * 2, page_size, &info);
    expect_to_be_true(result);
    expect_should_be(page_size * 3, pool->memory_mapped);

    // 4. Try to decommit a hole that is already decommitted (Page 0)
    // Depending on your implementation, this might return true (no-op) or false
    // (error). It definitely shouldn't crash or underflow `memory_mapped`.
    result = vmm_decommit_pages(pool, 0, page_size, &info);

    // If your logic is robust, memory_mapped shouldn't change
    expect_should_be(page_size * 3, pool->memory_mapped);

    vmm_release_page_pool(pool);
    vmm_shutdown();
    return failed ? false : true;
}

u8 vmm_should_handle_overlapping_commits() {
    u8 failed = false;
    setup_vmm();

    u64 page_size = vmm_page_size();
    memory_pool *pool = vmm_new_page_pool(page_size * 4);
    commit_info info = {0};

    // 1. Commit the first 2 pages
    vmm_commit_pages(pool, 0, page_size * 2, &info);
    expect_should_be(page_size * 2, pool->memory_mapped);

    // 2. Commit pages 1 and 2 (Overlapping page 1, new page 2)
    // Range: [Page 1, Page 2]
    // Page 1 is already committed. Page 2 is new.
    b8 result = vmm_commit_pages(pool, page_size, page_size * 2, &info);

    expect_to_be_true(result);

    // If your VMM is simple, it might count the re-commit as new mapped memory
    // (which is a bug in tracking, though maybe not a crash).
    // Ideally, memory_mapped should now represent 3 pages total (0, 1, 2).
    expect_should_be(page_size * 3, pool->memory_mapped);

    vmm_release_page_pool(pool);
    vmm_shutdown();
    return failed ? false : true;
}

u8 vmm_should_survive_rapid_allocation_cycles() {
    u8 failed = false;
    setup_vmm(); // Configured with max_pool_amount = 10

    u64 page_size = vmm_page_size();

    // Loop 100 times. Since max pools is 10, if you leak a pool handle
    // or don't reset the counter, this loop will fail at iteration 11.
    for (int i = 0; i < 100; ++i) {
        memory_pool *pool = vmm_new_page_pool(page_size);

        if (pool == 0) {
            KERROR("Allocation failed at iteration: %d", i);
            expect_to_be_true(false); // Force fail
            break;
        }

        // Do some work
        commit_info info;
        vmm_commit_pages(pool, 0, page_size, &info);

        // Kill it
        vmm_release_page_pool(pool);
    }

    vmm_shutdown();
    return failed ? false : true;
}

void vmm_register_tests() {
    test_manager_register_test(
        vmm_should_initialise_and_shutdown,
        "VMM should initialise and shutdown (Lifecycle).");

    test_manager_register_test(vmm_should_create_and_destroy_pool,
                               "VMM should create and release memory pools.");

    test_manager_register_test(
        vmm_should_commit_and_decommit_pages,
        "VMM should commit and decommit pages correctly.");

    test_manager_register_test(vmm_should_handle_rounding,
                               "VMM should round sizes up to nearest page.");

    test_manager_register_test(
        vmm_should_fail_invalid_commit,
        "VMM should fail on out-of-bounds commit attempts.");

    test_manager_register_test(vmm_should_handle_swiss_cheese_memory,
                               "VMM: Swiss Cheese (fragmentation) test");

    test_manager_register_test(vmm_should_handle_overlapping_commits,
                               "VMM: Overlapping commit check");

    test_manager_register_test(vmm_should_survive_rapid_allocation_cycles,
                               "VMM: Rapid allocation/free cycle (leak check)");
}
