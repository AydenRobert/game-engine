#pragma once

#include "renderer/vulkan/vulkan_types.inl"

typedef enum renderpass_clear_flag {
    RENDERPASS_CLEAR_FLAG_NONE = 0x00,
    RENDERPASS_CLEAR_FLAG_COLOUR_BUFFER = 0x01,
    RENDERPASS_CLEAR_FLAG_DEPTH_BUFFER = 0x02,
    RENDERPASS_CLEAR_FLAG_STENCIL_BUFFER = 0x04
} renderpass_clear_flag;

void vulkan_renderpass_create(vulkan_context *context,
                              vulkan_renderpass *out_renderpass,
                              vec4 render_area, vec4 clear_colour, f32 depth,
                              u32 stencil, u8 clear_flags, b8 has_prev_pass,
                              b8 has_next_pass);

void vulkan_renderpass_destroy(vulkan_context *context,
                               vulkan_renderpass *renderpass);

void vulkan_renderpass_begin(vulkan_command_buffer *command_buffer,
                             vulkan_renderpass *renderpass,
                             VkFramebuffer frame_buffer);

void vulkan_renderpass_end(vulkan_command_buffer *command_buffer,
                           vulkan_renderpass *renderpass);
