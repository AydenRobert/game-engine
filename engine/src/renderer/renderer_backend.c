#include "renderer/renderer_backend.h"

#include "core/event.h"
#include "core/logger.h"
#include "renderer/renderer_types.inl"

#include "renderer/vulkan/vulkan_backend.h"

b8 on_resize_event(u16 code, void *sender, void *listener_inst,
                           event_context data);

b8 renderer_backend_create(renderer_backend_type type,
                           struct platform_state *plat_state,
                           struct renderer_backend *out_renderer_backend) {
    out_renderer_backend->plat_state = plat_state;

    if (type == RENDERER_BACKEND_TYPES_VULKAN) {
        out_renderer_backend->initialize = vulkan_renderer_backend_initialize;
        out_renderer_backend->shutdown = vulkan_renderer_backend_shutdown;
        out_renderer_backend->begin_frame = vulkan_renderer_backend_begin_frame;
        out_renderer_backend->end_frame = vulkan_renderer_backend_end_frame;
        out_renderer_backend->resized = vulkan_renderer_backend_on_resized;

        return TRUE;
    }


    return FALSE;
}

void renderer_backend_destroy(struct renderer_backend *renderer_backend) {
    renderer_backend->initialize = 0;
    renderer_backend->shutdown = 0;
    renderer_backend->begin_frame = 0;
    renderer_backend->end_frame = 0;
    renderer_backend->resized = 0;
}
