#pragma once

#include "containers/hashtable.h"
#include "defines.h"
#include "renderer/renderer_types.inl"

/** @brief Configuration for the shader system */
typedef struct shader_system_config {
    /** @brief The maximum number of shaders held. At least 512. */
    u16 max_shader_count;
    /** @brief The maximum number of uniforms allowed in a shader. */
    u8 max_uniform_count;
    /** @brief The maximum number of global-scope textures in a shader. */
    u8 max_global_textures;
    /** @brief The maximum number of instance-scope textures in a shader. */
    u8 max_instance_textures;
} shader_system_config;

/**
 * @brief Represents the state of a shader.
 */
typedef enum shader_state {
    SHADER_STATE_NOT_CREATED,
    SHADER_STATE_UNINITIALIZED,
    SHADER_STATE_INITIALIZED
} shader_state;

/**
 * @brief Represents the scope of a shader object.
 */
typedef enum shader_scope {
    SHADER_SCOPE_GLOBAL,
    SHADER_SCOPE_INSTANCE,
    SHADER_SCOPE_LOCAL
} shader_scope;

/**
 * @brief Stages available in the system.
 */
typedef enum shader_stage {
    SHADER_STAGE_VERTEX = 0x01,
    SHADER_STAGE_GEOMETRY = 0x02,
    SHADER_STAGE_FRAGMENT = 0x04,
    SHADER_STAGE_COMPUTE = 0x08
} shader_stage;

/**
 * @brief Represents the type of a uniform.
 */
typedef enum shader_uniform_type {
    SHADER_UNIFORM_TYPE_FLOAT32 = 0U,
    SHADER_UNIFORM_TYPE_FLOAT32_2 = 1U,
    SHADER_UNIFORM_TYPE_FLOAT32_3 = 2U,
    SHADER_UNIFORM_TYPE_FLOAT32_4 = 3U,
    SHADER_UNIFORM_TYPE_INT8 = 4U,
    SHADER_UNIFORM_TYPE_UINT8 = 5U,
    SHADER_UNIFORM_TYPE_INT16 = 6U,
    SHADER_UNIFORM_TYPE_UINT16 = 7U,
    SHADER_UNIFORM_TYPE_INT32 = 8U,
    SHADER_UNIFORM_TYPE_UINT32 = 9U,
    SHADER_UNIFORM_TYPE_MATRIX_4 = 10U,
    SHADER_UNIFORM_TYPE_SAMPLER = 11U,
    SHADER_UNIFORM_TYPE_CUSTOM = 255U
} shader_uniform_type;

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
    SHADER_ATTRIB_TYPE_UINT32_4
} shader_attribute_type;

typedef struct shader_uniform_config {
    /** @brief . */
    u8 name_length;
    /** @brief . */
    char *name;
    /** @brief . */
    u8 size;
    /** @brief . */
    u32 location;
    /** @brief . */
    shader_uniform_type type;
    /** @brief . */
    shader_scope scope;
} shader_uniform_config;

typedef struct shader_uniform {
    /** @brief Offset in bytes from the beginning of the uniform */
    u64 offset;
    /** @brief Used as a lookup. */
    u16 location;
    /** @brief Index into the internal uniform array. */
    u16 index;
    /** @brief Size of the uniform. */
    u16 size;
    /** @brief Index of the descriptor set the uniform belongs to. */
    u8 set_index;
    /** @brief Scope of the uniform. */
    shader_scope scope;
    /** @brief Type of the uniform. */
    shader_uniform_type type;
} shader_uniform;

typedef struct shader_attribute_config {
    /** @brief . */
    u8 name_length;
    /** @brief . */
    char *name;
    /** @brief . */
    u8 size;
    /** @brief . */
    shader_attribute_type type;
} shader_attribute_config;

typedef struct shader_attribute {
    /** @brief The attribute name. */
    char *name;
    /** @brief The attribute type. */
    shader_attribute_type type;
    /** @brief The size of the attribute. */
    u32 size;
} shader_attribute;

