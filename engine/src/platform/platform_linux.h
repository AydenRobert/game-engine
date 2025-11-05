#pragma once

#include "defines.h"

#include "core/input.h"

typedef struct wayland_state wayland_state;
typedef struct x11_state x11_state;

typedef struct display_state {
    wayland_state *wayland_state;
    x11_state *x11_state;
} display_state;

keys translate_keycode(u32 x_keycode);
