#include "containers/stack.h"

b8 stack_create(u64 memory_size, void *memory, stack *out_stack) {
    if (!memory_size || !memory || !out_stack) {
        return false;
    }

    out_stack->memory_size = memory_size;
    out_stack->memory = memory;
    out_stack->allocated = 0;
    return true;
}

void stack_destroy(stack *s) {
    s->memory_size = 0;
    s->memory = 0;
}

b8 stack_push(stack *s, u64 value) {
    if (!s || !s->memory || s->allocated + sizeof(u64) > s->memory_size) {
        return false;
    }

    *(u64 *)((u64)s->memory + s->allocated) = value;
    s->allocated += sizeof(u64);
    return true;
}

b8 stack_pop(stack *s, u64 *value) {
    if (!s || !s->memory || s->allocated + sizeof(u64) > s->memory_size) {
        return false;
    }

    s->allocated -= sizeof(u64);
    *value = *(u64 *)((u64)s->memory + s->allocated);
    return true;
}

b8 stack_clear(stack *s) {
    if (!s || !s->memory) {
        return false;
    }

    s->allocated = 0;
    return true;
}
