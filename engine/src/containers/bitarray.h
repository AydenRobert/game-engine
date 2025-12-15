#pragma once

#include "defines.h"

// create once, no resizing, fill, fill ranges, set individual values

typedef struct bitarray bitarray;

struct bitarray {
    bitarray *parent;
    u64 length;
    u64 offset_bits;
    u64 *array;
};

b8 bitarray_create(u64 length, u64 *memory_requirement, void *memory,
                   bitarray *out_array);

void bitarray_destroy(bitarray *out_array);

b8 bitarray_fill(bitarray *array, b8 value);
b8 bitarray_fill_range(bitarray *array, b8 value, u64 start_index, u64 size);
b8 bitarray_set(bitarray *array, b8 value, u64 index);

b8 bitarray_test(bitarray *array, u64 index);
u64 bitarray_count_set(bitarray *array);

u64 bitarray_find_first(bitarray *array, u64 start_index, u64 end_index,
                        b8 val);

b8 bitarray_create_sub_array(bitarray *array, u64 start_index, u64 length,
                             bitarray *out_bitarray);
