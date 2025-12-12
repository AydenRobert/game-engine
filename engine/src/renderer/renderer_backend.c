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

        out_renderer_backend->begin_renderpass =
            vulkan_renderer_begin_renderpass;
        out_renderer_backend->end_renderpass = vulkan_renderer_end_renderpass;

        out_renderer_backend->resized = vulkan_renderer_backend_on_resized;

        out_renderer_backend->draw_geometry = vulkan_renderer_draw_geometry;

        out_renderer_backend->create_texture = vulkan_renderer_create_texture;
        out_renderer_backend->destroy_texture = vulkan_renderer_destroy_texture;

        out_renderer_backend->create_geometry = vulkan_renderer_create_geometry;
        out_renderer_backend->destroy_geometry =
            vulkan_renderer_destroy_geometry;

        out_renderer_backend->shader_create = vulkan_renderer_shader_create;
        out_renderer_backend->shader_destroy = vulkan_renderer_shader_destroy;

        out_renderer_backend->shader_initialize =
            vulkan_renderer_shader_initialize;
        out_renderer_backend->shader_use = vulkan_renderer_shader_use;

        out_renderer_backend->shader_bind_globals =
            vulkan_renderer_shader_bind_globals;
        out_renderer_backend->shader_bind_instance =
            vulkan_renderer_shader_bind_instance;

        out_renderer_backend->shader_apply_globals =
            vulkan_renderer_shader_apply_globals;
        out_renderer_backend->shader_apply_instance =
            vulkan_renderer_shader_apply_instance;

        out_renderer_backend->shader_acquire_instance_resources =
            vulkan_renderer_shader_acquire_instance_resources;
        out_renderer_backend->shader_release_instance_resources =
            vulkan_renderer_shader_release_instance_resources;

        out_renderer_backend->set_uniform = vulkan_renderer_set_uniform;

        return true;
    }

    return false;
}

void renderer_backend_destroy(struct renderer_backend *renderer_backend) {
    renderer_backend->initialize = 0;
    renderer_backend->shutdown = 0;

    renderer_backend->begin_frame = 0;
    renderer_backend->end_frame = 0;

    renderer_backend->begin_renderpass = 0;
    renderer_backend->end_renderpass = 0;

    renderer_backend->resized = 0;

    renderer_backend->draw_geometry = 0;

    renderer_backend->create_texture = 0;
    renderer_backend->destroy_texture = 0;

    renderer_backend->create_geometry = 0;
    renderer_backend->destroy_geometry = 0;

    renderer_backend->shader_create = 0;
    renderer_backend->shader_destroy = 0;

    renderer_backend->shader_initialize = 0;
    renderer_backend->shader_use = 0;

    renderer_backend->shader_bind_globals = 0;
    renderer_backend->shader_bind_instance = 0;

    renderer_backend->shader_apply_globals = 0;
    renderer_backend->shader_apply_instance = 0;

    renderer_backend->shader_acquire_instance_resources = 0;
    renderer_backend->shader_release_instance_resources = 0;

    renderer_backend->set_uniform = 0;
}
