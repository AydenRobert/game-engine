#pragma once

#include "renderer/renderer_types.inl"
#include "systems/shader_system.h"

struct static_mesh_data;
struct platform_state;

// SYSTEM FUNCTIONS

b8 renderer_initialize(const char *application_name,
                       struct platform_state *plat_state,
                       u64 *memory_requirement, void *state);
void renderer_shutdown(void *state);

// MAIN FUNCTIONS

void renderer_on_resize(u16 width, u16 height);
b8 renderer_draw_frame(render_packet *packet);

// TEXTURE FUNCTIONS

void renderer_texture_create(const u8 *pixels, struct texture *texture);
void renderer_texture_destroy(struct texture *texture);
void renderer_texture_create_writeable(struct texture *t);
void renderer_texture_resize(struct texture *t, u32 new_width, u32 new_height);
void renderer_texture_write_data(struct texture *t, u32 offset, u32 size,
                                 const u8 *pixels);
b8 renderer_texture_map_acquire_resources(texture_map *map);
void renderer_texture_map_release_resources(texture_map *map);

// GEOMETRY FUNCTIONS

b8 renderer_geometry_create(geometry *geometry, u32 vertex_size,
                            u32 vertex_count, const void *vertices,
                            u32 index_size, u32 index_count,
                            const void *indices);
void renderer_geometry_destroy(geometry *geometry);
void renderer_geometry_draw(geometry_render_data *data);

// RENDER TARGET FUNCTIONS

void renderer_render_target_create(u8 attachment_count, texture **attachments,
                                   renderpass *pass, u32 width, u32 height,
                                   render_target *out_target);
void renderer_render_target_destroy(render_target *target,
                                    b8 free_internal_memory);

// RENDERPASS FUNCTIONS

void renderer_renderpass_create(renderpass *out_renderpass, f32 depth,
                                u32 stencil, b8 has_prev_pass,
                                b8 has_next_pass);
void renderer_renderpass_destroy(renderpass *renderpass);
b8 renderer_renderpass_begin(renderpass *pass, render_target *target);
b8 renderer_renderpass_end(renderpass *pass);
renderpass *renderer_renderpass_get(const char *name);

// SHADER FUNCTIONS

b8 renderer_shader_create(struct shader *s, renderpass *pass, u8 stage_count,
                          const char **stage_filenames, shader_stage *stages);
void renderer_shader_destroy(struct shader *s);
b8 renderer_shader_initialize(struct shader *s);
b8 renderer_shader_use(struct shader *s);
b8 renderer_shader_bind_globals(struct shader *s);
b8 renderer_shader_bind_instance(struct shader *s, u32 instance_id);
b8 renderer_shader_apply_globals(struct shader *s);
b8 renderer_shader_apply_instance(struct shader *s, b8 needs_update);
b8 renderer_shader_acquire_instance_resources(struct shader *s,
                                              texture_map **maps,
                                              u32 *out_instance_id);
b8 renderer_shader_release_instance_resources(struct shader *s,
                                              u32 instance_id);
b8 renderer_set_uniform(struct shader *s, struct shader_uniform *uniform,
                        const void *value);
