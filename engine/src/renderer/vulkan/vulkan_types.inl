#pragma once

#include "containers/freelist.h"
#include "defines.h"

#include "core/asserts.h"
#include "renderer/renderer_types.inl"
#include "resources/resource_types.h"
#include <vulkan/vulkan.h>

#define VK_CHECK(expr)                                                         \
    {                                                                          \
        KASSERT(expr == VK_SUCCESS);                                           \
    }

typedef struct vulkan_context vulkan_context;

typedef struct vulkan_buffer {
    u64 total_size;
    VkBuffer handle;
    VkBufferUsageFlagBits usage;
    b8 is_locked;
    VkDeviceMemory memory;
    i32 memory_index;
    u32 memory_property_flags;
    u64 freelist_memory_requirement;
    void *freelist_block;
    freelist buffer_freelist;
} vulkan_buffer;

typedef struct vulkan_swapchain_support_info {
    VkSurfaceCapabilitiesKHR capabilities;
    u32 format_count;
    VkSurfaceFormatKHR *formats;
    u32 present_mode_count;
    VkPresentModeKHR *present_modes;
} vulkan_swapchain_support_info;

typedef struct vulkan_device {
    VkPhysicalDevice physical_device;
    VkDevice logical_device;
    vulkan_swapchain_support_info swapchain_support;

    b8 supports_device_local_host_visible;

    u32 graphics_queue_index;
    u32 present_queue_index;
    u32 transfer_queue_index;
    u32 compute_queue_index;

    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue transfer_queue;

    VkCommandPool graphics_command_pool;

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory;

    VkFormat depth_format;
} vulkan_device;

typedef struct vulkan_image {
    VkImage handle;
    VkDeviceMemory memory;
    VkImageView view;
    u32 width;
    u32 height;
} vulkan_image;

typedef enum vulkan_renderpass_state {
    READY,
    RECORDING,
    IN_RENDER_PASS,
    RECORDING_ENDED,
    SUBMITTED,
    NOT_ALLOCATED
} vulkan_renderpass_state;

typedef struct vulkan_renderpass {
    VkRenderPass handle;
    vec4 render_area;
    vec4 clear_colour;

    f32 depth;
    u32 stencil;

    u8 clear_flags;
    b8 has_prev_pass;
    b8 has_next_pass;

    vulkan_renderpass_state state;
} vulkan_renderpass;

typedef struct vulkan_swapchain {
    VkSurfaceFormatKHR image_format;
    u8 max_frames_in_flight;
    VkSwapchainKHR handle;
    u32 image_count;
    VkImage *images;
    VkImageView *views;

    vulkan_image depth_attachment;

    // one per frame
    VkFramebuffer framebuffers[3];

    VkSemaphore *render_finished_per_image;
} vulkan_swapchain;

typedef enum vulkan_command_buffer_state {
    COMMAND_BUFFER_STATE_READY,
    COMMAND_BUFFER_STATE_RECORDING,
    COMMAND_BUFFER_STATE_IN_RENDER_PASS,
    COMMAND_BUFFER_STATE_RECORDING_ENDED,
    COMMAND_BUFFER_STATE_SUBMITTED,
    COMMAND_BUFFER_STATE_NOT_ALLOCATED
} vulkan_command_buffer_state;

typedef struct vulkan_command_buffer {
    VkCommandBuffer handle;
    vulkan_command_buffer_state state;
} vulkan_command_buffer;

typedef enum vulkan_shader_scope {
    VULKAN_SHADER_SCOPE_GLOBAL,
    VULKAN_SHADER_SCOPE_INSTANCE,
} vulkan_shader_scope;

typedef struct vulkan_shader_stage {
    VkShaderModuleCreateInfo create_info;
    VkShaderModule handle;
    VkPipelineShaderStageCreateInfo shader_stage_create_info;
} vulkan_shader_stage;

typedef struct vulkan_pipeline {
    VkPipeline handle;
    VkPipelineLayout pipeline_layout;
} vulkan_pipeline;

