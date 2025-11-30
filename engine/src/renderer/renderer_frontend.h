#pragma once

#include "renderer/renderer_types.inl"

struct static_mesh_data;
struct platform_state;

b8 renderer_initialize(const char *application_name,
                       struct platform_state *plat_state,
                       u64 *memory_requirement, void *state);
void renderer_shutdown(void *state);

void renderer_on_resize(u16 width, u16 height);

b8 renderer_draw_frame(render_packet *packet);

void renderer_create_texture(const u8 *pixels, struct texture *texture);
void renderer_destroy_texture(struct texture *texture);

b8 renderer_create_material(struct material *material);
void renderer_destroy_material(struct material *material);

b8 renderer_create_geometry(geometry *geometry, u32 vertex_size,
                            u32 vertex_count, const void *vertices,
                            u32 index_size, u32 index_count,
                            const void *indices);
void renderer_destroy_geometry(geometry *geometry);

// HACK: this should not be exposed outside the engine
KAPI void renderer_set_view(mat4 view);
