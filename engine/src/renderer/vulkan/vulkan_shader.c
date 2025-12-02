#include "renderer/vulkan/vulkan_shader.h"

#include "renderer/vulkan/vulkan_buffer.h"
#include "renderer/vulkan/vulkan_pipeline.h"
#include "renderer/vulkan/vulkan_types.inl"
#include "renderer/vulkan/vulkan_utils.h"

#include "core/kmemory.h"
#include "core/kstring.h"
#include "core/logger.h"
#include "systems/resource_system.h"
#include "systems/texture_system.h"
#include <vulkan/vulkan_core.h>

const u32 DESC_SET_INDEX_GLOBAL = 0;
const u32 DESC_SET_INDEX_INSTANCE = 1;

const u32 BINDING_INDEX_UBO = 0;
const u32 BINDING_INDEX_SAMPLER = 1;

#define FAIL_DESTROY(shader)                                                   \
    vulkan_shader_destroy(shader);                                             \
    return false

b8 create_module(vulkan_shader *shader, vulkan_shader_stage_config config,
                 vulkan_shader_stage *shader_stage);
b8 uniform_name_valid(vulkan_shader *shader, const char *uniform_name);
b8 shader_uniform_add_state_valid(vulkan_shader *shader);
b8 uniform_add(vulkan_shader *shader, const char *uniform_name, u32 size,
               vulkan_shader_scope scope, u32 *out_location, b8 is_sampler);

b8 vulkan_shader_create(vulkan_context *context, const char *name,
                        vulkan_renderpass *renderpass,
                        VkShaderStageFlags stages, u32 max_descriptor_set_count,
                        b8 use_instances, b8 use_local,
                        vulkan_shader *out_shader) {
    if (!context || !name || !out_shader) {
        KERROR("vulkan_shader_create - requires a valid context, name and "
               "out_shader.");
        return false;
    }

    if (stages == 0) {
        KERROR("vulkan_shader_create - requires stages to be nonzero.");
        return false;
    }

    // Zero out structure
    kzero_memory(out_shader, sizeof(vulkan_shader));
    out_shader->state = VULKAN_SHADER_STATE_NOT_CREATED;
    out_shader->context = context;
    out_shader->name = string_duplicate(name);
    out_shader->use_instances = use_instances;
    out_shader->use_push_constants = use_local;
    out_shader->renderpass = renderpass;
    out_shader->config.attribute_stride = 0;
    out_shader->config.push_constant_range_count = 0;
    out_shader->bound_instance_id = INVALID_ID;
    kzero_memory(out_shader->config.push_constant_ranges,
                 sizeof(krange) * VULKAN_SHADER_MAX_PUSH_CONST_RANGES);
    out_shader->config.max_descriptor_set_count = max_descriptor_set_count;

    // Set shader stages
    kzero_memory(out_shader->config.stages,
                 sizeof(vulkan_shader_stage_config) * VULKAN_SHADER_MAX_STAGES);
    out_shader->config.stage_count = 0;
    for (u32 i = VK_SHADER_STAGE_VERTEX_BIT;
         i < VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM; i++) {
        if ((stages & i) == i) {
            vulkan_shader_stage_config stage_config;
            switch (i) {
            case VK_SHADER_STAGE_VERTEX_BIT:
                string_ncopy(stage_config.stage_str, "vert", 7);
                break;
            case VK_SHADER_STAGE_FRAGMENT_BIT:
                string_ncopy(stage_config.stage_str, "vert", 7);
                break;
            default:
                KERROR("vulkan_shader_create - Unsupported shader stage flag "
                       "'%d', ignoring stage.",
                       i);
                continue;
            }
            stage_config.stage = i;
            if (out_shader->config.stage_count + 1 > VULKAN_SHADER_MAX_STAGES) {
                KERROR("vulkan_shader_create - shaders must have a maximum "
                       "shader count of: '%d'.",
                       VULKAN_SHADER_MAX_STAGES);
                return false;
            }
            out_shader->config.stages[out_shader->config.stage_count] =
                stage_config;
            out_shader->config.stage_count++;
        }
    }

    kzero_memory(out_shader->config.descriptor_sets,
                 sizeof(vulkan_descriptor_set_config) * 2);
    kzero_memory(out_shader->global_textures,
                 sizeof(texture *) * VULKAN_SHADER_MAX_GLOBAL_TEXTURES);
    out_shader->global_texture_count = 0;
    kzero_memory(out_shader->config.attributes,
                 sizeof(VkVertexInputAttributeDescription) *
                     VULKAN_SHADER_MAX_ATTRIBUTES);
    out_shader->config.attribute_count = 0;
    kzero_memory(out_shader->uniforms, sizeof(vulkan_uniform_lookup_entry) * VULKAN_SHADER_MAX_UNIFORMS);
}

