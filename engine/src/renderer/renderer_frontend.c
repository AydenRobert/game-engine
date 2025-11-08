#include "renderer/renderer_frontend.h"

#include "math/kmath.h"
#include "math/math_types.h"
#include "renderer/renderer_backend.h"

#include "core/kmemory.h"
#include "core/logger.h"
#include "renderer/renderer_types.inl"

typedef struct renderer_system_state {
    b8 initialized;

    renderer_backend *backend;
} renderer_system_state;

static renderer_system_state *state_ptr;

b8 renderer_initialize(const char *application_name,
                       struct platform_state *plat_state,
                       u64 *memory_requirement, void *state) {
    *memory_requirement =
        sizeof(renderer_system_state) + sizeof(renderer_backend);
    if (state == 0) {
        return true;
    }

    state_ptr = state;
    state_ptr->initialized = true;

    u64 renderer_system_state_ptr = (u64)state_ptr;
    state_ptr->backend = (renderer_backend *)(renderer_system_state_ptr +
                                              sizeof(renderer_backend));
    state_ptr->backend->frame_number = 0;

    // TODO: make this configurable
    renderer_backend_create(RENDERER_BACKEND_TYPES_VULKAN, plat_state,
                            state_ptr->backend);

    if (!state_ptr->backend->initialize(state_ptr->backend, application_name,
                                        plat_state)) {
        KFATAL("Renderer backend failed to initialize. Shutting down.");
        return false;
    }

    return true;
}

void renderer_shutdown(void *state) {
    state_ptr->backend->shutdown(state_ptr->backend);
    state_ptr->backend = 0;
    state_ptr = 0;
}

b8 renderer_begin_frame(f32 delta_time) {
    return state_ptr->backend->begin_frame(state_ptr->backend, delta_time);
}

b8 renderer_end_frame(f32 delta_time) {
    b8 result = state_ptr->backend->end_frame(state_ptr->backend, delta_time);
    state_ptr->backend->frame_number++;
    return result;
}

void renderer_on_resize(u16 width, u16 height) {
    if (state_ptr) {
        state_ptr->backend->resized(state_ptr->backend, width, height);
    } else {
        KWARN("renderer_state_ptr->backend does not exist to accept resize.");
    }
}

b8 renderer_draw_frame(render_packet *packet) {
    // If begin frame returns successfully, then mid frame operation can
    // continue.
    if (renderer_begin_frame(packet->delta_time)) {

        mat4 projection =
            mat4_perspective(deg_to_rad(45), 9 / 16.0f, 0.1f, 1000.0f);
        static f32 z = -1.0f;
        z -= 0.001f;
        mat4 view = mat4_translation((vec3){{0, 0.5f * z, 5 * z}});
        state_ptr->backend->update_global_state(projection, view, vec3_zero(),
                                                vec4_one(), 0);

        // If renderer_end_frame fails, likely unrecoverable
        b8 result = renderer_end_frame(packet->delta_time);
        if (!result) {
            KERROR("renderer_end_frame failed. Application shutting down...");
            return false;
        }
    }

    return true;
}
