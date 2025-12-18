#include "containers/darray.h"

#include "core/kmemory.h"
#include "core/logger.h"
#include "core/utils.h"
#include "defines.h"

void *_darray_create(u64 length, u64 stride) {
    u64 header_size = DARRAY_FIELD_LENGTH * sizeof(u64);
    u64 array_size = length * stride;
    u64 *new_array = kallocate(header_size + array_size, MEMORY_TAG_DARRAY);
    kset_memory(new_array, 0, header_size + array_size);
    new_array[DARRAY_CAPACITY] = length;
    new_array[DARRAY_LENGTH] = 0;
    new_array[DARRAY_STRIDE] = stride;
    return (void *)(new_array + DARRAY_FIELD_LENGTH);
}

// TODO: REMOVE THIS, BAD SOLUTION
void *_darray_create_aligned(u64 length, u64 stride, u64 alignment,
                             void **base_ptr) {
    u64 header_size = DARRAY_FIELD_LENGTH * sizeof(u64);
    u64 array_size = length * stride;

    // 1. Allocate worst-case size:
    // We add 'alignment' bytes to the total to ensure we have enough room
    // to shift the memory forward to the next aligned boundary.
    u64 raw_size = header_size + array_size + alignment;
    void *raw_block = kallocate(raw_size, MEMORY_TAG_DARRAY);

    // 2. Store the original base pointer.
    // This is crucial! You need this specific address to free the memory later.
    if (base_ptr) {
        *base_ptr = raw_block;
    }

    // 3. Calculate alignment padding.
    // We want the payload (returned pointer) to be aligned, not the header.
    // Start by calculating where the payload *would* be if we didn't shift.
    u64 raw_addr = (u64)raw_block;
    u64 unaligned_payload_addr = raw_addr + header_size;

    // Calculate how much we are off by, and how much padding to add.
    u64 misalignment = unaligned_payload_addr % alignment;
    u64 padding = (alignment - misalignment) % alignment;

    u64 aligned_payload_addr = unaligned_payload_addr + padding;

    // 4. Setup the header at the new, shifted location.
    // The header sits immediately before the aligned payload.
    u64 *header = (u64 *)(aligned_payload_addr - header_size);

    // Clear the memory (header + array data)
    kset_memory(header, 0, header_size + array_size);

    header[DARRAY_CAPACITY] = length;
    header[DARRAY_LENGTH] = 0;
    header[DARRAY_STRIDE] = stride;

    // 5. Return the aligned payload address.
    return (void *)aligned_payload_addr;
}

void _darray_destroy(void *array) {
    u64 *header = (u64 *)array - DARRAY_FIELD_LENGTH;
    u64 header_size = DARRAY_FIELD_LENGTH * sizeof(u64);
    u64 total_size =
        header_size + header[DARRAY_CAPACITY] * header[DARRAY_STRIDE];
    kfree(header, total_size, MEMORY_TAG_DARRAY);
}

void _darray_destroy_aligned(void *array, u64 alignment, void *base_ptr) {
    if (!array || !base_ptr) {
        return;
    }

    // Access the header (shifted position) to get the size info
    u64 *header = (u64 *)array - DARRAY_FIELD_LENGTH;
    u64 header_size = DARRAY_FIELD_LENGTH * sizeof(u64);
    u64 total_size = header[DARRAY_CAPACITY] * header[DARRAY_STRIDE];

    // Reconstruct the total allocated size (worst case) used in create
    u64 total_allocated_size = header_size + total_size + alignment;

    // CRITICAL: Free the 'base_ptr', NOT the 'array' or 'header' pointer.
    // The allocator only knows about the raw block it gave you.
    kfree(base_ptr, total_allocated_size, MEMORY_TAG_DARRAY);
}

u64 _darray_field_get(void *array, u64 field) {
    u64 *header = (u64 *)array - DARRAY_FIELD_LENGTH;
    return header[field];
}
void _darray_field_set(void *array, u64 field, u64 value) {
    u64 *header = (u64 *)array - DARRAY_FIELD_LENGTH;
    header[field] = value;
}

void *_darray_resize(void *array) {
    u64 length = darray_length(array);
    u64 stride = darray_stride(array);
    void *temp =
        _darray_create(DARRAY_RESIZE_FACTOR * darray_capacity(array), stride);
    kcopy_memory(temp, array, length * stride);

    _darray_field_set(temp, DARRAY_LENGTH, length);
    _darray_destroy(array);
    return temp;
}

void *_darray_push(void *array, const void *value_ptr) {
    u64 length = darray_length(array);
    u64 stride = darray_stride(array);
    if (length >= darray_capacity(array)) {
        array = _darray_resize(array);
    }

    u64 addr = (u64)array;
    addr += (length * stride);
    kcopy_memory((void *)addr, value_ptr, stride);
    _darray_field_set(array, DARRAY_LENGTH, length + 1);
    return array;
}
void _darray_pop(void *array, void *dest) {
    u64 length = darray_length(array);
    u64 stride = darray_stride(array);

    u64 addr = (u64)array;
    addr += ((length - 1) * stride);
    kcopy_memory(dest, (void *)addr, stride);
    _darray_field_set(array, DARRAY_LENGTH, length - 1);
}

void *_darray_pop_at(void *array, u64 index, void *dest) {
    u64 length = darray_length(array);
    u64 stride = darray_stride(array);
    if (index >= length) {
        KERROR("Index outside the bounds of this array! Length: %i, index: %i",
               length, index);
        return array;
    }

    u64 addr = (u64)array;
    kcopy_memory(dest, (void *)(addr + (index * stride)), stride);

    if (index != length - 1) {
        kcopy_memory((void *)(addr + (index * stride)),
                     (void *)(addr + ((index + 1) * stride)),
                     stride * (length - index));
    }
    _darray_field_set(array, DARRAY_LENGTH, length - 1);
    return array;
}

void *_darray_insert_at(void *array, u64 index, void *value_ptr) {
    u64 length = darray_length(array);
    u64 stride = darray_stride(array);
    if (index >= length) {
        KERROR("Index outside the bounds of this array! Length: %i, index: %i",
               length, index);
        return array;
    }

    if (length >= darray_capacity(array)) {
        array = _darray_resize(array);
    }

    u64 addr = (u64)array;

    if (index != length - 1) {
        kcopy_memory((void *)(addr + ((index + 1) * stride)),
                     (void *)(addr + (index * stride)),
                     stride * (length - index));
    }

    kcopy_memory((void *)(addr + (index * stride)), value_ptr, stride);
    _darray_field_set(array, DARRAY_LENGTH, length + 1);
    return array;
}

void *_darray_reserve_on(void *array, u64 count_to_add) {
    u64 len = _darray_field_get(array, DARRAY_LENGTH);
    u64 capacity = _darray_field_get(array, DARRAY_CAPACITY);
    u64 stride = _darray_field_get(array, DARRAY_STRIDE);

    if (len + count_to_add <= capacity) {
        return array;
    }

    void *temp = _darray_create(next_pow2_u64(len + count_to_add), stride);
    kcopy_memory(temp, array, len * stride);
    return temp;
}
