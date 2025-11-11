#include "application.h"

#include "core/clock.h"
#include "core/event.h"
#include "core/input.h"
#include "core/kmemory.h"
#include "core/logger.h"
#include "game_types.h"
#include "memory/linear_allocator.h"
#include "platform/platform.h"
#include "renderer/renderer_frontend.h"

typedef struct application_state {
    game *game_inst;
    b8 is_running;
    b8 is_suspended;
    platform_state platform;
    i16 width;
    i16 height;
    clock clock;
    f64 last_time;
    linear_allocator systems_allocator;

    u64 logging_system_memory_requirement;
    void *logging_system_state;

    u64 memory_system_memory_requirement;
    void *memory_system_state;

    u64 input_system_memory_requirement;
    void *input_system_state;

    u64 event_system_memory_requirement;
    void *event_system_state;

    u64 renderer_system_memory_requirement;
    void *renderer_system_state;
} application_state;

static application_state *app_state;

// Event handlers
b8 application_on_event(u16 code, void *sender, void *listener_inst,
                        event_context context);
b8 application_on_key(u16 code, void *sender, void *listener_inst,
                      event_context context);
b8 application_on_resized(u16 code, void *sender, void *listener_inst,
                          event_context context);

KAPI b8 application_create(game *game_inst) {
    if (game_inst->application_state) {
        KERROR("application_create called more than once.");
        return false;
    }

    game_inst->application_state =
        kallocate(sizeof(application_state), MEMORY_TAG_APPLICATION);
    app_state = game_inst->application_state;
    app_state->game_inst = game_inst;

    u64 systems_allocator_total_size = 64 * 1024 * 1024; // 64 mb
    linear_allocator_create(systems_allocator_total_size, 0,
                            &app_state->systems_allocator);

    // memory
    initialize_memory(&app_state->memory_system_memory_requirement, 0);
    app_state->memory_system_state =
        linear_allocator_allocate(&app_state->systems_allocator,
                                  app_state->memory_system_memory_requirement);
    if (!initialize_memory(&app_state->memory_system_memory_requirement,
                           app_state->memory_system_state)) {
        KERROR("Failed to initialize memory system, shutting down.");
        return false;
    }

    // Initialize subsystems
    // logging
    initialize_logging(&app_state->logging_system_memory_requirement, 0);
    app_state->logging_system_state =
        linear_allocator_allocate(&app_state->systems_allocator,
                                  app_state->logging_system_memory_requirement);
    if (!initialize_logging(&app_state->logging_system_memory_requirement,
                            app_state->logging_system_state)) {
        KERROR("Failed to initialize logging system, shutting down.");
        return false;
    }
    // input
    input_initialize(&app_state->input_system_memory_requirement, 0);
    app_state->input_system_state =
        linear_allocator_allocate(&app_state->systems_allocator,
                                  app_state->input_system_memory_requirement);
    if (!input_initialize(&app_state->input_system_memory_requirement,
                          app_state->input_system_state)) {
        KERROR("Failed to initialize input system, shutting down.");
        return false;
    }

    app_state->is_suspended = false;
    // Set initial width and height
    app_state->width = game_inst->app_config.start_width;
    app_state->height = game_inst->app_config.start_height;

    // event
    event_initialize(&app_state->event_system_memory_requirement, 0);
    app_state->event_system_state =
        linear_allocator_allocate(&app_state->systems_allocator,
                                  app_state->event_system_memory_requirement);
    if (!event_initialize(&app_state->event_system_memory_requirement,
                          app_state->event_system_state)) {
        KERROR("Failed to initialize event system, shutting down.");
        return false;
    }

    event_register(EVENT_CODE_APPLICATION_QUIT, 0, application_on_event);
    event_register(EVENT_CODE_KEY_PRESSED, 0, application_on_key);
    event_register(EVENT_CODE_KEY_RELEASED, 0, application_on_key);
    event_register(EVENT_CODE_RESIZED, 0, application_on_resized);

    if (!platform_startup(&app_state->platform, game_inst->app_config.name,
                          game_inst->app_config.start_pos_x,
                          game_inst->app_config.start_pos_y,
                          game_inst->app_config.start_width,
                          game_inst->app_config.start_height)) {
        return false;
    }

    renderer_initialize(game_inst->app_config.name, &app_state->platform,
                        &app_state->renderer_system_memory_requirement, 0);
    app_state->renderer_system_state = linear_allocator_allocate(
        &app_state->systems_allocator,
        app_state->renderer_system_memory_requirement);
    if (!renderer_initialize(game_inst->app_config.name, &app_state->platform,
                             &app_state->renderer_system_memory_requirement,
                             app_state->renderer_system_state)) {
        KERROR("Failed to initialize renderer system, shutting down.");
        return false;
    }

    // Initialize the game
    if (!app_state->game_inst->initialize(app_state->game_inst)) {
        KFATAL("Game failed to initialize.");
        return false;
    }

    app_state->game_inst->on_resize(app_state->game_inst, app_state->width,
                                    app_state->height);

    return true;
}

