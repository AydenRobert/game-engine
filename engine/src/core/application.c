#include "application.h"

#include "core/clock.h"
#include "core/event.h"
#include "core/input.h"
#include "core/kmemory.h"
#include "core/kstring.h"
#include "core/logger.h"
#include "defines.h"
#include "game_types.h"
#include "memory/linear_allocator.h"
#include "platform/platform.h"
#include "renderer/renderer_frontend.h"

// systems
#include "resources/resource_types.h"
#include "systems/geometry_system.h"
#include "systems/material_system.h"
#include "systems/resource_system.h"
#include "systems/texture_system.h"

// TODO: temp
#include "math/kmath.h"

typedef struct application_state {
    game *game_inst;
    b8 is_running;
    b8 is_suspended;
    platform_state platform;
    i16 width;
    i16 height;
    clock clock;
    f64 last_time;

    u64 systems_allocator_memory_requirement;
    void *systems_allocator_memory;
    linear_allocator systems_allocator;

    u64 logging_system_memory_requirement;
    void *logging_system_state;

    u64 input_system_memory_requirement;
    void *input_system_state;

    u64 event_system_memory_requirement;
    void *event_system_state;

    u64 resource_system_memory_requirement;
    void *resource_system_state;

    u64 renderer_system_memory_requirement;
    void *renderer_system_state;

    u64 texture_system_memory_requirement;
    void *texture_system_state;

    u64 material_system_memory_requirement;
    void *material_system_state;

    u64 geometry_system_memory_requirement;
    void *geometry_system_state;

    // TODO: temp
    geometry *test_world_geometry;
    geometry *test_ui_geometry;
} application_state;

static application_state *app_state;

// Event handlers
b8 application_on_event(u16 code, void *sender, void *listener_inst,
                        event_context context);
b8 application_on_key(u16 code, void *sender, void *listener_inst,
                      event_context context);
b8 application_on_resized(u16 code, void *sender, void *listener_inst,
                          event_context context);

// TODO: temp
b8 event_on_debug_event(u16 code, void *sender, void *listener_inst,
                        event_context data) {
    const char *names[4] = {"cobblestone", "paving", "paving2", "grass"};
    static i8 choice = 2;

    // Save old name
    const char *old_name = names[choice];

    choice++;
    choice %= 4;

    // Acquire the new texture
    app_state->test_world_geometry->material->diffuse_map.texture =
        texture_system_acquire(names[choice], true);
    if (!app_state->test_world_geometry->material->diffuse_map.texture) {
        KWARN("event_on_debug - no texture, using default...");
        app_state->test_world_geometry->material->diffuse_map.texture =
            texture_system_get_default_texture();
    }

    // Release the old texture
    texture_system_release(old_name);

    return true;
}

