#include "renderer/views/render_view_world.h"

#include "core/event.h"
#include "core/kmemory.h"
#include "core/logger.h"

#include "containers/darray.h"

#include "math/kmath.h"
#include "math/transform.h"

#include "renderer/camera.h"
#include "renderer/renderer_frontend.h"
#include "renderer/renderer_types.inl"

#include "systems/camera_system.h"
#include "systems/material_system.h"
#include "systems/shader_system.h"

typedef struct render_view_world_internal_data {
    u32 shader_id;
    f32 fov;
    f32 near_clip;
    f32 far_clip;
    mat4 projection_matrix;
    camera *world_camera;
    vec4 ambient_colour;
    u32 render_mode;
} render_view_world_internal_data;

typedef struct geometry_distance {
    geometry_render_data g;
    f32 distance;
} geometry_distance;

static void quick_sort(geometry_distance arr[], i32 low_index, i32 high_index,
                       b8 ascending);

static b8 render_view_on_event(u16 code, void *sender, void *listener_inst,
                               event_context context) {
    if (!listener_inst || !((render_view *)listener_inst)->internal_data) {
        return false;
    }

    render_view *self = (render_view *)listener_inst;
    render_view_world_internal_data *data = self->internal_data;

    switch (code) {
    case EVENT_CODE_SET_RENDER_MODE: {
        i32 mode = context.data.i32[0];
        switch (mode) {
        default:
        case RENDERER_VIEW_MODE_DEFAULT:
            KDEBUG("Renderer mode set to default.");
            data->render_mode = RENDERER_VIEW_MODE_DEFAULT;
            break;
        case RENDERER_VIEW_MODE_LIGHTING:
            KDEBUG("Renderer mode set to lighting.");
            data->render_mode = RENDERER_VIEW_MODE_LIGHTING;
            break;
        case RENDERER_VIEW_MODE_NORMALS:
            KDEBUG("Renderer mode set to normals.");
            data->render_mode = RENDERER_VIEW_MODE_NORMALS;
            break;
        }
        return true;
    }
    }

    return false;
}

b8 render_view_world_on_create(render_view *self) {
    if (!self) {
        KERROR("render_view_world_on_create - self not passed.");
        return false;
    }

    self->internal_data =
        kallocate(sizeof(render_view_world_internal_data), MEMORY_TAG_RENDERER);
    render_view_world_internal_data *data = self->internal_data;

    data->shader_id = shader_system_get_id(self->custom_shader_name
                                               ? self->custom_shader_name
                                               : BUILTIN_SHADER_NAME_MATERIAL);
    // TODO: set from config
    data->near_clip = 0.1f;
    data->far_clip = 1000.0f;
    data->fov = deg_to_rad(45.0f);

    data->projection_matrix = mat4_perspective(data->fov, 1280.0f / 720.0f,
                                               data->near_clip, data->far_clip);
    data->world_camera = camera_system_get_default();

    // TODO: get from scene
    data->ambient_colour = (vec4){{0.25f, 0.25f, 0.25f, 0.0f}};

    if (!event_register(EVENT_CODE_SET_RENDER_MODE, self,
                        render_view_on_event)) {
        KERROR("render_view_world_on_create - failed to register "
               "render_view_on_event.");
        return false;
    }

    return true;
}

void render_view_world_on_destroy(render_view *self) {
    if (!self || !self->internal_data) {
        return;
    }

    event_unregister(EVENT_CODE_SET_RENDER_MODE, self, render_view_on_event);
    kfree(self->internal_data, sizeof(render_view_world_internal_data),
          MEMORY_TAG_RENDERER);
    self->internal_data = 0;
}

void render_view_world_on_resize(render_view *self, u16 width, u16 height) {
    if (!self || (width == self->width && height == self->height)) {
        return;
    }

    render_view_world_internal_data *data = self->internal_data;
    self->width = width;
    self->height = height;
    data->projection_matrix = mat4_perspective(
        data->fov, (f32)width / (f32)height, data->near_clip, data->far_clip);

    for (u32 i = 0; i < self->renderpass_count; i++) {
        self->passes[i]->render_area.x = 0;
        self->passes[i]->render_area.y = 0;
        self->passes[i]->render_area.z = width;
        self->passes[i]->render_area.w = height;
    }
}

