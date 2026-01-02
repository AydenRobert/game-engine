#include "renderer/views/render_view_ui.h"

#include "core/kmemory.h"
#include "core/logger.h"

#include "containers/darray.h"

#include "math/kmath.h"
#include "math/transform.h"

#include "renderer/renderer_frontend.h"
#include "renderer/renderer_types.inl"

#include "systems/material_system.h"
#include "systems/shader_system.h"

typedef struct render_view_ui_internal_data {
    u32 shader_id;
    f32 near_clip;
    f32 far_clip;
    mat4 projection_matrix;
    mat4 view_matrix;
    // u32 render_mode;
} render_view_ui_internal_data;

b8 render_view_ui_on_create(render_view *self) {
    if (!self) {
        KERROR("render_view_ui_on_create - self not passed.");
        return false;
    }

    self->internal_data =
        kallocate(sizeof(render_view_ui_internal_data), MEMORY_TAG_RENDERER);
    render_view_ui_internal_data *data = self->internal_data;

    data->shader_id =
        shader_system_get_id(self->custom_shader_name ? self->custom_shader_name
                                                      : BUILTIN_SHADER_NAME_UI);
    // TODO: set from config
    data->near_clip = -100.0f;
    data->far_clip = 100.0f;

    data->projection_matrix = mat4_orthographic(
        0.0f, 1280.0f, 720.0f, 0.0f, data->near_clip, data->far_clip);
    data->view_matrix = mat4_identity();

    return true;
}

void render_view_ui_on_destroy(render_view *self) {
    if (!self || !self->internal_data) {
        return;
    }

    kfree(self->internal_data, sizeof(render_view_ui_internal_data),
          MEMORY_TAG_RENDERER);
    self->internal_data = 0;
}

void render_view_ui_on_resize(render_view *self, u16 width, u16 height) {
    if (!self || (width == self->width && height == self->height)) {
        return;
    }

    render_view_ui_internal_data *data = self->internal_data;
    self->width = width;
    self->height = height;
    data->projection_matrix =
        mat4_orthographic(0.0f, (f32)self->width, (f32)self->height, 0.0f,
                          data->near_clip, data->far_clip);

    for (u32 i = 0; i < self->renderpass_count; i++) {
        self->passes[i]->render_area.x = 0;
        self->passes[i]->render_area.y = 0;
        self->passes[i]->render_area.z = width;
        self->passes[i]->render_area.w = height;
    }
}

b8 render_view_ui_on_build_packet(const render_view *self, void *data,
                                  render_view_packet *out_packet) {
    if (!self || !data || !out_packet) {
        KERROR("render_view_ui_on_build_packet - requires valid (render_view "
               "*, void *, render_view_packet *)");
        return false;
    }

    mesh_packet_data *mesh_data = (mesh_packet_data *)data;
    render_view_ui_internal_data *internal_data = self->internal_data;

    // NOTE: wasteful / slow, possible array reuse?
    out_packet->geometries = darray_create(geometry_render_data);
    out_packet->view = self;

    out_packet->projection_matrix = internal_data->projection_matrix;
    out_packet->view_matrix = internal_data->view_matrix;

    for (u32 i = 0; i < mesh_data->mesh_count; i++) {
        mesh *m = &mesh_data->meshes[i];
        for (u32 j = 0; j < m->geometry_count; j++) {
            geometry_render_data render_data;
            render_data.geometry = m->geometries[j];
            render_data.model = transform_get_world(&m->transform);
            darray_push(out_packet->geometries, render_data);
            out_packet->geometry_count++;
        }
    }

    return true;
}

b8 render_view_ui_on_render(const render_view *self,
                            const render_view_packet *packet, u64 frame_number,
                            u64 render_target_index) {
    render_view_ui_internal_data *internal_data = self->internal_data;
    u32 shader_id = internal_data->shader_id;

    for (u32 i = 0; i < self->renderpass_count; i++) {
        renderpass *pass = self->passes[i];
        if (!renderer_renderpass_begin(pass,
                                       &pass->targets[render_target_index])) {
            KERROR("render_view_ui_on_render - pass index %u failed to start.",
                   i);
            return false;
        }

        if (!shader_system_use_by_id(shader_id)) {
            KERROR("render_view_ui_on_render - Failed to use shader, Render "
                   "frame failed.");
            return false;
        }

        if (!material_system_apply_global(shader_id, frame_number,
                                          &packet->projection_matrix,
                                          &packet->view_matrix, 0, 0, 0)) {
            KERROR("render_view_ui_on_render - failed to apply globals.");
            return false;
        }

        u32 count = packet->geometry_count;
        for (u32 j = 0; j < count; j++) {
            material *m = 0;
            if (packet->geometries[j].geometry->material) {
                m = packet->geometries[j].geometry->material;
            } else {
                m = material_system_get_default();
            }

            // Apply the material if needed
            b8 needs_update = m->render_frame_number != frame_number;
            if (!material_system_apply_instance(m, needs_update)) {
                KWARN("Failed to apply material '%s'. Skipping draw.", m->name);
                continue;
            } else {
                m->render_frame_number = frame_number;
            }

            // Apply the locals
            material_system_apply_local(m, &packet->geometries[j].model);

            // Draw it
            renderer_geometry_draw(&packet->geometries[j]);
        }

        if (!renderer_renderpass_end(pass)) {
            KERROR("render_view_ui_on_render - renderpass %u end failed.", i);
            return false;
        }

    }

    return true;
}