KAPI b8 application_create(game *game_inst) {
    if (game_inst->application_state) {
        KERROR("application_create called more than once.");
        return false;
    }

    // memory
    memory_system_configuration memory_system_config = {};
    memory_system_config.total_alloc_count = GIBIBYTES(1);
    if (!memory_system_initialize(memory_system_config)) {
        KERROR("Failed to initialize memory system, shutting down.");
        return false;
    }

    game_inst->application_state =
        kallocate(sizeof(application_state), MEMORY_TAG_APPLICATION);
    app_state = game_inst->application_state;
    app_state->game_inst = game_inst;

    u64 systems_allocator_total_size = 64 * 1024 * 1024; // 64 mb
    linear_allocator_create(systems_allocator_total_size,
                            &app_state->systems_allocator_memory_requirement, 0,
                            &app_state->systems_allocator);
    app_state->systems_allocator_memory =
        kallocate(app_state->systems_allocator_memory_requirement,
                  MEMORY_TAG_LINEAR_ALLOCATOR);
    linear_allocator_create(systems_allocator_total_size,
                            &app_state->systems_allocator_memory_requirement,
                            app_state->systems_allocator_memory,
                            &app_state->systems_allocator);

    // Initialize subsystems
    // logging
    initialize_logging(&app_state->logging_system_memory_requirement, 0);
    app_state->logging_system_state = linear_allocator_allocate(
        &app_state->systems_allocator,
        app_state->logging_system_memory_requirement, 64);
    if (!initialize_logging(&app_state->logging_system_memory_requirement,
                            app_state->logging_system_state)) {
        KERROR("Failed to initialize logging system, shutting down.");
        return false;
    }
    // input
    input_initialize(&app_state->input_system_memory_requirement, 0);
    app_state->input_system_state = linear_allocator_allocate(
        &app_state->systems_allocator,
        app_state->input_system_memory_requirement, 64);
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
    app_state->event_system_state = linear_allocator_allocate(
        &app_state->systems_allocator,
        app_state->event_system_memory_requirement, 64);
    if (!event_initialize(&app_state->event_system_memory_requirement,
                          app_state->event_system_state)) {
        KERROR("Failed to initialize event system, shutting down.");
        return false;
    }

    event_register(EVENT_CODE_APPLICATION_QUIT, 0, application_on_event);
    event_register(EVENT_CODE_KEY_PRESSED, 0, application_on_key);
    event_register(EVENT_CODE_KEY_RELEASED, 0, application_on_key);
    event_register(EVENT_CODE_RESIZED, 0, application_on_resized);
    // TODO: temp registration
    event_register(EVENT_CODE_DEBUG0, 0, event_on_debug_event);

    if (!platform_startup(&app_state->platform, game_inst->app_config.name,
                          game_inst->app_config.start_pos_x,
                          game_inst->app_config.start_pos_y,
                          game_inst->app_config.start_width,
                          game_inst->app_config.start_height)) {
        return false;
    }

    // Initialize resource system
    resource_system_config resource_system_config;
    resource_system_config.asset_base_path = "./assets";
    resource_system_config.max_loader_count = 32;
    resource_system_initialize(&app_state->resource_system_memory_requirement,
                               0, resource_system_config);
    app_state->resource_system_state = linear_allocator_allocate(
        &app_state->systems_allocator,
        app_state->resource_system_memory_requirement, 64);
    if (!resource_system_initialize(
            &app_state->resource_system_memory_requirement,
            app_state->resource_system_state, resource_system_config)) {
        KFATAL("Failed to initialize resource system, shutting down.");
        return false;
    }

    // Initialize renderer
    renderer_initialize(game_inst->app_config.name, &app_state->platform,
                        &app_state->renderer_system_memory_requirement, 0);
    app_state->renderer_system_state = linear_allocator_allocate(
        &app_state->systems_allocator,
        app_state->renderer_system_memory_requirement, 64);
    if (!renderer_initialize(game_inst->app_config.name, &app_state->platform,
                             &app_state->renderer_system_memory_requirement,
                             app_state->renderer_system_state)) {
        KFATAL("Failed to initialize renderer system, shutting down.");
        return false;
    }

    // Initialize texture system
    texture_system_config texture_system_config;
    texture_system_config.max_texture_count = 65536;
    texture_system_initialize(&app_state->texture_system_memory_requirement, 0,
                              texture_system_config);
    app_state->texture_system_state = linear_allocator_allocate(
        &app_state->systems_allocator,
        app_state->texture_system_memory_requirement, 64);
    if (!texture_system_initialize(
            &app_state->texture_system_memory_requirement,
            app_state->texture_system_state, texture_system_config)) {
        KFATAL("Failed to initialize texture system, shutting down.");
        return false;
    }

    // Initialize material system
    material_system_config material_system_config;
    material_system_config.max_material_count = 4096;
    material_system_initialize(&app_state->material_system_memory_requirement,
                               0, material_system_config);
    app_state->material_system_state = linear_allocator_allocate(
        &app_state->systems_allocator,
        app_state->material_system_memory_requirement, 64);
    if (!material_system_initialize(
            &app_state->material_system_memory_requirement,
            app_state->material_system_state, material_system_config)) {
        KFATAL("Failed to initialize material system, shutting down.");
        return false;
    }

    // Initialize geometry system
    geometry_system_config geometry_system_config;
    geometry_system_config.max_geometry_count = 4096;
    geometry_system_initialize(&app_state->geometry_system_memory_requirement,
                               0, geometry_system_config);
    app_state->geometry_system_state = linear_allocator_allocate(
        &app_state->systems_allocator,
        app_state->geometry_system_memory_requirement, 64);
    if (!geometry_system_initialize(
            &app_state->geometry_system_memory_requirement,
            app_state->geometry_system_state, geometry_system_config)) {
        KFATAL("Failed to initialize geometry system, shutting down.");
        return false;
    }

    // TODO: temp

    // Load plane geometry
    geometry_config plane_config = geometry_system_generate_plane_config(
        10.0f, 5.0f, 5, 5, 5.0f, 2.0f, "test_plane", "test_material");
    app_state->test_world_geometry =
        geometry_system_acquire_from_config(plane_config, true);

    kfree(plane_config.vertices, sizeof(vertex_3d) * plane_config.vertex_count,
          MEMORY_TAG_ARRAY);
    kfree(plane_config.indices, sizeof(u32) * plane_config.index_count,
          MEMORY_TAG_ARRAY);

    geometry_config ui_config;
    ui_config.vertex_size = sizeof(vertex_2d);
    ui_config.vertex_count = 4;
    ui_config.index_size = sizeof(u32);
    ui_config.index_count = 6;
    string_ncopy(ui_config.material_name, "test_ui_material",
                 MATERIAL_NAME_MAX_LENGTH);
    string_ncopy(ui_config.name, "test_ui_geometry", GEOMETRY_NAME_MAX_LENGTH);

    const f32 f = 512.0f;
    vertex_2d uiverts[4];

    uiverts[0].position.x = 0.0f;
    uiverts[0].position.y = 0.0f;
    uiverts[0].texcoord.x = 0.0f;
    uiverts[0].texcoord.y = 0.0f;

    uiverts[1].position.x = f;
    uiverts[1].position.y = f;
    uiverts[1].texcoord.x = 1.0f;
    uiverts[1].texcoord.y = 1.0f;

    uiverts[2].position.x = 0.0f;
    uiverts[2].position.y = f;
    uiverts[2].texcoord.x = 0.0f;
    uiverts[2].texcoord.y = 1.0f;

    uiverts[3].position.x = f;
    uiverts[3].position.y = 0.0f;
    uiverts[3].texcoord.x = 1.0f;
    uiverts[3].texcoord.y = 0.0f;

    ui_config.vertices = uiverts;

    u32 uiindices[6] = {2, 1, 0, 3, 0, 1};
    ui_config.indices = uiindices;

    app_state->test_ui_geometry =
        geometry_system_acquire_from_config(ui_config, true);

    // Load default geometry
    // app_state->test_geometry = geometry_system_get_default_geometry();

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

            // TODO: temp
            geometry_render_data test_render;
            test_render.geometry = app_state->test_world_geometry;
            test_render.model = mat4_identity();

            packet.geometry_count = 1;
            packet.geometries = &test_render;

            geometry_render_data test_ui_render;
            test_ui_render.geometry = app_state->test_ui_geometry;
            test_ui_render.model = mat4_translation((vec3){{0, 0, 0}});
            packet.ui_geometry_count = 1;
            packet.ui_geometries = &test_ui_render;

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
    // TODO: temp
    event_register(EVENT_CODE_DEBUG0, 0, event_on_debug_event);

    input_shutdown(app_state->input_system_state);
    geometry_system_shutdown(app_state->geometry_system_state);
    material_system_shutdown(app_state->material_system_state);
    texture_system_shutdown(app_state->texture_system_state);
    renderer_shutdown(app_state->renderer_system_state);
    resource_system_shutdown(app_state->resource_system_state);
    event_shutdown(app_state->event_system_state);

    // Destroy and free linear allocator
    linear_allocator_destroy(&app_state->systems_allocator);
    kfree(app_state->systems_allocator_memory,
          app_state->systems_allocator_memory_requirement,
          MEMORY_TAG_LINEAR_ALLOCATOR);

    platform_shutdown(&app_state->platform);
    memory_system_shutdown();

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
