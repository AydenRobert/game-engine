#include "containers/linkedlist.h"
#include "core/kmemory.h"
#include "core/logger.h"

typedef struct linkedlist_node linkedlist_node;

typedef struct internal_state {
    u64 max_nodes;
    u64 length;
    linkedlist_node *nodes;
    linkedlist_node *head;
    linkedlist_node *tail;
    linkedlist_node *free;
} internal_state;

struct linkedlist_node {
    void *value;
    linkedlist_node *next;
};

linkedlist_node *ll_get_node(linkedlist *list);
void *ll_return_node(linkedlist *list, linkedlist_node *node);

b8 linkedlist_create(u64 max_nodes, u64 *memory_requirement, void *memory,
                     linkedlist *out_list) {
    if (!memory_requirement) {
        KERROR("linkedlist_create - needs valid memory_requirement pointer.");
        return false;
    }

    u64 internal_state_requirement = sizeof(internal_state);
    u64 nodes_requirement = sizeof(linkedlist_node) * max_nodes;
    *memory_requirement = internal_state_requirement + nodes_requirement;

    if (!memory) {
        return true;
    }

    if (!out_list) {
        KERROR("linkedlist_create - needs valid out_list pointer.");
        return false;
    }

    out_list->state = memory;
    internal_state *state = (internal_state *)out_list->state;
    state->nodes = (void *)((u64)memory + internal_state_requirement);

    state->max_nodes = max_nodes;
    state->length = 0;
    state->head = 0;
    state->tail = 0;

    state->free = state->nodes;

    linkedlist_node *current = state->free;
    for (u64 i = 0; i < state->max_nodes - 1; i++) {
        current->value = 0;
        current->next = (void *)((u64)current + sizeof(linkedlist_node));
        current = current->next;
    }
    current->value = 0;
    current->next = 0;

    return true;
}

void *linkedlist_destroy(linkedlist *list) {
    void *memory = list->state;
    list->state = 0;
    return memory;
}

void linkedlist_reset(linkedlist *list) {
    internal_state *state = (internal_state *)list->state;

    state->head = 0;
    state->tail = 0;
    state->length = 0;

    state->free = state->nodes;

    linkedlist_node *current = state->free;
    for (u64 i = 0; i < state->max_nodes - 1; i++) {
        current->value = 0;
        current->next = (void *)((u64)current + sizeof(linkedlist_node));
        current = current->next;
    }
    current->value = 0;
    current->next = 0;
}

b8 linkedlist_push_tail(linkedlist *list, void *data) {
    internal_state *state = (internal_state *)list->state;

    if (!state->free) {
        KERROR("linkedlist_push_tail - linkedlist full.");
        return false;
    }

    linkedlist_node *new_node = ll_get_node(list);
    new_node->value = data;

    // Case 0: head == tail == 0.
    if (!state->length) {
        state->head = new_node;
        state->tail = new_node;
        state->length++;
        return true;
    }

    // Case 1: Other
    state->tail->next = new_node;
    state->tail = new_node;
    state->length++;

    return true;
}

b8 linkedlist_push_head(linkedlist *list, void *data) {
    internal_state *state = (internal_state *)list->state;

    if (!state->free) {
        KERROR("linkedlist_push_head - linkedlist full.");
        return false;
    }

    linkedlist_node *new_node = ll_get_node(list);
    new_node->value = data;

    // Case 0: head == tail == 0.
    if (!state->length) {
        state->head = new_node;
        state->tail = new_node;
        state->length++;
        return true;
    }

    // Case 1: Other
    new_node->next = state->head;
    state->head = new_node;
    state->length++;

    return true;
}

void *linkedlist_pop_tail(linkedlist *list) {
    internal_state *state = (internal_state *)list->state;

    // Case 0: head == tail == 0.
    if (!state->length) {
        return 0;
    }

    linkedlist_node *node_to_return = state->head;

    // Case 1: head == tail, therefore length == 1.
    if (state->length == 1) {
        state->head = 0;
        state->tail = 0;
    } else {
        // Case 2: length > 1.
        linkedlist_node *previous = 0;

        for (u64 i = 1; i < state->length; i++) {
            previous = node_to_return;
            node_to_return = node_to_return->next;
        }

        if (previous) {
            state->tail = previous;
            previous->next = 0;
        }
    }

    return ll_return_node(list, node_to_return);
}

void *linkedlist_pop_head(linkedlist *list) {
    internal_state *state = (internal_state *)list->state;

    // Case 0: head == tail == 0.
    if (!state->length) {
        return 0;
    }

    linkedlist_node *node_to_return = state->head;

    // Case 1: head == tail, therefore length == 1.
    if (state->length == 1) {
        state->head = 0;
        state->tail = 0;
    } else {
        state->head = node_to_return->next;
    }

    return ll_return_node(list, node_to_return);
}