typedef struct shader_config {
    /** @brief The shader's name. */
    char *name;
    /** @brief Indicates if the shader uses instances. */
    b8 use_instances;
    /** @brief Indicates if locals are used. */
    b8 use_locals;
    /** @brief . */
    u8 attribute_count;
    /** @brief . */
    shader_attribute_config *attributes;
    /** @brief . */
    u8 uniform_count;
    /** @brief . */
    shader_uniform_config *uniforms;
    /** @brief . */
    char *renderpass_name;
    /** @brief . */
    u8 stage_count;
    /** @brief . */
    shader_stage *stages;
    /** @brief . */
    char **stage_names;
    /** @brief . */
    char **stage_filenames;
} shader_config;

typedef struct shader {
    /** @brief The shader identifier. */
    u32 id;
    /** @brief The shader's name. */
    char *name;
    /** @brief Indicates if the shader uses instances. */
    b8 use_instances;
    /** @brief Indicates if locals are used. */
    b8 use_locals;
    /** @brief UBO alignment in bytes. */
    u64 required_ubo_alignment;
    /** @brief The actual size of the global UBO. */
    u64 global_ubo_size;
    /** @brief The stride of the UBO. */
    u64 global_ubo_stride;
    /** @brief Offset from the start of the buffer. */
    u64 global_ubo_offset;
    /** @brief Size of the instance UBO. */
    u64 ubo_size;
    /** @brief Stride of the instance UBO. */
    u64 ubo_stride;
    /** @brief Push constant size. */
    u64 push_constant_size;
    /** @brief Push constant stride. */
    u64 push_constant_stride;
    /** @brief Array of global texture pointers (Darray). */
    texture **global_textures;
    /** @brief Number of instance textures. */
    u8 instance_texture_count;
    /** @brief Bounded scope. */
    shader_scope bound_scope;
    /** @brief Identifier of the currently bounded instance. */
    u32 bound_instance_id;
    /** @brief The currently bound instance's UBO offset. */
    u32 bound_ubo_offset;
    /** @brief Block of memory used by the uniform hastable. */
    void *hashtable_block;
    /** @brief Hashtable to store index/locations. */
    hashtable uniform_lookup;
    /** @brief An array of uniforms (Darray). */
    shader_uniform *uniforms;
    /** @brief An array of attributes (Darray). */
    shader_attribute *attributes;
    /** @brief Internal state of the shader. */
    shader_state state;
    /** @brief Number of push constant ranges. */
    u8 push_constant_range_count;
    /** @brief An array of push constant ranges. */
    krange push_constant_ranges[32];
    /** @brief Size of all the attributes combined. */
    u16 attribute_stride;
    /** @brief Opaque pointer to renderer specific data. */
    void *internal_data;
} shader;

/**
 * @brief Initialises the shader system. Call twice, first time for memory
 * requirement, second to initialise.
 *
 * @param memory_requirement A pointer to store the memory requirement.
 * @param memory A pointer to the block of memory, or 0;
 * @param config Config for the shader system.
 * @return True if successful; otherwise False.
 */
b8 shader_system_initialize(u64 *memory_requirement, void *memory,
                            shader_system_config config);

/**
 * @brief Shutdowns the shader system.
 *
 * @param state A pointer to the system state.
 */
void shader_system_shutdown(void *state);

KAPI b8 shader_system_create(const shader_config *config);

KAPI u32 shader_system_get_id(const char *shader_name);

KAPI shader *shader_system_get_by_id(u32 shader_id);

KAPI shader *shader_system_get(const char *shader_name);

KAPI b8 shader_system_use(const char *shader_name);

KAPI b8 shader_system_use_by_id(u32 shader_id);

KAPI u16 shader_system_uniform_index(shader *s, const char *uniform_name);

KAPI b8 shader_system_uniform_set(const char *uniform_name, const void *value);

KAPI b8 shader_system_sampler_set(const char *uniform_name, const texture *t);

KAPI b8 shader_system_uniform_set_by_index(u16 index, const void *value);

KAPI b8 shader_system_sampler_set_by_index(u16 index, const texture *t);

KAPI b8 shader_system_apply_global();

KAPI b8 shader_system_apply_instance();

KAPI b8 shader_system_bind_instance(u32 instance_id);
