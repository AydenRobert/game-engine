#pragma once

#include "renderer/renderer_types.inl"

typedef struct geometry_system_config {
    // NOTE: Should be significantly greater than number of static meshes.
    u32 max_geometry_count;
} geometry_system_config;

typedef struct geometry_config {
    u32 vertex_size;
    u32 vertex_count;
    void *vertices;
    u32 index_size;
    u32 index_count;
    void *indices;
    char name[GEOMETRY_NAME_MAX_LENGTH];
    char material_name[MATERIAL_NAME_MAX_LENGTH];
} geometry_config;

#define DEFAULT_GEOMETRY_NAME "default"

b8 geometry_system_initialize(u64 *memory_requirement, void *state,
                              geometry_system_config config);
void geometry_system_shutdown(void *state);

geometry *geometry_system_acquire_by_id(u32 id);
geometry *geometry_system_acquire_from_config(geometry_config config,
                                              b8 auto_release);
void geometry_system_release(geometry *geometry);

geometry *geometry_system_get_default_geometry_3d();
geometry *geometry_system_get_default_geometry_2d();

/**
 * @brief Creates a configuration for plane geometries given the input
 * parameters.
 * NOTE: vertex and index array are dynamically allocated and should be freed by
 * the caller, should not be used in production.
 *
 * @param width Overall width, must be non-zero.
 * @param height Overall height, must be non-zero.
 * @param x_segment_count Number of segments on the x-axis, must be non-zero.
 * @param y_segment_count Number of segments on the y-axis, must be non-zero.
 * @param tile_x Amount texture should tile across x-axis, must be non-zero.
 * @param tile_y Amount texture should tile across y-axis, must be non-zero.
 * @param name The name of the generated geometry.
 * @param material_name The name of the material used.
 * @return A geometry configuration, can be fed into
 * geometry_system_acquire_from_config().
 */
geometry_config geometry_system_generate_plane_config(
    f32 width, f32 height, u32 x_segment_count, u32 y_segment_count, f32 tile_x,
    f32 tile_y, const char *name, const char *material_name);
