#include <core/event.h>

#include "containers/darray.h"
#include "core/kmemory.h"
#include "core/logger.h"

typedef struct registered_event {
    void *listener;
    PFN_on_event callback;
} registered_event;

typedef struct event_code_entry {
    registered_event *events;
} event_code_entry;

// Should be more than enough...
#define MAX_MESSAGE_CODES 16384

typedef struct event_system_state {
    b8 initialized;

    event_code_entry registered[MAX_MESSAGE_CODES];
} event_system_state;

/**
 * Event system internal state
 */
static event_system_state *state_ptr;

b8 event_initialize(u64 *memory_requirement, void *state) {
    *memory_requirement = sizeof(event_system_state);
    if (state == 0) {
        return true;
    }

    state_ptr = state;
    kzero_memory(state, sizeof(event_system_state));

    state_ptr->initialized = true;
    KINFO("Event system initialized at %p, size %llu", state,
          *memory_requirement);

    return true;
}

void event_shutdown(void *state) {
    for (u16 i = 0; i < MAX_MESSAGE_CODES; i++) {
        if (state_ptr->registered[i].events == 0) {
            break;
        }

        darray_destroy(state_ptr->registered[i].events);
        state_ptr->registered[i].events = 0;
    }

    state_ptr = 0;
}

b8 event_register(u16 code, void *listener, PFN_on_event on_event) {
    if (state_ptr->initialized == false) {
        return false;
    }

    if (state_ptr->registered[code].events == 0) {
        state_ptr->registered[code].events = darray_create(registered_event);
    }

    // check for duplicate
    u64 registered_count = darray_length(state_ptr->registered[code].events);
    for (u64 i = 0; i < registered_count; i++) {

        if (state_ptr->registered[code].events[i].listener == listener) {
            // TODO: warn
            return false;
        }
    }

    registered_event event;
    event.listener = listener;
    event.callback = on_event;
    darray_push(state_ptr->registered[code].events, event);

    return true;
}

b8 event_unregister(u16 code, void *listener, PFN_on_event on_event) {
    if (!state_ptr) {
        return false;
    }

    if (state_ptr->initialized == false) {
        return false;
    }

    if (state_ptr->registered[code].events == 0) {
        // TODO: warn
        return false;
    }

    u64 registered_count = darray_length(state_ptr->registered[code].events);
    for (u64 i = 0; i < registered_count; i++) {
        registered_event e = state_ptr->registered[code].events[i];
        if (e.listener == listener && e.callback == on_event) {
            // Found event, remove
            registered_event popped_event;
            darray_pop_at(state_ptr->registered[code].events, i, &popped_event);
            return true;
        }
    }

    return false;
}

b8 event_fire(u16 code, void *sender, event_context context) {
    if (state_ptr->initialized == false) {
        return false;
    }

    if (state_ptr->registered[code].events == 0) {
        // TODO: warn
        return false;
    }

    u64 registered_count = darray_length(state_ptr->registered[code].events);
    for (u64 i = 0; i < registered_count; i++) {
        registered_event e = state_ptr->registered[code].events[i];
        if (e.callback(code, sender, e.listener, context)) {
            // Event has been handled, do not send to other listeners
            return true;
        }
    }

    return false;
}