b8 linkedlist_insert_at(linkedlist *list, u64 index, void *data) {
    internal_state *state = (internal_state *)list->state;

    if (index > state->length) {
        KERROR("linkedlist_insert_at - index (%d) is greater than length (%d).",
               index, state->length);
        return false;
    }

    if (!state->free) {
        KERROR("linkedlist_insert_at - linkedlist full.");
        return false;
    }

    linkedlist_node *new_node = ll_get_node(list);
    new_node->value = data;

    // Case 0: head == tail == 0.
    if (!state->length) {
        state->head = new_node;
        state->tail = new_node;
        state->length++;
        return true;
    }

    linkedlist_node *current = state->head;
    linkedlist_node *previous = 0;
    for (u64 i = 0; i < index; i++) {
        previous = current;
        current = current->next;
    }

    // Case 1: We are at the head.
    if (!previous) {
        new_node->next = state->head;
        state->head = new_node;
    } else {
        // Case 2: We are in the middle or the tail.
        previous->next = new_node;
        new_node->next = current;

        if (!current) {
            state->tail = new_node;
        }
    }

    state->length++;

    return true;
}

void *linkedlist_remove_at(linkedlist *list, u64 index) {
    internal_state *state = (internal_state *)list->state;

    if (index >= state->length) {
        KERROR("linkedlist_insert_at - index (%d) is greater than or equal to "
               "length (%d).",
               index, state->length);
        return 0;
    }

    // Case 0: head == tail == 0.
    if (!state->length) {
        return 0;
    }

    linkedlist_node *current = state->head;
    linkedlist_node *previous = 0;
    for (u64 i = 0; i < index; i++) {
        previous = current;
        current = current->next;
    }

    if (!previous) {
        // Case 1: we are at the head.
        state->head = current->next;
    } else if (!current->next) {
        // Case 2: we are at the tail.
        previous->next = 0;
        state->tail = previous;
    } else {
        // Case 3: we are in the middle.
        previous->next = current->next;
    }

    return ll_return_node(list, current);
}

void *linkedlist_get_at(linkedlist *list, u64 index) {
    internal_state *state = (internal_state *)list->state;

    if (index >= state->length) {
        KERROR("linkedlist_insert_at - index (%d) is greater than or equal to "
               "length (%d).",
               index, state->length);
        return 0;
    }

    linkedlist_node *current = state->head;
    for (u64 i = 0; i < index; i++) {
        current = current->next;
    }

    return current->value;
}

u64 linkedlist_length(linkedlist *list) {
    return ((internal_state *)list->state)->length;
}

b8 linkedlist_iterator_begin(linkedlist *list, linkedlist_iterator *iterator) {
    internal_state *state = (internal_state *)list->state;
    iterator->current_node = state->head;
    iterator->list = list;
    iterator->index = 0;
    return state->length > 0 ? true : false;
}

b8 linkedlist_iterator_begin_at(linkedlist *list, u64 index,
                                linkedlist_iterator *iterator) {
    internal_state *state = (internal_state *)list->state;

    if (index >= state->length) {
        return false;
    }

    iterator->current_node = state->head;
    for (u64 i = 0; i < index; i++) {
        iterator->current_node =
            ((linkedlist_node *)iterator->current_node)->next;
    }
    iterator->list = list;
    iterator->index = index;
    return state->length > 0 ? true : false;
}

b8 linkedlist_iterator_next(linkedlist_iterator *iterator) {
    if (!iterator->current_node) {
        return false;
    }

    iterator->current_node = ((linkedlist_node *)iterator->current_node)->next;
    iterator->index++;
    return iterator->current_node != 0;
}

void *linkedlist_iterator_get(linkedlist_iterator *iterator) {
    return ((linkedlist_node *)iterator->current_node)->value;
}

void *linkedlist_iterator_remove(linkedlist_iterator *iterator) {
    return linkedlist_remove_at(iterator->list, iterator->index);
}

linkedlist_node *ll_get_node(linkedlist *list) {
    internal_state *state = (internal_state *)list->state;

    if (!state->free) {
        return 0;
    }

    linkedlist_node *node_to_return = state->free;
    state->free = node_to_return->next;

    node_to_return->next = 0;

    return node_to_return;
}

void *ll_return_node(linkedlist *list, linkedlist_node *node) {
    internal_state *state = (internal_state *)list->state;

    void *out_value = node->value;
    node->value = 0;
    node->next = state->free;
    state->free = node;

    state->length--;

    return out_value;
}