#define MATERIAL_SHADER_STAGE_COUNT 2

typedef struct vulkan_descriptor_state {
    // One per frame
    u32 generations[3];
    u32 ids[3];
} vulkan_descriptor_state;

#define VULKAN_MATERIAL_SHADER_DESCRIPTOR_COUNT 2
#define VULKAN_MATERIAL_SHADER_SAMPLER_COUNT 1

typedef struct vulkan_material_shader_instance_state {
    // Per frame
    VkDescriptorSet descriptor_sets[3];

    // Per descriptor
    vulkan_descriptor_state
        descriptor_states[VULKAN_MATERIAL_SHADER_DESCRIPTOR_COUNT];
} vulkan_material_shader_instance_state;

// Max number of material instances
#define VULKAN_MAX_MATERIAL_COUNT 1024

// Maximum amount of uploaded geometries
// TODO: Make configurable
#define VULKAN_MAX_GEOMETRY_COUNT 4096

/**
 * @typedef vulkan_geometry_data
 * @brief Internal buffer data for geometry data.
 */
typedef struct vulkan_geometry_data {
    u32 id;
    u32 generation;
    u32 vertex_count;
    u32 vertex_element_size;
    u64 vertex_buffer_offset;
    u32 index_count;
    u32 index_element_size;
    u64 index_buffer_offset;
} vulkan_geometry_data;

// Required to be 256 bytes
typedef struct vulkan_material_shader_global_ubo {
    mat4 projection;  // 64 bytes
    mat4 view;        // 64 bytes
    mat4 m_reserved0; // 64 bytes, reserved for future
    mat4 m_reserved1; // 64 bytes, reserved for future
} vulkan_material_shader_global_ubo;

typedef struct vulkan_material_shader_instance_ubo {
    vec4 diffuse_color; // 16 bytes
    vec4 v_reserved0;   // 16 bytes, reserved for future
    vec4 v_reserved1;   // 16 bytes, reserved for future
    vec4 v_reserved2;   // 16 bytes, reserved for future
    mat4 m_reserved0;   // 64 bytes, reserved for future
    mat4 m_reserved1;   // 64 bytes, reserved for future
    mat4 m_reserved2;   // 64 bytes, reserved for future
} vulkan_material_shader_instance_ubo;

typedef struct vulkan_material_shader {
    // vertex, fragment
    vulkan_shader_stage stages[MATERIAL_SHADER_STAGE_COUNT];

    VkDescriptorPool global_descriptor_pool;
    VkDescriptorSetLayout global_descriptor_set_layout;

    // One descriptor set per frame
    VkDescriptorSet global_descriptor_sets[3];
    b8 descriptor_updated[3];

    // Global uniform object
    vulkan_material_shader_global_ubo global_ubo;

    // Global uniform buffer
    vulkan_buffer global_uniform_buffer;

    VkDescriptorPool object_descriptor_pool;
    VkDescriptorSetLayout object_descriptor_set_layout;

    // One buffer for all objects
    vulkan_buffer object_uniform_buffer;
    // TODO: manage a free list of some kind here
    u32 object_uniform_buffer_index;

    texture_use sampler_uses[VULKAN_MATERIAL_SHADER_SAMPLER_COUNT];

    // TODO: make dynamic
    vulkan_material_shader_instance_state
        instance_states[VULKAN_MAX_MATERIAL_COUNT];

    vulkan_pipeline pipeline;
} vulkan_material_shader;

#define UI_SHADER_STAGE_COUNT 2
#define VULKAN_UI_SHADER_DESCRIPTOR_COUNT 2
#define VULKAN_UI_SHADER_SAMPLER_COUNT 1

// Max number of UI control instances
// TODO: make configurable
#define VULKAN_MAX_UI_COUNT 1024

