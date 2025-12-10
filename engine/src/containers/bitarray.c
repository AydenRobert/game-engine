#include "containers/bitarray.h"
#include "core/kmemory.h"
#include "platform/platform.h"

#define DIV_CEIL(val, div) (((val) + (div) - 1) / (div))
#define CHUNK(index) (index / 64)
#define INDEX(index) (index % 64)

#define MASK_VAL(value) (((u64)value) & 1ULL)

#define SET(array, value, index)                                               \
    array[CHUNK(index)] = (array[CHUNK(index)] & ~(1ULL << INDEX(index))) |    \
                          (((u64)(value) & 1) << INDEX(index))

#define SET_BIT_RANGE(array, chunk, flooded_val, mask)                         \
    array[chunk] = (array[chunk] & ~mask) | (flooded_val & mask);

KINLINE u64 bit_flood(u64 bit) { return -(i64)bit; }

b8 fill_range(u64 *array, b8 value, u64 start_index, u64 end_index);

b8 bitarray_create(u64 length, u64 *memory_requirement, void *memory,
                   bitarray *out_array) {
    u64 length_bytes = DIV_CEIL(length, 8);
    *memory_requirement = length_bytes;

    if (!memory) {
        return true;
    }

    out_array->array = memory;
    out_array->length = length;

    bitarray_fill(out_array, false);

    return true;
}

void bitarray_destroy(bitarray *array) {
    array->length = 0;
    array->array = 0;
}

b8 bitarray_fill(bitarray *array, b8 value) {
    return fill_range(array->array, value, 0, array->length);
}

b8 bitarray_fill_range(bitarray *array, b8 value, u64 start_index, u64 size) {
    if (start_index + size > array->length) {
        return false;
    }
    return fill_range(array->array, value, start_index, start_index + size);
}

b8 bitarray_set(bitarray *array, b8 value, u64 index) {
    if (index >= array->length) {
        return false;
    }
    SET(array->array, value, index);
    return true;
}

b8 fill_range(u64 *array, b8 value, u64 start_index, u64 end_index) {
    u64 masked_val = MASK_VAL(value);
    u64 flooded_val = bit_flood(masked_val);

    u64 start_chunk = CHUNK(start_index);
    u64 end_chunk = CHUNK(end_index);
    u64 start_offset = INDEX(start_index);
    u64 end_offset = INDEX(end_index);

    // Case 1: in one chunk
    if (start_chunk == end_chunk) {
        u64 width = end_offset - start_offset;
        u64 mask = ((1ULL << width) - 1) << start_offset;

        SET_BIT_RANGE(array, start_chunk, flooded_val, mask);
        return true;
    }

    // Case 2: multiple chunks

    // Case 2a: handle head
    if (start_offset > 0) {
        u64 head_mask = ~0ULL << start_offset;
        SET_BIT_RANGE(array, start_chunk, flooded_val, head_mask);
        start_chunk++;
    }
    // Case 2b: handle body
    if (end_chunk > start_chunk) {
        u64 bytes = (end_chunk - start_chunk) * sizeof(u64);
        kset_memory(array + start_chunk, (value & 1) ? 0xFF : 0x00, bytes);
    }

    // Case 2c: handle tail
    if (end_offset > 0) {
        u64 tail_mask = (1ULL << end_offset) - 1;
        SET_BIT_RANGE(array, end_chunk, flooded_val, tail_mask);
    }

    return true;
}

b8 bitarray_test(bitarray *array, u64 index) {
    if (index >= array->length)
        return false;
    return (array->array[CHUNK(index)] >> INDEX(index)) & 1ULL;
}

u64 bitarray_count_set(bitarray *array) {
    u64 count = 0;
    u64 total_chunks = DIV_CEIL(array->length, 64);

    for (u64 i = 0; i < total_chunks; i++) {
        count += platform_popcount64(array->array[i]);
    }

    u64 remaining_bits = INDEX(array->length);
    u64 last_chunk = array->array[total_chunks];
    if (remaining_bits > 0) {
        u64 mask = (1ULL << remaining_bits) - 1;
        last_chunk &= mask;
    }
    count += platform_popcount64(last_chunk);
    return count;
}

u64 bitarray_find_first(bitarray *array, u64 start_index, u64 end_index,
                        b8 val) {
    if (start_index >= end_index) {
        return end_index;
    }

    u64 current_chunk = CHUNK(start_index);
    u64 offset = INDEX(start_index);

    b8 invert = (val == 0);

    u64 chunk_data = array->array[current_chunk];
    if (invert) {
        chunk_data = ~chunk_data;
    }

    chunk_data &= (~0ULL << offset);
    if (chunk_data != 0) {
        u64 found_offset = platform_ctz(chunk_data);
        u64 found_index = current_chunk * 64 + found_offset;
        return (found_index < end_index) ? found_index : end_index;
    }

    current_chunk++;
    u64 max_chunk = CHUNK(end_index - 1);
    while (current_chunk <= max_chunk) {
        chunk_data = array->array[current_chunk];
        if (invert) {
            chunk_data = ~chunk_data;
        }

        if (chunk_data != 0) {
            u64 found_offset = platform_ctz(chunk_data);
            u64 found_index = current_chunk * 64 + found_offset;
            return (found_index < end_index) ? found_index : end_index;
        }
        current_chunk++;
    }

    return end_index;
}
