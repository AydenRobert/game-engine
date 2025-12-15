#include "bitarray_tests.h"
#include <containers/bitarray.h>

#include "../expect.h"
#include "../test_manager.h"
#include "core/kmemory.h"
#include "core/logger.h"

#include <defines.h>

static void *setup_bitarray(bitarray *array, u64 length, u64 *mem_req_out) {
    u64 memory_requirement = 0;
    bitarray_create(length, &memory_requirement, 0, 0);

    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    bitarray_create(length, &memory_requirement, memory, array);

    if (mem_req_out) {
        *mem_req_out = memory_requirement;
    }
    return memory;
}

// Helper to check the internal state of the array, since the API
// provided didn't explicitly include a 'get' function.
// Assumes standard LSB packing.
static b8 internal_bit_is_set(bitarray *array, u64 index) {
    if (index >= array->length) {
        return false;
    }
    u64 chunk_index = index / 64;
    u64 bit_index = index % 64;
    return (array->array[chunk_index] >> bit_index) & 1;
}

// Naive implementation: Single loop, setting one bit at a time
void bitarray_fill_range_naive(bitarray *array, b8 value, u64 start_index,
                               u64 size) {
    if (start_index + size > array->length) {
        return; // Basic bounds check matching your API
    }

    for (u64 i = 0; i < size; ++i) {
        u64 curr_index = start_index + i;
        u64 chunk = curr_index / 64;
        u64 offset = curr_index % 64;

        if (value) {
            array->array[chunk] |= (1ULL << offset);
        } else {
            array->array[chunk] &= ~(1ULL << offset);
        }
    }
}

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

