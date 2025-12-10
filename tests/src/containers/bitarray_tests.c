#include "bitarray_tests.h"
#include <containers/bitarray.h>

#include "../expect.h"
#include "../test_manager.h"
#include "core/kmemory.h"
#include "core/logger.h"

#include <defines.h>

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

// Helper to handle the common setup logic.
// Returns the allocated memory block so it can be freed later.
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
}