typedef struct vulkan_ui_shader_instance_state {
    // Per frame
    VkDescriptorSet descriptor_sets[3];

    // Per descriptor
    vulkan_descriptor_state
        descriptor_states[VULKAN_UI_SHADER_DESCRIPTOR_COUNT];
} vulkan_ui_shader_instance_state;

// Required to be 256 bytes
typedef struct vulkan_ui_shader_global_ubo {
    mat4 projection;  // 64 bytes
    mat4 view;        // 64 bytes
    mat4 m_reserved0; // 64 bytes, reserved for future
    mat4 m_reserved1; // 64 bytes, reserved for future
} vulkan_ui_shader_global_ubo;

typedef struct vulkan_ui_shader_instance_ubo {
    vec4 diffuse_color; // 16 bytes
    vec4 v_reserved0;   // 16 bytes, reserved for future
    vec4 v_reserved1;   // 16 bytes, reserved for future
    vec4 v_reserved2;   // 16 bytes, reserved for future
    mat4 m_reserved0;   // 64 bytes, reserved for future
    mat4 m_reserved1;   // 64 bytes, reserved for future
    mat4 m_reserved2;   // 64 bytes, reserved for future
} vulkan_ui_shader_instance_ubo;

typedef struct vulkan_ui_shader {
    // vertex, fragment
    vulkan_shader_stage stages[MATERIAL_SHADER_STAGE_COUNT];

    VkDescriptorPool global_descriptor_pool;
    VkDescriptorSetLayout global_descriptor_set_layout;

    // One descriptor set per frame
    VkDescriptorSet global_descriptor_sets[3];

    // Global uniform object
    vulkan_ui_shader_global_ubo global_ubo;

    // Global uniform buffer
    vulkan_buffer global_uniform_buffer;

    VkDescriptorPool object_descriptor_pool;
    VkDescriptorSetLayout object_descriptor_set_layout;

    // One buffer for all objects
    vulkan_buffer object_uniform_buffer;
    // TODO: manage a free list of some kind here
    u32 object_uniform_buffer_index;

    texture_use sampler_uses[VULKAN_UI_SHADER_SAMPLER_COUNT];

    // TODO: make dynamic
    vulkan_ui_shader_instance_state instance_states[VULKAN_MAX_UI_COUNT];

    vulkan_pipeline pipeline;
} vulkan_ui_shader;

#define VULKAN_SHADER_MAX_STAGES 8
#define VULKAN_SHADER_MAX_GLOBAL_TEXTURES 31
#define VULKAN_SHADER_MAX_INSTANCE_TEXTURES 31
#define VULKAN_SHADER_MAX_ATTRIBUTES 16

#define VULKAN_SHADER_MAX_UNIFORMS 128
#define VULKAN_SHADER_MAX_BINDINGS 32
#define VULKAN_SHADER_MAX_PUSH_CONST_RANGES 32

typedef enum shader_attribute_type {
    SHADER_ATTRIB_TYPE_FLOAT32,
    SHADER_ATTRIB_TYPE_FLOAT32_2,
    SHADER_ATTRIB_TYPE_FLOAT32_3,
    SHADER_ATTRIB_TYPE_FLOAT32_4,
    SHADER_ATTRIB_TYPE_MATRIX4,
    SHADER_ATTRIB_TYPE_INT8,
    SHADER_ATTRIB_TYPE_INT8_2,
    SHADER_ATTRIB_TYPE_INT8_3,
    SHADER_ATTRIB_TYPE_INT8_4,
    SHADER_ATTRIB_TYPE_UINT8,
    SHADER_ATTRIB_TYPE_UINT8_2,
    SHADER_ATTRIB_TYPE_UINT8_3,
    SHADER_ATTRIB_TYPE_UINT8_4,
    SHADER_ATTRIB_TYPE_INT16,
    SHADER_ATTRIB_TYPE_INT16_2,
    SHADER_ATTRIB_TYPE_INT16_3,
    SHADER_ATTRIB_TYPE_INT16_4,
    SHADER_ATTRIB_TYPE_UINT16,
    SHADER_ATTRIB_TYPE_UINT16_2,
    SHADER_ATTRIB_TYPE_UINT16_3,
    SHADER_ATTRIB_TYPE_UINT16_4,
    SHADER_ATTRIB_TYPE_INT32,
    SHADER_ATTRIB_TYPE_INT32_2,
    SHADER_ATTRIB_TYPE_INT32_3,
    SHADER_ATTRIB_TYPE_INT32_4,
    SHADER_ATTRIB_TYPE_UINT32,
    SHADER_ATTRIB_TYPE_UINT32_2,
    SHADER_ATTRIB_TYPE_UINT32_3,
    SHADER_ATTRIB_TYPE_UINT32_4,
} shader_attribute_type;

