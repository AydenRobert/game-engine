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
