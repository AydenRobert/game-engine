#include "memory_system_tests.h"

#include <defines.h>
#include <systems/memory_system.h>
#include <systems/vmm_system.h>

#include "../expect.h"
#include "../test_manager.h"

// --- Helpers ---

// Helper to start BOTH VMM and Memory System
// VMM must be valid for Memory System to work.
static b8 setup_memory_system() {
    // 1. Initialise VMM (The foundation)
    vmm_config vmm_cfg = {0};
    vmm_cfg.max_memory_reserved = 1024 * 1024 * 256; // 256 MB
    vmm_cfg.max_memory_mapped = 1024 * 1024 * 64;    // 64 MB
    vmm_cfg.max_pool_amount = 100;

    if (!vmm_initialise(vmm_cfg)) {
        KERROR("Failed to initialise VMM in test setup.");
        return false;
    }

    // 2. Initialise Memory System
    memory_system_config config = {0};
    config.initial_allocated = 1024 * 1024 * 10; // 10 MB
    config.max_memory = 1024 * 1024 * 64;        // 64 MB
    config.max_allocations = 100;

    return memory_system_initialize(config);
}

// Helper to tear down everything in reverse order
static void teardown_memory_system() {
    memory_system_shutdown_tempname();
    vmm_shutdown();
}

// --- Tests ---

u8 memory_should_initialise_and_shutdown() {
    u8 failed = false;

    // We must manually start VMM for this specific lifecycle test
    vmm_config vmm_cfg = {0};
    vmm_cfg.max_memory_reserved = 1024 * 1024 * 256;
    vmm_cfg.max_memory_mapped = 1024 * 1024 * 64;
    vmm_cfg.max_pool_amount = 10;
    vmm_initialise(vmm_cfg);

    memory_system_config config = {0};
    config.initial_allocated = 1024 * 1024;
    config.max_memory = 1024 * 1024 * 10;
    config.max_allocations = 10;

    // 1. Initialise Memory System
    b8 result = memory_system_initialize(config);
    expect_to_be_true(result);

    // 2. Shutdown Memory System
    memory_system_shutdown_tempname();

    // 3. Re-initialise Memory System (Sanity check)
    result = memory_system_initialize(config);
    expect_to_be_true(result);

    // Final Cleanup
    memory_system_shutdown_tempname();
    vmm_shutdown();

    return failed ? false : true;
}

u8 memory_should_allocate_reserved() {
    u8 failed = false;
    if (!setup_memory_system())
        return false;

    u64 req_size = 1024 * 10; // 10KB

    // 1. Allocate Reserved (Address space only)
    void *block = allocate_reserved(req_size);
    expect_should_not_be(0, (u64)block);

    // 2. Validate Metadata
    allocation *alloc = 0;
    b8 found = allocation_get_struct(block, &alloc);
    expect_to_be_true(found);

    if (found) {
        expect_should_be(req_size, alloc->size);
        expect_should_not_be(0, (u64)alloc->pool);
    }

    allocation_free(block);
    teardown_memory_system();

    return failed ? false : true;
}

u8 memory_should_allocate_committed() {
    u8 failed = false;
    if (!setup_memory_system())
        return false;

    u64 req_size = 1024 * 10; // 10KB

    // 1. Allocate Committed (Address space + Physical RAM)
    void *block = allocate_commited(req_size);
    expect_should_not_be(0, (u64)block);

    allocation *alloc = 0;
    allocation_get_struct(block, &alloc);

    // 2. Verify pool statistics
    if (alloc->pool) {
        // Mapped size should be >= req_size (due to page alignment)
        b8 is_enough_mapped = (alloc->pool->memory_mapped >= req_size);
        expect_to_be_true(is_enough_mapped);
    }

    allocation_free(block);
    teardown_memory_system();

    return failed ? false : true;
}

u8 memory_should_ensure_committed_pages() {
    u8 failed = false;
    setup_memory_system();

    // 1. Create a reserved block
    // We allocate enough space to definitely span multiple pages
    void *block = allocate_reserved(1024 * 1024);

    allocation *alloc = 0;
    allocation_get_struct(block, &alloc);

    // 2. Commit pages 0 and 1 (indices)
    b8 result = allocation_ensure_commited_pages(block, 0, 2);
    expect_to_be_true(result);

    // Access the memory
    ((u64*)block)[0] = 0;
    ((u64*)block)[512] = 0;

    // 4. Idempotency check (Ensure same pages again)
    result = allocation_ensure_commited_pages(block, 0, 2);
    expect_to_be_true(result);

    allocation_free(block);
    teardown_memory_system();
    return failed ? false : true;
}

u8 memory_should_ensure_committed_bytes_and_round() {
    u8 failed = false;
    setup_memory_system();

    void *block = allocate_reserved(1024 * 1024); // 1MB reserved
    allocation *alloc = 0;
    allocation_get_struct(block, &alloc);
    u64 page_size = alloc->pool->page_size;

    // 1. Ensure committed by byte offset
    // Offset 0, size 10 bytes -> Should force 1 page commit
    b8 result = allocation_ensure_commited(block, 0, 10);
    expect_to_be_true(result);

    expect_should_not_be(0, alloc->pool->memory_mapped);

    // 2. Ensure committed crossing page boundary
    // Start at byte 0, go to page_size + 10 bytes -> Should force 2 pages total
    result = allocation_ensure_commited(block, 0, page_size + 10);
    expect_to_be_true(result);

    expect_should_not_be(0, alloc->pool->memory_mapped);

    allocation_free(block);
    teardown_memory_system();
    return failed ? false : true;
}

u8 memory_should_handle_metadata_retrieval() {
    u8 failed = false;
    setup_memory_system();

    void *block = allocate_reserved(100);
    allocation *alloc = 0;

    // 1. Valid retrieval
    expect_to_be_true(allocation_get_struct(block, &alloc));

    // 2. Invalid retrieval (Random address)
    void *bad_ptr = (void *)0x12345678;
    expect_to_be_false(allocation_get_struct(bad_ptr, &alloc));

    // 3. Retrieval after free
    allocation_free(block);
    expect_to_be_false(allocation_get_struct(block, &alloc));

    teardown_memory_system();
    return failed ? false : true;
}

u8 memory_should_not_crash_on_double_free() {
    u8 failed = false;
    setup_memory_system();

    void *block = allocate_reserved(100);

    // 1. First Free
    allocation_free(block);

    // 2. Second Free
    // Should gracefully fail/return without crashing
    allocation_free(block);

    expect_to_be_true(true);

    teardown_memory_system();
    return failed ? false : true;
}

void memory_system_register_tests() {
    test_manager_register_test(memory_should_initialise_and_shutdown,
                               "Memory System: Lifecycle (Init/Shutdown)");

    test_manager_register_test(
        memory_should_allocate_reserved,
        "Memory System: Allocate Reserved (Uncommitted)");

    test_manager_register_test(memory_should_allocate_committed,
                               "Memory System: Allocate Committed");

    test_manager_register_test(memory_should_ensure_committed_pages,
                               "Memory System: Ensure Committed (Pages)");

    test_manager_register_test(
        memory_should_ensure_committed_bytes_and_round,
        "Memory System: Ensure Committed (Bytes/Rounding)");

    test_manager_register_test(memory_should_handle_metadata_retrieval,
                               "Memory System: Metadata Retrieval");

    test_manager_register_test(memory_should_not_crash_on_double_free,
                               "Memory System: Double Free Safety");
}
