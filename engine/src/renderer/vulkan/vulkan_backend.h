#pragma once

#include "renderer/renderer_backend.h"
#include "resources/resource_types.h"
#include "systems/shader_system.h"

struct shader;
struct shader_uniform;

b8 vulkan_renderer_backend_initialize(renderer_backend *backend,
                                      const char *application_name,
                                      struct platform_state *plat_state);
void vulkan_renderer_backend_shutdown(renderer_backend *backend);

void vulkan_renderer_backend_on_resized(renderer_backend *backend, u16 width,
                                        u16 height);
b8 vulkan_renderer_backend_begin_frame(renderer_backend *backend,
                                       f32 delta_time);
b8 vulkan_renderer_backend_end_frame(renderer_backend *backend, f32 delta_time);

b8 vulkan_renderer_begin_renderpass(renderer_backend *backend,
                                    u8 renderpass_id);
b8 vulkan_renderer_end_renderpass(renderer_backend *backend, u8 renderpass_id);

void vulkan_renderer_update_global_world_state(mat4 projection, mat4 view,
                                               vec3 view_position,
                                               vec4 ambient_colour, i32 mode);
void vulkan_renderer_update_global_ui_state(mat4 projection, mat4 view,
                                            i32 mode);
void vulkan_renderer_draw_geometry(renderer_backend *backend,
                                   geometry_render_data data);

void vulkan_renderer_create_texture(const u8 *pixels, struct texture *texture);
void vulkan_renderer_destroy_texture(texture *texture);

b8 vulkan_renderer_create_material(struct material *material);
void vulkan_renderer_destroy_material(struct material *material);

b8 vulkan_renderer_create_geometry(geometry *geometry, u32 vertex_size,
                                   u32 vertex_count, const void *vertices,
                                   u32 index_size, u32 index_count,
                                   const void *indices);
void vulkan_renderer_destroy_geometry(geometry *geometry);

b8 vulkan_renderer_shader_create(struct shader *shader, u8 renderpass_id,
                                 u8 stage_count, const char **stage_filenames,
                                 shader_stage *stages);
void vulkan_renderer_shader_destroy(struct shader *shader);

b8 vulkan_renderer_shader_initialize(struct shader *s);
b8 vulkan_renderer_shader_use(struct shader *s);
b8 vulkan_renderer_shader_bind_globals(struct shader *s);
b8 vulkan_renderer_shader_bind_instance(struct shader *s, u32 instance_id);
b8 vulkan_renderer_shader_apply_globals(struct shader *s);
b8 vulkan_renderer_shader_apply_instance(struct shader *s);
b8 vulkan_renderer_shader_acquire_instance_resources(struct shader *s,
                                                     u32 *out_instance_id);
b8 vulkan_renderer_shader_release_instnace_resources(struct shader *s,
                                                     u32 instance_id);
b8 vulkan_renderer_shader_set_uniform(struct shader *s,
                                      struct shader_uniform *uniform,
                                      const void *value);
