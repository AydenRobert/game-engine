#include "containers/freelist.h"
#include "core/kmemory.h"
#include "core/logger.h"

typedef struct freelist_node {
    u32 offset;
    u32 size;
    struct freelist_node *next;
} freelist_node;

typedef struct internal_state {
    u32 total_size;
    u32 max_entries;
    freelist_node *free_node_head;
    freelist_node *head;
    freelist_node *nodes;
} internal_state;

freelist_node *get_node(freelist *list);
void return_node(freelist *list, freelist_node *node);

void freelist_create(u32 total_size, u64 *memory_requirement, void *memory,
                     freelist *out_list) {
    // Enough space to hold state and all nodes
    u32 max_entries = (total_size / sizeof(void *));
    *memory_requirement =
        sizeof(internal_state) + (sizeof(freelist_node) * max_entries);

    if (!memory) {
        return;
    }

    // Warn about wasteful memory usage
    u32 memory_minimum = (sizeof(internal_state) + sizeof(freelist_node)) * 8;
    if (total_size < memory_minimum) {
        KWARN("Freelists are inefficient for small amounts of memory.");
    }

    out_list->memory = memory;
    kzero_memory(out_list->memory, *memory_requirement);

    // Setup state
    internal_state *state = (internal_state *)out_list->memory;
    state->nodes = (void *)(out_list->memory + sizeof(internal_state));
    state->max_entries = max_entries;
    state->total_size = total_size;

    // Setup head node
    state->head = &state->nodes[0];
    state->head->offset = 0;
    state->head->size = total_size;
    state->head->next = 0;

    state->free_node_head = &state->nodes[1];

    // Invalidate other nodes
    for (u32 i = 1; i < state->max_entries - 1; i++) {
        state->nodes[i].offset = INVALID_ID;
        state->nodes[i].size = INVALID_ID;
        state->nodes[i].next = &state->nodes[i + 1];
    }

    state->nodes[state->max_entries - 1].offset = INVALID_ID;
    state->nodes[state->max_entries - 1].size = INVALID_ID;
    state->nodes[state->max_entries - 1].next = 0;
}

void freelist_destroy(freelist *list) {
    if (!list || !list->memory) {
        KERROR("freelist_destroy - passed invalid freelist.");
        return;
    }

    internal_state *state = (internal_state *)list->memory;
    u32 memory_size =
        sizeof(internal_state) + sizeof(freelist_node) * state->max_entries;
    kzero_memory(list->memory, memory_size);
}

b8 freelist_allocate_block(freelist *list, u32 size, u32 *out_offset) {
    if (!list || !list->memory) {
        KERROR("freelist_allocate_block - passed invalid freelist.");
        return false;
    }

    if (!out_offset) {
        KERROR("freelist_allocate_block - expects valid out_offset.");
        return false;
    }

    internal_state *state = (internal_state *)list->memory;
    freelist_node *node = state->head;
    freelist_node *previous = 0;
    while (node) {
        if (node->size == size) {
            *out_offset = node->offset;
            freelist_node *node_to_return = 0;
            if (previous) {
                previous->next = node->next;
                node_to_return = node;
            } else {
                node_to_return = state->head;
                state->head = node->next;
            }
            return_node(list, node_to_return);
            return true;
        } else if (node->size > size) {
            *out_offset = node->offset;
            node->size -= size;
            node->offset += size;
            return true;
        }

        previous = node;
        node = node->next;
    }

    u64 free_space = freelist_free_space(list);
    KWARN("freelist_allocate_block - no space was found to allocate block of "
          "size: %uB. Remaining space: %lluB.",
          size, free_space);
    return false;
}

b8 freelist_free_block(freelist *list, u32 size, u32 offset) {
    if (!list || !list->memory) {
        KERROR("freelist_free_block - passed invalid freelist.");
        return false;
    }

    internal_state *state = (internal_state *)list->memory;

    if (offset > state->total_size || (offset + size) > state->total_size) {
        KERROR("freelist_free_block - passed invalid block.");
        return false;
    }

    // Case 1: Completely full -> list is empty
    if (state->head == 0) {
        freelist_node *new_node = get_node(list);
        new_node->offset = offset;
        new_node->size = size;
        state->head = new_node;
        return true;
    }

    freelist_node *node = state->head;
    freelist_node *previous = 0;
    while (node) {
        // Case 2: There is a free node after the allocated space
        if (node->offset > offset) {
            // sanity check: see if the allocated space is at least as big as
            // the block
            if ((offset + size) > node->offset) {
                KERROR("freelist_free_block - block size invalid.");
                return false;
            }

            freelist_node *new_node;
            if ((offset + size) == node->offset) {
                // Case 2.1a: that free node is directly after the allocated
                // space
                node->offset -= size;
                node->size += size;
                new_node = node;
            } else {
                // Case 2.1b: that free node is not directly after the allocated
                // space
                // Certain scenarios a get will get called when not
                // needed. Makes the logic a lot cleaner though
                new_node = get_node(list);
                new_node->offset = offset;
                new_node->size = size;
                new_node->next = node;
            }

            // Case 2.2a: This is the new head node
            if (!previous) {
                state->head = new_node;
                return true;
            }

            // Case 2.2b: This is a middle insert
            previous->next = new_node;

            // Case 2.3: The previous chuck of memory is directly behind the
            // allocated space
            if ((previous->offset + previous->size) == new_node->offset) {
                previous->size += new_node->size;
                previous->next = new_node->next;

                return_node(list, new_node);
            }

            return true;
        } else if (node->offset == offset) {
            // Case 0: You fucked up
            KERROR("freelist_free_block - block offset already in freelist.");
            return false;
        }

        previous = node;
        node = node->next;
    }

    // Case 3: Tail insert
    // Case 0: You fucked up
    if ((previous->offset + previous->size) > offset) {
        KERROR("freelist_free_block - free block overlaps with allocation.");
        return false;
    }

    // Case 3a: Tail insert directly after a free space
    if ((previous->offset + previous->size) == offset) {
        previous->size += size;
        return true;
    }

    // Case 3b: Tail insert not directly after a free space
    freelist_node *new_node = get_node(list);
    new_node->offset = offset;
    new_node->size = size;
    previous->next = new_node;
    return true;
}

void freelist_clear(freelist *list) {
    internal_state *state = (internal_state *)list->memory;

    // Setup head node
    state->head = &state->nodes[0];
    state->head->offset = 0;
    state->head->size = state->total_size;
    state->head->next = 0;

    // Invalidate other nodes
    for (u32 i = 1; i < state->max_entries; i++) {
        state->nodes[i].offset = INVALID_ID;
        state->nodes[i].size = INVALID_ID;
    }
}

u64 freelist_free_space(freelist *list) {
    internal_state *state = (internal_state *)list->memory;
    u64 free_space = 0;

    freelist_node *node = state->head;
    while (node) {
        free_space += node->size;
        node = node->next;
    }

    return free_space;
}

// NOTE: Internal function, shouldn't get a null pointer or invalid list
freelist_node *get_node(freelist *list) {
    internal_state *state = (internal_state *)list->memory;

    freelist_node *node = state->free_node_head;
    if (!node) {
        // Full
        return 0;
    }

    // Pop
    state->free_node_head = node->next;

    node->next = 0;
    return node;
}

void return_node(freelist *list, freelist_node *node) {
    internal_state *state = (internal_state *)list->memory;

    node->offset = INVALID_ID;
    node->size = INVALID_ID;
    node->next = state->free_node_head;
    state->free_node_head = node;
}
