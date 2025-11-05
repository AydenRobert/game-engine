#include "renderer/renderer_frontend.h"

#include "renderer/renderer_backend.h"

#include "core/kmemory.h"
#include "core/logger.h"
#include "renderer/renderer_types.inl"

// Backend render context.
static renderer_backend *backend = 0;

b8 renderer_initialize(const char *application_name,
                       struct platform_state *plat_state) {
    backend = kallocate(sizeof(renderer_backend), MEMORY_TAG_RENDERER);
    backend->frame_number = 0;

    // TODO: make this configurable
    renderer_backend_create(RENDERER_BACKEND_TYPES_VULKAN, plat_state, backend);

    if (!backend->initialize(backend, application_name, plat_state)) {
        KFATAL("Renderer backend failed to initialize. Shutting down.");
        return false;
    }

    return true;
}

void renderer_shutdown() {
    backend->shutdown(backend);
    kfree(backend, sizeof(renderer_backend), MEMORY_TAG_RENDERER);
}

b8 renderer_begin_frame(f32 delta_time) {
    return backend->begin_frame(backend, delta_time);
}

b8 renderer_end_frame(f32 delta_time) {
    b8 result = backend->end_frame(backend, delta_time);
    backend->frame_number++;
    return result;
}

void renderer_on_resize(u16 width, u16 height) {
    if (backend) {
        backend->resized(backend, width, height);
    } else {
        KWARN("renderer_backend does not exist to accept resize.");
    }
}

b8 renderer_draw_frame(render_packet *packet) {
    // If begin frame returns successfully, then mid frame operation can
    // continue.
    if (renderer_begin_frame(packet->delta_time)) {

        // If renderer_end_frame fails, likely unrecoverable
        b8 result = renderer_end_frame(packet->delta_time);
        if (!result) {
            KERROR("renderer_end_frame failed. Application shutting down...");
            return false;
        }
    }

    return true;
}