b8 vulkan_shader_destroy(vulkan_shader *shader);

b8 vulkan_shader_add_attribute(vulkan_shader *shader, const char *name,
                               shader_attribute_type type);

b8 vulkan_shader_add_sampler(vulkan_shader *shader, const char *sampler_name,
                             vulkan_shader_scope scope, u32 *out_location);

b8 vulkan_shader_add_uniform_i8(vulkan_shader *shader, const char *uniform_name,
                                vulkan_shader_scope scope, u32 *out_location);

b8 vulkan_shader_add_uniform_i16(vulkan_shader *shader,
                                 const char *uniform_name,
                                 vulkan_shader_scope scope, u32 *out_location);

b8 vulkan_shader_add_uniform_i32(vulkan_shader *shader,
                                 const char *uniform_name,
                                 vulkan_shader_scope scope, u32 *out_location);

b8 vulkan_shader_add_uniform_u8(vulkan_shader *shader, const char *uniform_name,
                                vulkan_shader_scope scope, u32 *out_location);

b8 vulkan_shader_add_uniform_u16(vulkan_shader *shader,
                                 const char *uniform_name,
                                 vulkan_shader_scope scope, u32 *out_location);

b8 vulkan_shader_add_uniform_u32(vulkan_shader *shader,
                                 const char *uniform_name,
                                 vulkan_shader_scope scope, u32 *out_location);

b8 vulkan_shader_add_uniform_f32(vulkan_shader *shader,
                                 const char *uniform_name,
                                 vulkan_shader_scope scope, u32 *out_location);

b8 vulkan_shader_add_uniform_vec2(vulkan_shader *shader,
                                  const char *uniform_name,
                                  vulkan_shader_scope scope, u32 *out_location);

b8 vulkan_shader_add_uniform_vec3(vulkan_shader *shader,
                                  const char *uniform_name,
                                  vulkan_shader_scope scope, u32 *out_location);

b8 vulkan_shader_add_uniform_vec4(vulkan_shader *shader,
                                  const char *uniform_name,
                                  vulkan_shader_scope scope, u32 *out_location);

b8 vulkan_shader_add_uniform_mat4(vulkan_shader *shader,
                                  const char *uniform_name,
                                  vulkan_shader_scope scope, u32 *out_location);

b8 vulkan_shader_initialize(vulkan_shader *shader);

b8 vulkan_shader_use(vulkan_shader *shader);

b8 vulkan_bind_globals(vulkan_shader *shader);

b8 vulkan_bind_instance(vulkan_shader *shader, u32 instance_id);

b8 vulkan_apply_globals(vulkan_shader *shader);

b8 vulkan_apply_instance(vulkan_shader *shader);

b8 vulkan_shader_acquire_instance_resources(vulkan_shader *shader,
                                            u32 *out_instance_id);

b8 vulkan_shader_release_instance_resources(vulkan_shader *shader,
                                            u32 instance_id);

u32 vulkan_shader_uniform_location(vulkan_shader *shader, const char *name);

b8 vulkan_shader_set_sampler(vulkan_shader *shader, u32 location, texture *t);

b8 vulkan_shader_set_uniform_i8(vulkan_shader *shader, u32 location, i8 value);

b8 vulkan_shader_set_uniform_i16(vulkan_shader *shader, u32 location,
                                 i16 value);

b8 vulkan_shader_set_uniform_i32(vulkan_shader *shader, u32 location,
                                 i32 value);

b8 vulkan_shader_set_uniform_u8(vulkan_shader *shader, u32 location, u8 value);

b8 vulkan_shader_set_uniform_u16(vulkan_shader *shader, u32 location,
                                 u16 value);

b8 vulkan_shader_set_uniform_u32(vulkan_shader *shader, u32 location,
                                 u32 value);

b8 vulkan_shader_set_uniform_f32(vulkan_shader *shader, u32 location,
                                 f32 value);

b8 vulkan_shader_set_uniform_vec2(vulkan_shader *shader, u32 location,
                                  vec2 value);

b8 vulkan_shader_set_uniform_vec2f(vulkan_shader *shader, u32 location,
                                   f32 value_0, f32 value_1);

b8 vulkan_shader_set_uniform_vec3(vulkan_shader *shader, u32 location,
                                  vec3 value);

b8 vulkan_shader_set_uniform_vec3f(vulkan_shader *shader, u32 location,
                                   f32 value_0, f32 value_1, f32 value_2);

b8 vulkan_shader_set_uniform_vec4(vulkan_shader *shader, u32 location,
                                  vec4 value);

b8 vulkan_shader_set_uniform_vec4f(vulkan_shader *shader, u32 location,
                                   f32 value_0, f32 value_1, f32 value_2,
                                   f32 value_3);

b8 vulkan_shader_set_uniform_mat4(vulkan_shader *shader, u32 location,
                                  mat4 value);
