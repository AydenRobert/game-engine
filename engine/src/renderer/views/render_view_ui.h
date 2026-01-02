#pragma once

#include "defines.h"
#include "renderer/renderer_types.inl"

b8 render_view_ui_on_create(render_view *self);
void render_view_ui_on_destroy(render_view *self);
void render_view_ui_on_resize(render_view *self, u16 width, u16 height);
b8 render_view_ui_on_build_packet(const render_view *self, void *data,
                                  render_view_packet *out_packet);
b8 render_view_ui_on_render(const render_view *self,
                            const render_view_packet *packet, u64 frame_number,
                            u64 render_target_index);
