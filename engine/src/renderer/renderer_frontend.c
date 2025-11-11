#include "renderer/renderer_frontend.h"

#include "math/kmath.h"
#include "renderer/renderer_backend.h"

#include "core/kmemory.h"
#include "core/logger.h"
#include "renderer/renderer_types.inl"
#include "renderer/vulkan/vulkan_backend.h"

#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGN_UP_PTR(p, a) ((void *)ALIGN_UP((uintptr_t)(p), (uintptr_t)(a)))

typedef struct renderer_system_state {
    b8 initialized;

    renderer_backend *backend;

    mat4 projection;
    mat4 view;

    f32 near_clip;
    f32 far_clip;
} renderer_system_state;

static renderer_system_state *state_ptr;

b8 renderer_initialize(const char *application_name,
                       struct platform_state *plat_state,
                       u64 *memory_requirement, void *state) {
    /* Compute memory requirement with worst-case front padding and proper
       alignment between the system state and backend. */
    const size_t a_state = alignof(renderer_system_state);
    const size_t a_back = alignof(renderer_backend);

    const size_t after_state_aligned =
        ALIGN_UP(sizeof(renderer_system_state), a_back);

    /* Worst-case front padding to align the base to renderer_system_state. */
    const size_t worst_front_pad = a_state - 1;

    const size_t total_bytes =
        worst_front_pad + after_state_aligned + sizeof(renderer_backend);

    *memory_requirement = (u64)total_bytes;

    if (state == 0) {
        return true;
    }

    u8 *base = (u8 *)state;
    renderer_system_state *rss =
        (renderer_system_state *)ALIGN_UP_PTR(base, a_state);

    u8 *after_rss = (u8 *)rss + sizeof(*rss);
    renderer_backend *rb = (renderer_backend *)ALIGN_UP_PTR(after_rss, a_back);

    state_ptr = rss;
    state_ptr->backend = rb;

    state_ptr->initialized = true;

    renderer_backend_create(RENDERER_BACKEND_TYPES_VULKAN, plat_state,
                            state_ptr->backend);

    state_ptr->backend->frame_number = 0;

    if (!state_ptr->backend->initialize(state_ptr->backend, application_name,
                                        plat_state)) {
        KFATAL("Renderer backend failed to initialize. Shutting down.");
        return false;
    }

    state_ptr->near_clip = 0.1f;
    state_ptr->far_clip = 1000.0f;
    state_ptr->projection =
        mat4_perspective(deg_to_rad(45), 1280 / 720.0f, state_ptr->near_clip,
                         state_ptr->far_clip);
    state_ptr->view = mat4_translation((vec3){{0, 0, 30.0f}});
    state_ptr->view = mat4_inverse(state_ptr->view);

    return true;
}

void renderer_shutdown(void *state) {
    (void)state;
    if (state_ptr && state_ptr->backend && state_ptr->backend->shutdown) {
        state_ptr->backend->shutdown(state_ptr->backend);
    }
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
        state_ptr->projection =
            mat4_perspective(deg_to_rad(45.0f), width / (f32)height,
                             state_ptr->near_clip, state_ptr->far_clip);
        state_ptr->backend->resized(state_ptr->backend, width, height);
    } else {
        KWARN("renderer_state_ptr->backend does not exist to accept resize.");
    }
}

b8 renderer_draw_frame(render_packet *packet) {
    if (renderer_begin_frame(packet->delta_time)) {

        state_ptr->backend->update_global_state(
            state_ptr->projection, state_ptr->view, vec3_zero(), vec4_one(), 0);

        static f32 angle = 0.001f;
        angle += 0.0005f;
        quat rotation = quat_from_axis_angle(vec3_forward(), angle, false);
        mat4 model = quat_to_rotation_matrix(rotation, vec3_zero());
        geometry_render_data data = {};
        data.object_id = 0;
        data.model = model;
        state_ptr->backend->update_object(state_ptr->backend, data);

        b8 result = renderer_end_frame(packet->delta_time);
        if (!result) {
            KERROR("renderer_end_frame failed. Application shutting down...");
            return false;
        }
    }

    return true;
}

void renderer_set_view(mat4 view) { state_ptr->view = view; }

void renderer_create_texture(b8 auto_release, i32 width, i32 height,
                             i32 channel_count, const u8 *pixels,
                             b8 has_transparency, struct texture *out_texture) {
    state_ptr->backend->create_texture(auto_release, width, height,
                                       channel_count, pixels, has_transparency,
                                       out_texture);
}

void renderer_destroy_texture(struct texture *texture) {
    state_ptr->backend->destroy_texture(texture);
}
