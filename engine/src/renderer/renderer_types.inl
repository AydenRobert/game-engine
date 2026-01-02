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

typedef enum renderer_debug_view_mode {
    RENDERER_VIEW_MODE_DEFAULT,
    RENDERER_VIEW_MODE_LIGHTING,
    RENDERER_VIEW_MODE_NORMALS
} renderer_debug_view_mode;

typedef struct render_target {
    b8 sync_to_window_size;
    u8 attatchment_count;
    texture **attatchments;
    void *internal_framebuffer;
} render_target;

typedef enum renderpass_clear_flag {
    RENDERPASS_CLEAR_FLAG_NONE = 0x00,
    RENDERPASS_CLEAR_FLAG_COLOUR_BUFFER = 0x01,
    RENDERPASS_CLEAR_FLAG_DEPTH_BUFFER = 0x02,
    RENDERPASS_CLEAR_FLAG_STENCIL_BUFFER = 0x04,
} renderpass_clear_flag;

typedef struct renderpass_config {
    const char *name;
    const char *prev_name;
    const char *next_name;
    vec4 render_area;
    vec4 clear_colour;
    u8 clear_flags;
} renderpass_config;

typedef struct renderpass {
    u32 id;
    vec4 render_area;
    vec4 clear_colour;
    u8 clear_flags;
    u8 render_target_count;
    render_target *targets;
    void *internal_data;
} renderpass;

typedef struct renderer_backend_config {
    const char *application_name;
    u16 renderpass_count;
    renderpass_config *pass_configs;
    void (*on_rendertarget_refresh_required)();
} renderer_backend_config;

typedef struct renderer_backend {
    struct platform_state *plat_state;
    u64 frame_number;

    b8 (*initialize)(struct renderer_backend *backend,
                     const renderer_backend_config *config,
                     u8 *out_window_render_target_count,
                     struct platform_state *plat_state);
    void (*shutdown)(struct renderer_backend *backend);

    void (*resized)(struct renderer_backend *backend, u16 width, u16 height);

    b8 (*begin_frame)(struct renderer_backend *backend, f32 delta_time);
    b8 (*end_frame)(struct renderer_backend *backend, f32 delta_time);

    b8 (*renderpass_begin)(renderpass *pass, render_target *target);
    b8 (*renderpass_end)(renderpass *pass);

    renderpass *(*renderpass_get)(const char *name);

    // NOTE: expects geometry_render_data at alignment of 16
    void (*draw_geometry)(struct renderer_backend *backend,
                          geometry_render_data *data);

    void (*texture_create)(const u8 *pixels, struct texture *texture);
    void (*texture_destroy)(struct texture *texture);

    void (*texture_create_writeable)(struct texture *t);
    void (*texture_resize)(struct texture *t, u32 new_width, u32 new_height);
    void (*texture_write_data)(struct texture *t, u32 offset, u32 size,
                               const u8 *pixels);

    b8 (*create_geometry)(geometry *geometry, u32 vertex_size, u32 vertex_count,
                          const void *vertices, u32 index_size, u32 index_count,
                          const void *indices);
    void (*destroy_geometry)(geometry *geometry);

    b8 (*shader_create)(struct shader *s, renderpass *pass, u8 stage_count,
                        const char **stage_filenames, shader_stage *stages);
    void (*shader_destroy)(struct shader *s);

    b8 (*shader_initialize)(struct shader *s);
    b8 (*shader_use)(struct shader *s);

    b8 (*shader_bind_globals)(struct shader *s);
    b8 (*shader_bind_instance)(struct shader *s, u32 instance_id);

    b8 (*shader_apply_globals)(struct shader *s);
    b8 (*shader_apply_instance)(struct shader *s, b8 needs_update);

    b8 (*shader_acquire_instance_resources)(struct shader *s,
                                            texture_map **maps,
                                            u32 *out_instance_id);
    b8 (*shader_release_instance_resources)(struct shader *s, u32 instance_id);

    b8 (*set_uniform)(struct shader *s, struct shader_uniform *uniform,
                      const void *value);

    b8 (*texture_map_acquire_resources)(texture_map *map);
    void (*texture_map_release_resources)(texture_map *map);

    void (*render_target_create)(u8 attachment_count, texture **attachments,
                                 renderpass *pass, u32 width, u32 height,
                                 render_target *out_target);
    void (*render_target_destroy)(render_target *target,
                                  b8 free_internal_memory);

    void (*renderpass_create)(renderpass *out_renderpass, f32 depth,
                              u32 stencil, b8 has_prev_pass, b8 has_next_pass);
    void (*renderpass_destroy)(renderpass *pass);

    texture *(*window_attachment_get)(u8 index);

    texture *(*depth_attachment_get)();

    u8 (*window_attachment_index_get)();
} renderer_backend;

typedef enum render_view_known_type {
    RENDER_VIEW_KNOWN_TYPE_WORLD = 0x01,
    RENDER_VIEW_KNOWN_TYPE_UI = 0x02,
} render_view_known_type;

typedef enum render_view_view_matrix_source {
    RENDER_VIEW_VIEW_MATRIX_SOURCE_SCENE_CAMERA = 0X01,
    RENDER_VIEW_VIEW_MATRIX_SOURCE_UI_CAMERA = 0X02,
    RENDER_VIEW_VIEW_MATRIX_SOURCE_LIGHT_CAMERA = 0X03,
} render_view_view_matrix_source;

typedef enum render_view_projectection_matrix_source {
    RENDER_VIEW_PROJECTECTION_MATRIX_SOURCE_DEFAULT_PERSPECTIVE = 0x01,
    RENDER_VIEW_PROJECTECTION_MATRIX_SOURCE_DEFAULT_ORTHOGRAPHIC = 0x02,
} render_view_projection_matrix_source;

typedef struct render_view_pass_config {
    const char *name;
} render_view_pass_config;

typedef struct render_view_config {
    const char *name;
    const char *custom_shader_name;
    /* Set these to 0 for 100% */
    u16 width;
    u16 height;
    render_view_known_type type;
    render_view_view_matrix_source view_matrix_source;
    render_view_projection_matrix_source projection_matrix_source;
    u8 pass_count;
    render_view_pass_config *passes;
} render_view_config;

typedef struct render_view_packet render_view_packet;

typedef struct render_view render_view;

struct render_view {
    u16 id;
    const char *name;
    u16 width;
    u16 height;
    render_view_known_type type;
    u8 renderpass_count;
    renderpass **passes;
    const char *custom_shader_name;
    void *internal_data;

    b8 (*on_create)(render_view *self);
    void (*on_destroy)(render_view *self);
    void (*on_resize)(render_view *self, u16 width, u16 height);
    b8 (*on_build_packet)(const render_view *self, void *data,
                            render_view_packet *out_packet);
    b8 (*on_render)(const render_view *self, const render_view_packet *packet,
                      u64 frame_number, u64 render_target_index);
};

struct render_view_packet {
    const render_view *view;
    mat4 view_matrix;
    mat4 projection_matrix;
    vec3 view_position;
    vec4 ambient_colour;
    u32 geometry_count;
    geometry_render_data *geometries;
    const char *custom_shader_name;
    void *extended_data;
};

typedef struct mesh_packet_data {
    u32 mesh_count;
    mesh *meshes;
} mesh_packet_data;

typedef struct render_packet {
    f32 delta_time;

    u16 view_count;
    render_view_packet *views;
} render_packet;