b8 render_view_world_on_build_packet(const render_view *self, void *data,
                                     render_view_packet *out_packet) {
    if (!self || !data || !out_packet) {
        KERROR("render_view_world_on_build_packet - reqworldres valid "
               "(render_view "
               "*, void *, render_view_packet *)");
        return false;
    }

    mesh_packet_data *mesh_data = (mesh_packet_data *)data;
    render_view_world_internal_data *internal_data = self->internal_data;

    // NOTE: wasteful / slow, possible array reuse?
    out_packet->geometries = darray_create(geometry_render_data);
    out_packet->view = self;

    out_packet->projection_matrix = internal_data->projection_matrix;
    out_packet->view_matrix = camera_view_get(internal_data->world_camera);
    out_packet->view_position =
        camera_position_get(internal_data->world_camera);
    out_packet->ambient_colour = internal_data->ambient_colour;

    geometry_distance *geometry_distances = darray_create(geometry_distance);

    for (u32 i = 0; i < mesh_data->mesh_count; i++) {
        mesh *m = &mesh_data->meshes[i];
        mat4 model = transform_get_world(&m->transform);
        for (u32 j = 0; j < m->geometry_count; j++) {
            geometry_render_data render_data;
            render_data.geometry = m->geometries[j];
            render_data.model = model;

            if ((m->geometries[j]->material->diffuse_map.texture->flags &
                 TEXTURE_FLAG_HAS_TRANSPARENCY) == 0) {
                darray_push(out_packet->geometries, render_data);
                out_packet->geometry_count++;
            } else {
                vec3 centre =
                    vec3_transform(render_data.geometry->centre, model);
                f32 distance = vec3_distance(
                    centre, internal_data->world_camera->position);

                geometry_distance gdist;
                gdist.distance = kabs(distance);
                gdist.g = render_data;

                darray_push(geometry_distances, gdist);
            }
        }
    }

    // Sort the distances
    u32 geometry_count = darray_length(geometry_distances);
    quick_sort(geometry_distances, 0, geometry_count - 1, false);

    for (u32 i = 0; i < geometry_count; i++) {
        darray_push(out_packet->geometries, geometry_distances[i].g);
        out_packet->geometry_count++;
    }

    darray_destroy(geometry_distances);

    return true;
}

b8 render_view_world_on_render(const render_view *self,
                               const render_view_packet *packet,
                               u64 frame_number, u64 render_target_index) {
    render_view_world_internal_data *internal_data = self->internal_data;
    u32 shader_id = internal_data->shader_id;

    for (u32 i = 0; i < self->renderpass_count; i++) {
        renderpass *pass = self->passes[i];
        if (!renderer_renderpass_begin(pass,
                                       &pass->targets[render_target_index])) {
            KERROR(
                "render_view_world_on_render - pass index %u failed to start.",
                i);
            return false;
        }

        if (!shader_system_use_by_id(shader_id)) {
            KERROR("render_view_world_on_render - Failed to use shader, Render "
                   "frame failed.");
            return false;
        }

        // TODO: generic data get from scene
        if (!material_system_apply_global(
                shader_id, frame_number, &packet->projection_matrix,
                &packet->view_matrix, &packet->ambient_colour,
                &packet->view_position, internal_data->render_mode)) {
            KERROR("render_view_world_on_render - failed to apply globals.");
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
            KERROR("render_view_world_on_render - renderpass %u end failed.",
                   i);
            return false;
        }
    }

    return true;
}

static void swap(geometry_distance *a, geometry_distance *b) {
    geometry_distance temp = *a;
    *a = *b;
    *b = temp;
}

static i32 partition(geometry_distance arr[], i32 low_index, i32 high_index,
                     b8 ascending) {
    geometry_distance pivot = arr[high_index];
    i32 i = (low_index - 1);

    for (u32 j = low_index; j <= high_index - 1; j++) {
        if (ascending) {
            if (arr[j].distance < pivot.distance) {
                i++;
                swap(&arr[i], &arr[j]);
            }
        } else {
            if (arr[j].distance > pivot.distance) {
                i++;
                swap(&arr[i], &arr[j]);
            }
        }
    }

    swap(&arr[i + 1], &arr[high_index]);
    return i + 1;
}

static void quick_sort(geometry_distance arr[], i32 low_index, i32 high_index,
                       b8 ascending) {
    if (low_index >= high_index)
        return;
    i32 pivot_point = partition(arr, low_index, high_index, ascending);
    quick_sort(arr, low_index, pivot_point - 1, ascending);
    quick_sort(arr, pivot_point + 1, high_index, ascending);
}
