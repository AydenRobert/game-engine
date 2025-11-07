#include "core/input.h"

#include "core/event.h"
#include "core/kmemory.h"
#include "core/logger.h"

typedef struct keyboard_state {
    b8 keys[256];
} keyboard_state;

typedef struct mouse_state {
    i16 x;
    i16 y;
    u8 buttons[BUTTON_MAX_BUTTONS];
} mouse_state;

typedef struct input_system_state {
    b8 initialized;

    keyboard_state keyboard_current;
    keyboard_state keyboard_previous;
    mouse_state mouse_current;
    mouse_state mouse_previous;
} input_system_state;

// Internal input state
static input_system_state *state_ptr;

b8 input_initialize(u64 *memory_requirement, void *state) {
    *memory_requirement = sizeof(input_system_state);
    if (state == 0) {
        return true;
    }

    state_ptr = state;
    state_ptr->initialized = true;

    kzero_memory(state_ptr, sizeof(state));
    state_ptr->initialized = true;
    KINFO("Input subsystem initialized.");
    return true;
}
void input_shutdown(void *state) {
    // TODO: Add shutdown routine when needed.
    state_ptr = 0;
}
void input_update(f64 delta_time) {
    if (!state_ptr->initialized) {
        return;
    }

    kcopy_memory(&state_ptr->keyboard_previous, &state_ptr->keyboard_current,
                 sizeof(keyboard_state));
    kcopy_memory(&state_ptr->mouse_previous, &state_ptr->mouse_current,
                 sizeof(mouse_state));
}

void input_process_key(keys key, b8 pressed) {
    if (state_ptr->keyboard_current.keys[key] != pressed) {
        state_ptr->keyboard_current.keys[key] = pressed;

        // Fire off event for processing
        event_context context;
        context.data.u16[0] = key;
        event_fire(pressed ? EVENT_CODE_KEY_PRESSED : EVENT_CODE_KEY_RELEASED,
                   0, context);
    }
}

void input_process_button(buttons button, b8 pressed) {
    if (state_ptr->mouse_current.buttons[button] != pressed) {
        state_ptr->mouse_current.buttons[button] = pressed;

        // Fire off event for processing
        event_context context;
        context.data.u16[0] = button;
        event_fire(pressed ? EVENT_CODE_BUTTON_PRESSED
                           : EVENT_CODE_BUTTON_RELEASED,
                   0, context);
    }
}

void input_process_mouse_move(i16 x, i16 y) {
    if (state_ptr->mouse_current.x != x || state_ptr->mouse_current.y != y) {
        state_ptr->mouse_current.x = x;
        state_ptr->mouse_current.y = y;

        // KDEBUG("Mouse x: %i, y: %i", x, y);

        // Fire off event for processing
        event_context context;
        context.data.u16[0] = x;
        context.data.u16[1] = y;
        event_fire(EVENT_CODE_MOUSE_MOVED, 0, context);
    }
}

void input_process_mouse_wheel(i8 z_delta) {
    event_context context;
    context.data.u8[0] = z_delta;
    event_fire(EVENT_CODE_MOUSE_WHEEL, 0, context);
}

KAPI b8 input_is_key_down(keys key) {
    if (!state_ptr->initialized) {
        return false;
    }
    return state_ptr->keyboard_current.keys[key] == true;
}

KAPI b8 input_is_key_up(keys key) {
    if (!state_ptr->initialized) {
        return true;
    }
    return state_ptr->keyboard_current.keys[key] == false;
}

KAPI b8 input_was_key_down(keys key) {
    if (!state_ptr->initialized) {
        return false;
    }
    return state_ptr->keyboard_previous.keys[key] == true;
}

KAPI b8 input_was_key_up(keys key) {
    if (!state_ptr->initialized) {
        return true;
    }
    return state_ptr->keyboard_previous.keys[key] == false;
}

// mouse input
KAPI b8 input_is_button_down(buttons button) {
    if (!state_ptr->initialized) {
        return false;
    }
    return state_ptr->mouse_current.buttons[button] == true;
}

KAPI b8 input_is_button_up(buttons button) {
    if (!state_ptr->initialized) {
        return true;
    }
    return state_ptr->mouse_current.buttons[button] == false;
}

KAPI b8 input_was_button_down(buttons button) {
    if (!state_ptr->initialized) {
        return false;
    }
    return state_ptr->mouse_previous.buttons[button] == true;
}

KAPI b8 input_was_button_up(buttons button) {
    if (!state_ptr->initialized) {
        return true;
    }
    return state_ptr->mouse_previous.buttons[button] == false;
}

KAPI void input_get_mouse_position(i32 *x, i32 *y) {
    if (!state_ptr->initialized) {
        *x = 0;
        *y = 0;
        return;
    }

    *x = state_ptr->mouse_current.x;
    *y = state_ptr->mouse_current.y;
}

KAPI void input_get_previous_mouse_position(i32 *x, i32 *y) {
    if (!state_ptr->initialized) {
        *x = 0;
        *y = 0;
        return;
    }

    *x = state_ptr->mouse_previous.x;
    *y = state_ptr->mouse_previous.y;
}
