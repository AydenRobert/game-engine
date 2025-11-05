#include "platform_linux_x11.h"

#if KPLATFORM_LINUX

#include <defines.h>

#include "core/event.h"
#include "core/input.h"
#include "core/logger.h"

#include "containers/darray.h"

#include <stdlib.h>
#include <string.h>

#include <X11/XKBlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <sys/time.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#define VK_USE_PLATFORM_XCB_KHR
#include "renderer/vulkan/vulkan_types.inl"
#include <vulkan/vulkan.h>

typedef struct x11_state {
    Display *display;
    xcb_connection_t *connection;
    xcb_window_t window;
    xcb_screen_t *screen;
    xcb_atom_t wm_protocols;
    xcb_atom_t wm_delete_win;
    VkSurfaceKHR surface;
} x11_state;

b8 platform_startup_x11(display_state *disp_state, const char *application_name,
                        i32 x, i32 y, i32 width, i32 height) {
    disp_state->x11_state = malloc(sizeof(x11_state));
    x11_state *state = (x11_state *)disp_state->x11_state;

    state->display = XOpenDisplay(NULL);

    XAutoRepeatOff(state->display);

    state->connection = XGetXCBConnection(state->display);

    if (xcb_connection_has_error(state->connection)) {
        KFATAL("Failed to connect to X server via XCB");
        return FALSE;
    }

    // Get data from X server
    const struct xcb_setup_t *setup = xcb_get_setup(state->connection);

    // Loop through screen and assign
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    int screen_p = 0;
    for (i32 s = screen_p; s > 0; s--) {
        xcb_screen_next(&it);
    }
    state->screen = it.data;

    // Allocating an XID for the window
    state->window = xcb_generate_id(state->connection);

    // Register event types
    u32 event_mask = XCB_CW_BACKING_PIXEL | XCB_CW_EVENT_MASK;

    // Listen for keyboard and mouse
    u32 event_values = XCB_EVENT_MASK_BUTTON_PRESS |
                       XCB_EVENT_MASK_BUTTON_RELEASE |
                       XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
                       XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_POINTER_MOTION |
                       XCB_EVENT_MASK_STRUCTURE_NOTIFY;

    // Values to be sent over to XCB
    u32 value_list[] = {state->screen->black_pixel, event_values};

    // Create the window
    // xcb_void_cookie_t cookie =
    xcb_create_window(state->connection, XCB_COPY_FROM_PARENT, state->window,
                      state->screen->root, x, y, width, height, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, state->screen->root_visual,
                      event_mask, value_list);

    // Change the title
    xcb_change_property(state->connection, XCB_PROP_MODE_REPLACE, state->window,
                        XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                        strlen(application_name), application_name);

    // Tell the server to notify the client when attempting to delete the window
    xcb_intern_atom_cookie_t wm_delete_cookie = xcb_intern_atom(
        state->connection, 0, strlen("WM_DELETE_WINDOW"), "WM_DELETE_WINDOW");
    xcb_intern_atom_cookie_t wm_protocols_cookie = xcb_intern_atom(
        state->connection, 0, strlen("WM_PROTOCOLS"), "WM_PROTOCOLS");
    xcb_intern_atom_reply_t *wm_delete_reply =
        xcb_intern_atom_reply(state->connection, wm_delete_cookie, NULL);
    xcb_intern_atom_reply_t *wm_protocols_reply =
        xcb_intern_atom_reply(state->connection, wm_protocols_cookie, NULL);

    state->wm_delete_win = wm_delete_reply->atom;
    state->wm_protocols = wm_protocols_reply->atom;

    xcb_change_property(state->connection, XCB_PROP_MODE_REPLACE, state->window,
                        wm_protocols_reply->atom, 4, 32, 1,
                        &wm_delete_reply->atom);

    // Map the window to the screen
    xcb_map_window(state->connection, state->window);

    // Flush the stream
    i32 stream_result = xcb_flush(state->connection);
    if (stream_result <= 0) {
        KFATAL("An error occurred while flushing the stream: %d",
               stream_result);
        return FALSE;
    }

    return TRUE;
}