typedef enum vulkan_shader_state {
    VULKAN_SHADER_STATE_NOT_CREATED,
} vulkan_shader_state;

typedef struct vulkan_shader_stage_config {
    VkShaderStageFlagBits stage;
    char file_name[255];
    char stage_str[255];
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
    u32 attribute_count;
    u32 attribute_stride;
    VkVertexInputAttributeDescription attributes[VULKAN_SHADER_MAX_ATTRIBUTES];

    u32 push_constant_range_count;
    krange push_constant_ranges[VULKAN_SHADER_MAX_PUSH_CONST_RANGES];
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
    vulkan_shader_state state;
    vulkan_context *context;
    char *name;
    vulkan_shader_config config;
    vulkan_renderpass *renderpass;
    vulkan_shader_stage stages[VULKAN_SHADER_MAX_STAGES];
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_set_layouts[2];
    VkDescriptorSet global_descriptor_sets[3];
    u32 global_texture_count;
    texture global_textures[VULKAN_SHADER_MAX_GLOBAL_TEXTURES];
    vulkan_buffer uniform_buffer;
    vulkan_pipeline pipeline;
    u32 bound_instance_id;
    u32 instance_count;
    vulkan_shader_instance_state instance_states[VULKAN_MAX_MATERIAL_COUNT];

    b8 use_instances;
    b8 use_push_constants;
} vulkan_shader;

struct vulkan_context {
    f32 frame_delta_time;

    u32 framebuffer_width;
    u32 framebuffer_height;

    // Current generation of framebuffer size. If it does not match
    // framebuffer_size_last_generation, a new one should be generated.
    u64 framebuffer_size_generation;

    // The generation of the size buffer when it was last created. Set the
    // framebuffer_size_generation when updated.
    u64 framebuffer_size_last_generation;

    VkInstance instance;
    VkAllocationCallbacks *allocator;
    VkSurfaceKHR surface;

#if defined(_DEBUG)
    VkDebugUtilsMessengerEXT debug_messenger;
#endif // defined(_DEBUG)

    vulkan_device device;

    vulkan_swapchain swapchain;
    vulkan_renderpass main_renderpass;
    vulkan_renderpass ui_renderpass;

    vulkan_buffer object_vertex_buffer;
    vulkan_buffer object_index_buffer;

    // darray
    vulkan_command_buffer *graphics_command_buffers;

    // darray
    VkSemaphore *image_available_semaphores;

    // darray
    VkSemaphore *queue_complete_semaphores;

    u32 in_flight_fence_count;
    VkFence in_flight_fences[2];

    // holds pointers to fences which exist and are owned elsewhere, one per
    // frame
    VkFence *images_in_flight[3];

    u32 image_index;
    u32 current_frame;

    b8 recreating_swapchain;

    vulkan_material_shader material_shader;
    vulkan_ui_shader ui_shader;

    // TODO: Make dynamic
    vulkan_geometry_data geometries[VULKAN_MAX_GEOMETRY_COUNT];

    // One per frame
    VkFramebuffer world_framebuffers[3];

    i32 (*find_memory_index)(u32 type_filter, u32 property_flags);
};

typedef struct vulkan_texture_data {
    vulkan_image image;
    VkSampler sampler;
} vulkan_texture_data;
