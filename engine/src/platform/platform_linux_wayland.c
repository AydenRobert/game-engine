#include "platform/platform_linux_wayland.h"
#include "core/event.h"

#if KPLATFORM_LINUX

#include "core/input.h"
#include "core/logger.h"

#include "containers/darray.h"

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include <poll.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <xkbcommon/xkbcommon.h>

#include <linux/input-event-codes.h>

#include "xdg-shell-client-protocol.h"

#define VK_USE_PLATFORM_WAYLAND_KHR
#include "renderer/vulkan/vulkan_types.inl"
#include <vulkan/vulkan.h>

typedef struct wayland_state {
    struct wl_display *display;
    struct wl_registry *registry;

    struct wl_compositor *compositor;
    struct wl_surface *surface;

    struct xdg_wm_base *wm_base;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;

    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct wl_pointer *pointer;
    struct xkb_context *xkb_context;
    struct xkb_keymap *xkb_keymap;
    struct xkb_state *xkb_state;

    VkSurfaceKHR vulkan_surface;

    u32 configure_serial;
    b8 configured;

    i32 x;
    i32 y;
    i32 width;
    i32 height;

    b8 should_close;
} wayland_state;

// XDG Surface
void xdg_surface_config(void *data, struct xdg_surface *xdg_surface,
                        u32 serial) {
    wayland_state *state = data;

    // Must acknowledge the configure event
    xdg_surface_ack_configure(xdg_surface, serial);

    state->configure_serial = serial;
    state->configured = TRUE;
}

struct xdg_surface_listener xdg_surface_listener = {.configure =
                                                        xdg_surface_config};

// XDG Toplevel
void xdg_toplevel_config(void *data, struct xdg_toplevel *top, i32 new_width,
                         i32 new_height, struct wl_array *states) {
    wayland_state *s = data;
    if (new_width > 0 && new_height > 0) {
        s->width = new_width;
        s->height = new_height;

        event_context event_data = {};
        event_data.data.u16[0] = new_width;
        event_data.data.u16[1] = new_height;
        event_fire(EVENT_CODE_RESIZED, 0, event_data);
    }

}

void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    wayland_state *state = (wayland_state *)data;

    state->should_close = TRUE;
}

struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_config, .close = xdg_toplevel_close};

// PING PONG
void handle_ping(void *data, struct xdg_wm_base *wm_base, u32 serial) {
    xdg_wm_base_pong(wm_base, serial);
}

struct xdg_wm_base_listener wm_base_listener = {.ping = handle_ping};

// KEYMAP
static void keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
                                   u32 format, i32 fd, u32 size) {
    wayland_state *state = data;

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    char *map_shm = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    state->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    state->xkb_keymap = xkb_keymap_new_from_string(state->xkb_context, map_shm,
                                                   XKB_KEYMAP_FORMAT_TEXT_V1,
                                                   XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(map_shm, size);

    state->xkb_state = xkb_state_new(state->xkb_keymap);
}

static void keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
                                  u32 serial, struct wl_surface *surface,
                                  struct wl_array *keys) {}

static void keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
                                  u32 serial, struct wl_surface *surface) {}

static void keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
                                u32 serial, u32 time, u32 wl_key, u32 state_w) {
    wayland_state *state = data;

    b8 pressed = state_w == WL_KEYBOARD_KEY_STATE_PRESSED;

    u32 keycode = wl_key + 8; // xkbcommon expects keycodes +8
    xkb_keysym_t sym = xkb_state_key_get_one_sym(state->xkb_state, keycode);

    keys key = translate_keycode(sym);
    input_process_key(key, pressed);
}

static void keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
                                      u32 serial, u32 depressed, u32 latched,
                                      u32 locked, u32 group) {
    wayland_state *state = data;
    xkb_state_update_mask(state->xkb_state, depressed, latched, locked, 0, 0,
                          group);
}

static void keyboard_handle_repeat_info(void *data,
                                        struct wl_keyboard *keyboard, i32 rate,
                                        i32 delay) {}

struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_handle_keymap,
    .enter = keyboard_handle_enter,
    .leave = keyboard_handle_leave,
    .key = keyboard_handle_key,
    .modifiers = keyboard_handle_modifiers,
    .repeat_info = keyboard_handle_repeat_info,
};

static void pointer_handle_enter(void *data, struct wl_pointer *pointer,
                                 u32 serial, struct wl_surface *surface,
                                 wl_fixed_t sx, wl_fixed_t sy) {}