void platform_shutdown_x11(display_state *disp_state) {
    x11_state *state = (x11_state *)disp_state->x11_state;

    XAutoRepeatOn(state->display);

    xcb_destroy_window(state->connection, state->window);
}

b8 platform_pump_messages_x11(display_state *disp_state) {
    x11_state *state = (x11_state *)disp_state->x11_state;

    xcb_generic_event_t *event;
    xcb_client_message_event_t *cm;

    b8 quit_flagged = FALSE;

    // Poll for events until null is returned
    while ((event = xcb_poll_for_event(state->connection))) {
        if (event == 0) {
            break;
        }

        switch (event->response_type & ~0x80) {
        case XCB_KEY_PRESS:
        case XCB_KEY_RELEASE: {
            xcb_key_press_event_t *kb_event = (xcb_key_press_event_t *)event;
            b8 pressed = event->response_type == XCB_KEY_PRESS;
            xcb_keycode_t code = kb_event->detail;
            KeySym key_sym = XkbKeycodeToKeysym(state->display, (KeyCode)code,
                                                0, code & ShiftMask ? 1 : 0);
            keys key = translate_keycode(key_sym);

            // Pass to input subsystem
            input_process_key(key, pressed);
        } break;
        case XCB_BUTTON_PRESS:
        case XCB_BUTTON_RELEASE: {
            xcb_button_press_event_t *mouse_event =
                (xcb_button_press_event_t *)event;
            b8 pressed = event->response_type == XCB_BUTTON_PRESS;
            buttons mouse_button = BUTTON_MAX_BUTTONS;
            switch (mouse_event->detail) {
            case XCB_BUTTON_INDEX_1:
                mouse_button = BUTTON_LEFT;
                break;
            case XCB_BUTTON_INDEX_2:
                mouse_button = BUTTON_MIDDLE;
                break;
            case XCB_BUTTON_INDEX_3:
                mouse_button = BUTTON_RIGHT;
                break;
            case XCB_BUTTON_INDEX_4: // Scroll Up
                if (pressed) {
                    input_process_mouse_wheel(-1);
                }
                break;
            case XCB_BUTTON_INDEX_5: // Scroll Down
                if (pressed) {
                    input_process_mouse_wheel(1);
                }
                break;
            }

            // Pass to input subsystem
            if (mouse_button != BUTTON_MAX_BUTTONS) {
                input_process_button(mouse_button, pressed);
            }
        } break;
        case XCB_MOTION_NOTIFY: {
            xcb_motion_notify_event_t *move_event =
                (xcb_motion_notify_event_t *)event;

            // Pass to input subsystem
            input_process_mouse_move(move_event->event_x, move_event->event_y);
        } break;
        case XCB_CONFIGURE_NOTIFY: {
                xcb_configure_notify_event_t *configure_event = (xcb_configure_notify_event_t *)event;
                event_context context = {};
                context.data.u16[0] = configure_event->width;
                context.data.u16[1] = configure_event->height;
                event_fire(EVENT_CODE_RESIZED, 0, context);
        } break;
        case XCB_CLIENT_MESSAGE: {
            cm = (xcb_client_message_event_t *)event;

            // Window close
            if (cm->data.data32[0] == state->wm_delete_win) {
                quit_flagged = TRUE;
            }
        } break;
        default:
            break;
        }
        free(event);
    }
    return !quit_flagged;
}

void platform_get_required_extension_names_x11(const char ***names_darray) {
    darray_push(*names_darray, &"VK_KHR_xcb_surface");
}

b8 platform_create_vulkan_surface_x11(display_state *disp_state,
                                      struct vulkan_context *context) {
    x11_state *state = (x11_state *)disp_state->x11_state;

    VkXcbSurfaceCreateInfoKHR create_info = {
        VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR};
    create_info.connection = state->connection;
    create_info.window = state->window;

    VkResult result = vkCreateXcbSurfaceKHR(
        context->instance, &create_info, context->allocator, &state->surface);
    if (result != VK_SUCCESS) {
        KFATAL("Vulkan surface creation failed.");
        return FALSE;
    }

    context->surface = state->surface;
    return TRUE;
}

#endif // KPLATFORM_LINUX
