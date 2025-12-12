#pragma once

#include "defines.h"

#include "math/math_types.h"
#include "resources/resource_types.h"
#include "systems/shader_system.h"

#define BUILTIN_SHADER_NAME_MATERIAL "Shader.Builtin.Material"
#define BUILTIN_SHADER_NAME_UI "Shader.Builtin.UI"

#define MAX_SHADER_STAGE_COUNT 5
#define MAX_SHADER_ATTRIBUTE_COUNT 16
#define MAX_SHADER_UNIFORM_COUNT 16

typedef enum renderer_backend_type {
    RENDERER_BACKEND_TYPES_VULKAN,
    RENDERER_BACKEND_TYPES_OPENGL,
    RENDERER_BACKEND_TYPES_DIRECTX
} renderer_backend_type;

typedef struct geometry_render_data {
    mat4 model;
    geometry *geometry;
} geometry_render_data;

typedef enum builtin_renderpass {
    BUILTIN_RENDERPASS_WORLD = 0x01,
    BUILTIN_RENDERPASS_UI = 0x02,
} builtin_renderpass;

typedef struct renderer_backend {
    struct platform_state *plat_state;
    u64 frame_number;

    b8 (*initialize)(struct renderer_backend *backend,
                     const char *application_name,
                     struct platform_state *plat_state);
    void (*shutdown)(struct renderer_backend *backend);

    void (*resized)(struct renderer_backend *backend, u16 width, u16 height);

    b8 (*begin_frame)(struct renderer_backend *backend, f32 delta_time);
    b8 (*end_frame)(struct renderer_backend *backend, f32 delta_time);

    b8 (*begin_renderpass)(struct renderer_backend *backend, u8 renderpass_id);
    b8 (*end_renderpass)(struct renderer_backend *backend, u8 renderpass_id);

    void (*draw_geometry)(struct renderer_backend *backend,
                          geometry_render_data data);

    void (*create_texture)(const u8 *pixels, struct texture *texture);
    void (*destroy_texture)(struct texture *texture);

    b8 (*create_geometry)(geometry *geometry, u32 vertex_size, u32 vertex_count,
                          const void *vertices, u32 index_size, u32 index_count,
                          const void *indices);
    void (*destroy_geometry)(geometry *geometry);

    b8 (*shader_create)(struct shader *s, u8 renderpass_id, u8 stage_count,
                        const char **stage_filenames, shader_stage *stages);
    void (*shader_destroy)(struct shader *s);

    b8 (*shader_initialize)(struct shader *s);
    b8 (*shader_use)(struct shader *s);

    b8 (*shader_bind_globals)(struct shader *s);
    b8 (*shader_bind_instance)(struct shader *s, u32 instance_id);

    b8 (*shader_apply_globals)(struct shader *s);
    b8 (*shader_apply_instance)(struct shader *s);

    b8 (*shader_acquire_instance_resources)(struct shader *s,
                                            u32 *out_instance_id);
    b8 (*shader_release_instance_resources)(struct shader *s, u32 instance_id);

    b8 (*set_uniform)(struct shader *s, struct shader_uniform *uniform,
                      const void *value);

} renderer_backend;

typedef struct render_packet {
    f32 delta_time;

    u32 geometry_count;
    geometry_render_data *geometries;

    u32 ui_geometry_count;
    geometry_render_data *ui_geometries;
} render_packet;