static void pointer_handle_leave(void *data, struct wl_pointer *pointer,
                                 u32 serial, struct wl_surface *surface) {}

static void pointer_handle_motion(void *data, struct wl_pointer *pointer,
                                  u32 time, wl_fixed_t sx, wl_fixed_t sy) {
    input_process_mouse_move(wl_fixed_to_int(sx), wl_fixed_to_int(sy));
}

static void pointer_handle_button(void *data, struct wl_pointer *pointer,
                                  u32 serial, u32 time, u32 button,
                                  u32 state_w) {
    b8 pressed = state_w == WL_POINTER_BUTTON_STATE_PRESSED;

    buttons mouse_button = BUTTON_MAX_BUTTONS;
    switch (button) {
    case BTN_LEFT:
        mouse_button = BUTTON_LEFT;
        break;
    case BTN_MIDDLE:
        mouse_button = BUTTON_MIDDLE;
        break;
    case BTN_RIGHT:
        mouse_button = BUTTON_RIGHT;
        break;
    }

    // Pass to input subsystem
    if (mouse_button != BUTTON_MAX_BUTTONS) {
        input_process_button(mouse_button, pressed);
    }
}

static void pointer_handle_axis(void *data, struct wl_pointer *pointer,
                                u32 time, u32 axis, wl_fixed_t value) {
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        float z_delta = wl_fixed_to_double(value);
        if (z_delta != 0) {
            // flatten to be OS independent
            z_delta = (z_delta < 0) ? -1 : 1;
        }

        // pass to input subsystem
        input_process_mouse_wheel(z_delta);
    }
}

static void pointer_handle_frame(void *data, struct wl_pointer *pointer) {}
static void pointer_handle_axis_source(void *data, struct wl_pointer *pointer,
                                       u32 source) {}
static void pointer_handle_axis_stop(void *data, struct wl_pointer *pointer,
                                     u32 time, u32 axis) {}
static void pointer_handle_axis_discrete(void *data, struct wl_pointer *pointer,
                                         u32 axis, i32 discrete) {}

struct wl_pointer_listener pointer_listener = {
    .enter = pointer_handle_enter,
    .leave = pointer_handle_leave,
    .motion = pointer_handle_motion,
    .button = pointer_handle_button,
    .axis = pointer_handle_axis,
    .frame = pointer_handle_frame,
    .axis_source = pointer_handle_axis_source,
    .axis_stop = pointer_handle_axis_stop,
    .axis_discrete = pointer_handle_axis_discrete,
};

// seat
static void seat_handle_capabilities(void *data, struct wl_seat *seat,
                                     u32 caps) {
    wayland_state *state = data;

    if (caps & WL_SEAT_CAPABILITY_POINTER) {
        state->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(state->pointer, &pointer_listener, state);
    }

    if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
        state->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(state->keyboard, &keyboard_listener, state);
    }
}

static void seat_handle_name(void *data, struct wl_seat *seat,
                             const char *name) {
    // Optional: printf("Seat name: %s\n", name);
}

struct wl_seat_listener seat_listener = {
    .capabilities = seat_handle_capabilities,
    .name = seat_handle_name,
};

// Registry
void registry_global(void *data, struct wl_registry *wl_registry, u32 name,
                     const char *interface, u32 version) {
    wayland_state *state = (wayland_state *)data;

    if (!strcmp(interface, wl_compositor_interface.name)) {
        state->compositor =
            wl_registry_bind(wl_registry, name, &wl_compositor_interface, 4);
    } else if (!strcmp(interface, xdg_wm_base_interface.name)) {
        state->wm_base =
            wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(state->wm_base, &wm_base_listener, state);
    } else if (!strcmp(interface, wl_seat_interface.name)) {
        state->seat =
            wl_registry_bind(wl_registry, name, &wl_seat_interface, 5);
        wl_seat_add_listener(state->seat, &seat_listener, state);
    }
}

void registry_global_remove(void *data, struct wl_registry *wl_registry,
                            u32 name) {}

struct wl_registry_listener registry_listener = {
    .global = registry_global, .global_remove = registry_global_remove};