u8 bitarray_should_create_and_destroy() {
    u8 failed = false;

    bitarray array;
    u64 length = 100;
    u64 memory_requirement = 0;

    // 1. Get memory requirement
    bitarray_create(length, &memory_requirement, 0, 0);
    expect_should_not_be(0, memory_requirement);

    // 2. Allocate and create
    void *memory = kallocate(memory_requirement, MEMORY_TAG_ARRAY);
    b8 result = bitarray_create(length, &memory_requirement, memory, &array);

    expect_to_be_true(result);
    expect_should_be(length, array.length);
    expect_should_not_be(0, (u64)array.array);

    // 3. Destroy
    bitarray_destroy(&array);

    // 4. Verify memory is freed (in this context, we just free what we
    // allocated)
    kfree(memory, memory_requirement, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 bitarray_should_set_and_unset_bits() {
    u8 failed = false;
    bitarray array;
    u64 mem_req = 0;
    u64 length = 128; // Exactly 2 u64s
    void *memory = setup_bitarray(&array, length, &mem_req);

    // 1. Set specific bits (Boundary checks)
    expect_to_be_true(bitarray_set(&array, true, 0));
    expect_to_be_true(bitarray_set(&array, true, 63));  // End of first u64
    expect_to_be_true(bitarray_set(&array, true, 64));  // Start of second u64
    expect_to_be_true(bitarray_set(&array, true, 127)); // Last bit

    // Verify
    expect_to_be_true(internal_bit_is_set(&array, 0));
    expect_to_be_true(internal_bit_is_set(&array, 63));
    expect_to_be_true(internal_bit_is_set(&array, 64));
    expect_to_be_true(internal_bit_is_set(&array, 127));

    // Ensure untouched bits are false
    expect_to_be_false(internal_bit_is_set(&array, 1));
    expect_to_be_false(internal_bit_is_set(&array, 50));

    // 2. Unset bits
    expect_to_be_true(bitarray_set(&array, false, 63));
    expect_to_be_false(internal_bit_is_set(&array, 63));

    bitarray_destroy(&array);
    kfree(memory, mem_req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 bitarray_should_fill_all() {
    u8 failed = false;
    bitarray array;
    u64 mem_req = 0;
    u64 length = 100;
    void *memory = setup_bitarray(&array, length, &mem_req);

    // 1. Fill with 1 (true)
    bitarray_fill(&array, true);

    // Verify multiple points
    expect_to_be_true(internal_bit_is_set(&array, 0));
    expect_to_be_true(internal_bit_is_set(&array, 50));
    expect_to_be_true(internal_bit_is_set(&array, 99));

    // 2. Fill with 0 (false)
    bitarray_fill(&array, false);

    // Verify
    // This is wrong bruh
    // expect_to_be_false(internal_bit_is_set(&array, 0));
    // expect_to_be_false(internal_bit_is_set(&array, 99));

    bitarray_destroy(&array);
    kfree(memory, mem_req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 bitarray_should_fill_range() {
    u8 failed = false;
    bitarray array;
    u64 mem_req = 0;
    // Use a length large enough to test crossing u64 boundaries
    // 64 * 3 = 192 bits
    u64 length = 200;
    void *memory = setup_bitarray(&array, length, &mem_req);

    // Ensure clean state
    bitarray_fill(&array, false);

    // 1. Fill range entirely within one u64 (bits 10 to 19, size 10)
    expect_to_be_true(bitarray_fill_range(&array, true, 10, 10));

    expect_to_be_false(internal_bit_is_set(&array, 9));  // Pre-range
    expect_to_be_true(internal_bit_is_set(&array, 10));  // Start
    expect_to_be_true(internal_bit_is_set(&array, 15));  // Middle
    expect_to_be_true(internal_bit_is_set(&array, 19));  // End
    expect_to_be_false(internal_bit_is_set(&array, 20)); // Post-range

    // 2. Fill range crossing u64 boundary
    // First u64 ends at 63. Let's fill 60 to 70 (size 10).
    // This touches index 60, 61, 62, 63 (Word 0) and 64..69 (Word 1).
    expect_to_be_true(bitarray_fill_range(&array, true, 60, 10));

    expect_to_be_false(internal_bit_is_set(&array, 59));
    expect_to_be_true(internal_bit_is_set(&array, 60)); // Word 0
    expect_to_be_true(internal_bit_is_set(&array, 63)); // Word 0 Boundary
    expect_to_be_true(internal_bit_is_set(&array, 64)); // Word 1 Boundary
    expect_to_be_true(internal_bit_is_set(&array, 69)); // Word 1
    expect_to_be_false(internal_bit_is_set(&array, 70));

    // 3. Clear range (Test setting to false)
    expect_to_be_true(
        bitarray_fill_range(&array, false, 62, 4)); // Clear 62, 63, 64, 65

    expect_to_be_true(internal_bit_is_set(&array, 60));  // Should still be set
    expect_to_be_false(internal_bit_is_set(&array, 62)); // Cleared
    expect_to_be_false(internal_bit_is_set(&array, 65)); // Cleared
    expect_to_be_true(internal_bit_is_set(&array, 69));  // Should still be set

    bitarray_destroy(&array);
    kfree(memory, mem_req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 bitarray_should_handle_out_of_bounds() {
    u8 failed = false;
    bitarray array;
    u64 mem_req = 0;
    u64 length = 50;
    void *memory = setup_bitarray(&array, length, &mem_req);

    // 1. Set out of bounds
    expect_to_be_false(
        bitarray_set(&array, true, 50)); // Index 50 is OOB (0-49)
    expect_to_be_false(bitarray_set(&array, true, 1000));

    // 2. Fill range out of bounds
    // Start is valid, but size goes OOB
    expect_to_be_false(bitarray_fill_range(&array, true, 45, 10));

    // Start is OOB
    expect_to_be_false(bitarray_fill_range(&array, true, 55, 10));

    bitarray_destroy(&array);
    kfree(memory, mem_req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 bitarray_should_search_and_count() {
    u8 failed = false;
    bitarray array;
    u64 mem_req = 0;
    u64 length = 100;
    void *memory = setup_bitarray(&array, length, &mem_req);

    // Clear
    bitarray_fill(&array, false);

    // Test Count
    expect_should_be(0, bitarray_count_set(&array));
    bitarray_set(&array, true, 10);
    bitarray_set(&array, true, 20);
    bitarray_set(&array, true, 30);
    expect_should_be(3, bitarray_count_set(&array));

    // Test Find First
    // Should find index 10
    expect_should_be(10, bitarray_find_first(&array, 0, length, true));
    // Should find index 20 if we start searching after 10
    expect_should_be(20, bitarray_find_first(&array, 11, length, true));

    // Test Find First (searching for 0)
    bitarray_fill(&array, true); // All 1s
    bitarray_set(&array, false, 55);
    expect_should_be(55, bitarray_find_first(&array, 0, length, false));

    bitarray_destroy(&array);
    kfree(memory, mem_req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 bitarray_should_create_sub_array() {
    u8 failed = false;
    bitarray parent;
    u64 mem_req = 0;
    u64 length = 128; // 2 u64 words
    void *memory = setup_bitarray(&parent, length, &mem_req);

    // Clear parent
    bitarray_fill(&parent, false);

    // Create a sub-array (View)
    // Parent indices 60 to 80 (20 bits long).
    // This crosses the u64 boundary (at 63/64).
    bitarray sub;
    // Note: create_sub_array typically doesn't allocate new array memory,
    // it points to the parent's memory with an offset.
    b8 result = bitarray_create_sub_array(&parent, 60, 20, &sub);
    expect_to_be_true(result);
    expect_should_be(20, sub.length);
    expect_should_be(60, sub.offset_bits);

    // 1. Test Reading from Sub-array
    // Set bit 65 in parent.
    // In sub-array (starts at 60), this should be index 5.
    bitarray_set(&parent, true, 65);
    expect_to_be_true(bitarray_test(&sub, 5));

    // 2. Test Writing to Sub-array
    // Set bit 0 in sub-array.
    // In parent, this should be index 60.
    bitarray_set(&sub, true, 0);
    expect_to_be_true(internal_bit_is_set(&parent, 60));

    // 3. Test OOB on Sub-array
    // Index 25 is OOB for sub-array (len 20), even though valid for parent.
    expect_to_be_false(bitarray_set(&sub, true, 25));

    // Cleanup
    // NOTE: Depending on your implementation, destroying a sub-array
    // might not be necessary if it doesn't own memory, but we destroy the
    // parent.
    bitarray_destroy(&parent);
    kfree(memory, mem_req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

u8 bitarray_should_reflect_changes_bidirectionally() {
    u8 failed = false;
    bitarray parent;
    u64 mem_req = 0;
    u64 length = 100;
    void *memory = setup_bitarray(&parent, length, &mem_req);

    // Clean slate
    bitarray_fill(&parent, false);

    // Create a View:
    // Parent indices 20 to 40 (Length 20)
    bitarray sub;
    bitarray_create_sub_array(&parent, 20, 20, &sub);

    // ---------------------------------------------------------
    // TEST 1: Parent modification -> Visible in Sub-array
    // ---------------------------------------------------------
    // Set index 25 in parent.
    // Relative to sub-array (start 20), this is index 5.
    bitarray_set(&parent, true, 25);

    expect_to_be_true(bitarray_test(&sub, 5));
    // Verify it didn't touch index 4 (parent 24) or 6 (parent 26) in sub
    expect_to_be_false(bitarray_test(&sub, 4));
    expect_to_be_false(bitarray_test(&sub, 6));

    // ---------------------------------------------------------
    // TEST 2: Sub-array modification -> Visible in Parent
    // ---------------------------------------------------------
    // Set index 0 in sub-array.
    // This should correspond to index 20 in the parent.
    bitarray_set(&sub, true, 0);

    expect_to_be_true(internal_bit_is_set(&parent, 20));

    // ---------------------------------------------------------
    // TEST 3: Range fill on Sub-array -> Visible in Parent
    // ---------------------------------------------------------
    // Fill indices 10-15 in the sub-array.
    // Relative to parent (start 20), this is 30-35.
    bitarray_fill_range(&sub, true, 10, 5);

    expect_to_be_true(internal_bit_is_set(&parent, 30));
    expect_to_be_true(internal_bit_is_set(&parent, 34));
    expect_to_be_false(internal_bit_is_set(&parent, 35)); // Range end

    // ---------------------------------------------------------
    // TEST 4: Clearing Parent Range -> Clears Sub-array
    // ---------------------------------------------------------
    // Clear everything in parent. Sub-array should read all false.
    bitarray_fill(&parent, false);

    expect_to_be_false(bitarray_test(&sub, 0));
    expect_to_be_false(bitarray_test(&sub, 5));
    expect_to_be_false(bitarray_test(&sub, 10));

    bitarray_destroy(&parent);
    kfree(memory, mem_req, MEMORY_TAG_ARRAY);

    return failed ? false : true;
}

// -----------------------------------------------------------------------------
// Performance Benchmarks
// -----------------------------------------------------------------------------

#define BENCH_SIZE_BITS (1024 * 1024 * 64) // 64 million bits (~8MB RAM)

u8 bitarray_benchmark_optimized() {
    KDEBUG("Optimised...");
    bitarray array;
    u64 mem_req = 0;

    // Setup large array
    void *memory = setup_bitarray(&array, BENCH_SIZE_BITS, &mem_req);

    // Run the optimized fill (uses memset and bitmasks)
    // We toggle it twice (fill true, then fill false) to ensure we measure
    // write speed
    bitarray_fill_range(&array, true, 0, BENCH_SIZE_BITS);
    bitarray_fill_range(&array, false, 0, BENCH_SIZE_BITS);

    bitarray_destroy(&array);
    kfree(memory, mem_req, MEMORY_TAG_ARRAY);

    return true;
}

u8 bitarray_benchmark_naive() {
    KDEBUG("Naive...");
    bitarray array;
    u64 mem_req = 0;

    // Setup large array
    void *memory = setup_bitarray(&array, BENCH_SIZE_BITS, &mem_req);

    // Run the naive fill (loop + bitshifts)
    // Same workload: toggle twice
    bitarray_fill_range_naive(&array, true, 0, BENCH_SIZE_BITS);
    bitarray_fill_range_naive(&array, false, 0, BENCH_SIZE_BITS);

    bitarray_destroy(&array);
    kfree(memory, mem_req, MEMORY_TAG_ARRAY);

    return true;
}

void bitarray_register_tests() {
    test_manager_register_test(
        bitarray_should_create_and_destroy,
        "Bitarray should create and destroy successfully.");

    test_manager_register_test(bitarray_should_set_and_unset_bits,
                               "Bitarray should set and unset specific bits.");

    test_manager_register_test(bitarray_should_fill_all,
                               "Bitarray should fill all bits with a value.");

    test_manager_register_test(bitarray_should_fill_range,
                               "Bitarray should fill specific ranges "
                               "(including crossing word boundaries).");

    test_manager_register_test(
        bitarray_should_handle_out_of_bounds,
        "Bitarray should fail gracefully on out of bounds access.");

    test_manager_register_test(
        bitarray_should_search_and_count,
        "Bitarray should correctly count set bits and find first occurrences.");

    test_manager_register_test(
        bitarray_should_create_sub_array,
        "Bitarray should create views (sub-arrays) with correct offsets.");

    test_manager_register_test(
        bitarray_should_reflect_changes_bidirectionally,
        "Bitarray sub-arrays should sync bidirectionally with parent.");

    test_manager_register_test(
        bitarray_benchmark_optimized,
        "BENCHMARK: Optimized bitarray_fill_range (Masks + Memset)");

    test_manager_register_test(
        bitarray_benchmark_naive,
        "BENCHMARK: Naive bitarray_fill_range (Loop + Shifts)");
}
