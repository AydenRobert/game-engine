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

b8 renderer_begin_frame(f32 delta_time);

b8 renderer_end_frame(f32 delta_time);

void renderer_create_texture(b8 auto_release, i32 width, i32 height,
                             i32 channel_count, const u8 *pixels,
                             b8 has_transparency, struct texture *out_texture);

void renderer_destroy_texture(struct texture *texture);

// HACK: this should not be exposed outside the engine
KAPI void renderer_set_view(mat4 view);