b8 platform_startup_wayland(display_state *disp_state,
                            const char *application_name, i32 x, i32 y,
                            i32 width, i32 height) {
    disp_state->wayland_state = calloc(1, sizeof(wayland_state));
    wayland_state *state = (wayland_state *)disp_state->wayland_state;

    // Connect to display and registry
    state->display = wl_display_connect(NULL);
    if (!state->display) {
        KFATAL("Failed to connect to Wayland display.");
        return FALSE;
    }

    state->registry = wl_display_get_registry(state->display);

    // Add registry listener
    wl_registry_add_listener(state->registry, &registry_listener, state);
    wl_display_roundtrip(state->display);

    if (!state->compositor || !state->wm_base) {
        KFATAL("Wayland: Missing required globals (compositor or wm_base).");
        return FALSE;
    }

    // Create base wl_surface
    state->surface = wl_compositor_create_surface(state->compositor);

    // Create xdg_surface and toplevel
    state->xdg_surface =
        xdg_wm_base_get_xdg_surface(state->wm_base, state->surface);
    xdg_surface_add_listener(state->xdg_surface, &xdg_surface_listener, state);

    state->xdg_toplevel = xdg_surface_get_toplevel(state->xdg_surface);
    xdg_toplevel_add_listener(state->xdg_toplevel, &xdg_toplevel_listener,
                              state);
    xdg_toplevel_set_title(state->xdg_toplevel, application_name);

    // Set an app_id for compositors like GNOME
    xdg_toplevel_set_app_id(state->xdg_toplevel, application_name);

    // You must commit once to "announce" the surface to compositor
    wl_surface_commit(state->surface);
    wl_display_flush(state->display);

    // Wait for the initial configure event
    while (!state->configured && wl_display_dispatch(state->display) != -1) {
    }

    return TRUE;
}

void platform_shutdown_wayland(display_state *disp_state) {
    wayland_state *state = disp_state->wayland_state;

    if (!state)
        return;

    if (state->keyboard)
        wl_keyboard_destroy(state->keyboard);
    if (state->pointer)
        wl_pointer_destroy(state->pointer);
    if (state->seat)
        wl_seat_destroy(state->seat);

    if (state->xdg_toplevel)
        xdg_toplevel_destroy(state->xdg_toplevel);
    if (state->xdg_surface)
        xdg_surface_destroy(state->xdg_surface);

    if (state->surface)
        wl_surface_destroy(state->surface);

    if (state->xkb_state)
        xkb_state_unref(state->xkb_state);
    if (state->xkb_keymap)
        xkb_keymap_unref(state->xkb_keymap);
    if (state->xkb_context)
        xkb_context_unref(state->xkb_context);

    if (state->display)
        wl_display_disconnect(state->display);

    free(state);
}

b8 platform_pump_messages_wayland(display_state *disp_state) {
    wayland_state *state = disp_state->wayland_state;

    // First, handle any already-queued events.
    wl_display_dispatch_pending(state->display);

    // Try to read new events without blocking.
    int prepare = wl_display_prepare_read(state->display);
    if (prepare == 0) {
        // Flush outgoing requests (e.g., our last frame’s present).
        wl_display_flush(state->display);

        // Poll the Wayland fd with zero timeout (non-blocking).
        int fd = wl_display_get_fd(state->display);

        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        // You can set a tiny timeout (e.g., 0–1 ms). Using 0 is fully
        // non-blocking.
        int ret = poll(&pfd, 1, 0);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            // Read new events into the queue.
            if (wl_display_read_events(state->display) < 0) {
                // On error, cancel and fall back.
                wl_display_cancel_read(state->display);
            }
        } else {
            // No data to read; cancel the prepared read.
            wl_display_cancel_read(state->display);
        }
    } else {
        // Couldn’t prepare read (events pending), just flush.
        wl_display_flush(state->display);
    }

    // Dispatch everything we now have in the queue.
    wl_display_dispatch_pending(state->display);

    return !state->should_close;
}

void platform_get_required_extension_names_wayland(const char ***names_darray) {
    darray_push(*names_darray, &"VK_KHR_wayland_surface");
}

b8 platform_create_vulkan_surface_wayland(display_state *disp_state,
                                          struct vulkan_context *context) {
    wayland_state *state = (wayland_state *)disp_state->wayland_state;

    VkWaylandSurfaceCreateInfoKHR create_info = {
        VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR};
    create_info.display = state->display;
    create_info.surface = state->surface;

    VkResult result =
        vkCreateWaylandSurfaceKHR(context->instance, &create_info,
                                  context->allocator, &state->vulkan_surface);
    if (result != VK_SUCCESS) {
        KFATAL("Vulkan surface creation failed.");
        return FALSE;
    }

    context->surface = state->vulkan_surface;
    return TRUE;
}

#endif // KPLATFORM_LINUX