KAPI b8 application_run() {
    app_state->is_running = true;
    clock_start(&app_state->clock);
    clock_update(&app_state->clock);
    app_state->last_time = app_state->clock.elapsed;
    f64 running_time = 0;
    u8 frame_count = 0;
    f64 target_frame_seconds = 1.0f / 60;

    KINFO(get_memory_usage_str());

    while (app_state->is_running) {
        if (!platform_pump_messages(&app_state->platform)) {
            app_state->is_running = false;
        }

        if (!app_state->is_suspended) {
            // Update clock and get delta time
            clock_update(&app_state->clock);
            f64 current_time = app_state->clock.elapsed;
            f64 delta = (current_time - app_state->last_time);
            f64 frame_start_time = platform_get_absolute_time();

            if (!app_state->game_inst->update(app_state->game_inst,
                                              (f32)delta)) {
                KFATAL("Game update failed, shutting down");
                break;
            }

            if (!app_state->game_inst->render(app_state->game_inst,
                                              (f32)delta)) {
                KFATAL("Game render failed, shutting down");
                break;
            }

            // TODO: refactor packet creation
            render_packet packet;
            packet.delta_time = delta;
            renderer_draw_frame(&packet);

            // Calculate frame time
            f64 frame_end_time = platform_get_absolute_time();
            f64 frame_elapsed_time = frame_end_time - frame_start_time;
            running_time += frame_elapsed_time;
            f64 remaining_seconds = target_frame_seconds - frame_elapsed_time;

            if (remaining_seconds > 0) {
                u64 remaining_ms = (remaining_seconds * 1000);

                // If there is time left, give it back to the OS
                b8 limit_frames = false;
                if (remaining_ms > 1 && limit_frames) {
                    platform_sleep(remaining_ms - 1);
                }
            }

            frame_count++;

            input_update(delta);

            // Update last time
            app_state->last_time = current_time;
        }
    }

    KDEBUG("Frame count: %d, running time: %f", frame_count, running_time);
    app_state->is_running = false;

    KINFO("after temp watch close");

    // Shutdown event system
    event_unregister(EVENT_CODE_APPLICATION_QUIT, 0, application_on_event);
    event_unregister(EVENT_CODE_KEY_PRESSED, 0, application_on_key);
    event_unregister(EVENT_CODE_KEY_RELEASED, 0, application_on_key);
    event_unregister(EVENT_CODE_RESIZED, 0, application_on_resized);

    event_shutdown(app_state->event_system_state);
    input_shutdown(app_state->input_system_state);
    renderer_shutdown(app_state->renderer_system_state);

    linear_allocator_destroy(&app_state->systems_allocator);

    platform_shutdown(&app_state->platform);
    shutdown_memory(app_state->memory_system_state);

    return true;
}

void application_get_framebuffer_size(u32 *width, u32 *height) {
    *width = app_state->width;
    *height = app_state->height;
}

b8 application_on_event(u16 code, void *sender, void *listener_inst,
                        event_context context) {
    switch (code) {
    case EVENT_CODE_APPLICATION_QUIT: {
        KINFO(
            "EVENT_CODE_APPLICATION_QUIT has been recieved, shutting down.\n");
        app_state->is_running = false;
        return true;
    }
    }
    return false;
}

b8 application_on_key(u16 code, void *sender, void *listener_inst,
                      event_context context) {
    if (code == EVENT_CODE_KEY_PRESSED) {
        u16 key_code = context.data.u16[0];
        if (key_code == KEY_ESCAPE) {
            event_context data = {};
            event_fire(EVENT_CODE_APPLICATION_QUIT, 0, data);
            return true;
        }
    } else if (code == EVENT_CODE_KEY_RELEASED) {
    }
    return false;
}

b8 application_on_resized(u16 code, void *sender, void *listener_inst,
                          event_context context) {
    if (code == EVENT_CODE_RESIZED) {
        u16 width = context.data.u16[0];
        u16 height = context.data.u16[1];

        if (width != app_state->width || height != app_state->height) {
            app_state->width = width;
            app_state->height = height;

            KDEBUG("Window resize: %i %i", width, height);

            // Handle minimization
            if (width == 0 || height == 0) {
                KINFO("Window is minimized, suspending application.");
                app_state->is_suspended = true;
            } else {
                if (app_state->is_suspended) {
                    KINFO("Window restored, resuming application.");
                    app_state->is_suspended = false;
                }
                app_state->game_inst->on_resize(app_state->game_inst, width,
                                                height);
                renderer_on_resize(width, height);
            }
        }
    }

    return false;
}
