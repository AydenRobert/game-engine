#pragma once

#include "defines.h"

typedef struct stack {
    u64 memory_size;
    u64 allocated;
    void *memory;
} stack;

// TODO: make value_size dynamic
b8 stack_create(u64 memory_size, void *memory, stack *out_stack);

void stack_destroy(stack *s);

b8 stack_push(stack *s, u64 value);

b8 stack_pop(stack *s, u64 *value);

b8 stack_clear(stack *s);
