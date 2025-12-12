#pragma once

#include "renderer/vulkan/vulkan_types.inl"
#include "systems/shader_system.h"

#define VULKAN_SHADER_MAX_STAGES 8
#define VULKAN_SHADER_MAX_GLOBAL_TEXTURES 31
#define VULKAN_SHADER_MAX_INSTANCE_TEXTURES 31
#define VULKAN_SHADER_MAX_ATTRIBUTES 16

#define VULKAN_SHADER_MAX_UNIFORMS 128
#define VULKAN_SHADER_MAX_BINDINGS 32
#define VULKAN_SHADER_MAX_PUSH_CONST_RANGES 32

typedef struct vulkan_shader_stage_config {
    VkShaderStageFlagBits stage;
    char file_name[255];
} vulkan_shader_stage_config;

typedef struct vulkan_descriptor_set_config {
    u8 binding_count;
    VkDescriptorSetLayoutBinding bindings[VULKAN_SHADER_MAX_BINDINGS];
} vulkan_descriptor_set_config;

typedef struct vulkan_shader_config {
    u8 stage_count;
    vulkan_shader_stage_config stages[VULKAN_SHADER_MAX_STAGES];
    VkDescriptorPoolSize pool_sizes[2];
    u16 max_descriptor_set_count;
    u8 descriptor_set_count;
    vulkan_descriptor_set_config descriptor_sets[2];
    // Darray
    VkVertexInputAttributeDescription *attributes;
} vulkan_shader_config;

typedef struct vulkan_shader_descriptor_set_state {
    VkDescriptorSet descriptor_sets[3];
    vulkan_descriptor_state descriptor_states[VULKAN_SHADER_MAX_BINDINGS];
} vulkan_shader_descriptor_set_state;

typedef struct vulkan_shader_instance_state {
    u32 id;
    u64 offset;
    vulkan_shader_descriptor_set_state descriptor_set_state;
    struct texture **instance_textures;
} vulkan_shader_instance_state;

typedef struct vulkan_shader {
    void *mapped_uniform_buffer_block;
    u32 id;
    vulkan_shader_config config;
    vulkan_renderpass *renderpass;
    vulkan_shader_stage stages[VULKAN_SHADER_MAX_STAGES];
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_set_layouts[2];
    VkDescriptorSet global_descriptor_sets[3];
    vulkan_buffer uniform_buffer;
    vulkan_pipeline pipeline;
    u32 instance_count;
    vulkan_shader_instance_state instance_states[VULKAN_MAX_MATERIAL_COUNT];
} vulkan_shader;
