#pragma once

#include "math/math_types.h"

typedef enum resource_type {
    RESOURCE_TYPE_TEXT,
    RESOURCE_TYPE_BINARY,
    RESOURCE_TYPE_IMAGE,
    RESOURCE_TYPE_MATERIAL,
    RESOURCE_TYPE_MESH,
    RESOURCE_TYPE_STATIC_MESH,
    RESOURCE_TYPE_SHADER,
    RESOURCE_TYPE_CUSTOM
} resource_type;

typedef struct resource {
    u32 loader_id;
    const char *name;
    char *full_path;
    u64 data_size;
    void *data;
} resource;

typedef struct image_resource_data {
    u8 channel_count;
    u32 width;
    u32 height;
    u8 *pixels;
} image_resource_data;

#define TEXTURE_NAME_MAX_LENGTH 512

typedef enum texture_flag {
    TEXTURE_FLAG_HAS_TRANSPARENCY = 0x01,
    TEXTURE_FLAG_IS_WRITEABLE = 0x02,
    // if we have control over the texture or if we are just wrapping it
    TEXTURE_FLAG_IS_WRAPPED = 0x04,
} texture_flag;

typedef u8 texture_flag_bits;

typedef struct texture {
    u32 id;
    u32 width;
    u32 height;
    u8 channel_count;
    texture_flag_bits flags;
    u32 generation;
    char name[TEXTURE_NAME_MAX_LENGTH];
    void *internal_data;
} texture;

typedef enum texture_use {
    TEXTURE_USE_UNKNOWN = 0x00,
    TEXTURE_USE_MAP_DIFFUSE = 0x01,
    TEXTURE_USE_MAP_SPECULAR = 0x02,
    TEXTURE_USE_MAP_NORMAL = 0x02
} texture_use;

typedef enum texture_filter {
    TEXTURE_FILTER_MODE_NEAREST = 0x0,
    TEXTURE_FILTER_MODE_LINEAR = 0x1,
} texture_filter;

typedef enum texture_repeat {
    TEXTURE_REPEAT_REPEAT = 0x1,
    TEXTURE_REPEAT_MIRRORED_REPEAT = 0x2,
    TEXTURE_REPEAT_CLAMPED_TO_EDGE = 0x3,
    TEXTURE_REPEAT_CLAMPED_TO_BORDER = 0x4,
} texture_repeat;

typedef struct texture_map {
    texture *texture;
    texture_use use;
    texture_filter filter_minify;
    texture_filter filter_magnify;
    texture_repeat repeat_u; // x|s
    texture_repeat repeat_v; // y|t
    texture_repeat repeat_w; // z|u
    void *internal_data;
} texture_map;

#define MATERIAL_NAME_MAX_LENGTH 512

typedef struct material_config {
    char name[MATERIAL_NAME_MAX_LENGTH];
    char *shader_name;
    b8 auto_release;
    vec4 diffuse_colour;
    f32 shininess;
    char diffuse_map_name[TEXTURE_NAME_MAX_LENGTH];
    char specular_map_name[TEXTURE_NAME_MAX_LENGTH];
    char normal_map_name[TEXTURE_NAME_MAX_LENGTH];
} material_config;

typedef struct material {
    u32 id;
    u32 generation;
    u32 internal_id;
    char name[MATERIAL_NAME_MAX_LENGTH];
    vec4 diffuse_colour;
    texture_map diffuse_map;
    texture_map specular_map;
    texture_map normal_map;
    f32 shininess;
    u32 shader_id;
    u32 render_frame_number;
} material;

#define GEOMETRY_NAME_MAX_LENGTH 256

typedef struct geometry {
    u32 id;
    u32 internal_id;
    u16 generation;
    vec3 centre;
    extents_3d extents;
    char name[GEOMETRY_NAME_MAX_LENGTH];
    material *material;
} geometry;

typedef struct mesh {
    u16 geometry_count;
    geometry **geometries;
    transform transform;
} mesh;
