#include "systems/geometry_system.h"

#include "core/kmemory.h"
#include "core/kstring.h"
#include "core/logger.h"

#include "defines.h"
#include "systems/material_system.h"

#include "renderer/renderer_frontend.h"

typedef struct geometry_reference {
    u64 reference_count;
    geometry geometry;
    b8 auto_release;
} geometry_reference;

typedef struct geometry_system_state {
    geometry_system_config config;

    geometry default_3d_geometry;
    geometry default_2d_geometry;

    // Array of geometries.
    geometry_reference *registered_geometries;
} geometry_system_state;

static geometry_system_state *state_ptr = 0;

b8 create_default_geometries();
b8 create_geometry(geometry_config config, geometry *geo);
void destroy_geometry(geometry *geo);

b8 geometry_system_initialize(u64 *memory_requirement, void *state,
                              geometry_system_config config) {
    if (config.max_geometry_count == 0) {
        KFATAL(
            "geometry_system_initialize - config.max_geometry_count must be > "
            "0.");
        return false;
    }

    u64 struct_requirement = sizeof(geometry_system_state);
    u64 array_requirement =
        sizeof(geometry_reference) * config.max_geometry_count;
    *memory_requirement = struct_requirement + array_requirement;

    if (!state) {
        return true;
    }

    state_ptr = state;
    state_ptr->config = config;

    // The array block is after the state. Already allocated, so just set the
    // pointer.
    void *array_block = state + struct_requirement;
    state_ptr->registered_geometries = array_block;

    // Invalidate all geometries
    u32 count = state_ptr->config.max_geometry_count;
    for (u32 i = 0; i < count; i++) {
        state_ptr->registered_geometries[i].geometry.id = INVALID_ID;
        state_ptr->registered_geometries[i].geometry.internal_id = INVALID_ID;
        state_ptr->registered_geometries[i].geometry.generation = INVALID_ID;
    }

    if (!create_default_geometries()) {
        KFATAL("Failed to create default geometries. Application cannot "
               "continue.");
        return false;
    }

    return true;
}

void geometry_system_shutdown(void *state) {
    // Note nothing to do here
}

geometry *geometry_system_acquire_by_id(u32 id) {
    if (id == INVALID_ID ||
        state_ptr->registered_geometries[id].geometry.id == INVALID_ID) {
        KERROR("geometry_system_acquire_by_id cannot load invalid geometry id "
               ": '%d'. Returning nullptr.",
               id);
        return 0;
    }

    state_ptr->registered_geometries[id].reference_count++;
    return &state_ptr->registered_geometries[id].geometry;
}

geometry *geometry_system_acquire_from_config(geometry_config config,
                                              b8 auto_release) {
    geometry *geo = 0;
    for (u32 i = 0; i < state_ptr->config.max_geometry_count; i++) {
        if (state_ptr->registered_geometries[i].geometry.id == INVALID_ID) {
            state_ptr->registered_geometries[i].reference_count = 1;
            state_ptr->registered_geometries[i].auto_release = auto_release;
            geo = &state_ptr->registered_geometries[i].geometry;
            geo->id = i;
            break;
        }
    }

    if (!geo) {
        KERROR("Unable to obtain free slot for geometry, adjust config. "
               "Retuning nullptr.");
        return 0;
    }

    if (!create_geometry(config, geo)) {
        KERROR("Failed to create geometry, returning nullptr.");
        return 0;
    }

    return geo;
}

void geometry_system_release(geometry *geometry) {
    if (!geometry) {
        KWARN("geometry_system_release - nullptr was passed.");
        return;
    }

    if (geometry->id == INVALID_ID) {
        KWARN(
            "geometry_system_release - cannot load geometry with invalid id.");
        return;
    }

    u32 id = geometry->id;
    geometry_reference *ref = &state_ptr->registered_geometries[id];
    if (ref->geometry.id == id) {
        if (ref->reference_count > 0) {
            ref->reference_count--;
        }

        if (ref->reference_count < 1 && ref->auto_release) {
            destroy_geometry(&ref->geometry);
            ref->reference_count = 0;
            ref->auto_release = false;
        }
    } else {
        KFATAL("Geometry id mismatch, this should never happen.");
    }
}

geometry *geometry_system_get_default_geometry_3d() {
    if (!state_ptr) {
        KERROR("geometry_system_get_default_geometry_3d - called before system "
               "initialization. Returning nullptr.");
        return 0;
    }

    return &state_ptr->default_3d_geometry;
}

geometry *geometry_system_get_default_geometry_2d() {
    if (!state_ptr) {
        KERROR("geometry_system_get_default_geometry_2d - called before system "
               "initialization. Returning nullptr.");
        return 0;
    }

    return &state_ptr->default_2d_geometry;
}

b8 create_geometry(geometry_config config, geometry *geo) {
    if (!renderer_create_geometry(geo, config.vertex_size, config.vertex_count,
                                  config.vertices, config.index_size,
                                  config.index_count, config.indices)) {
        state_ptr->registered_geometries[geo->id].reference_count = 0;
        state_ptr->registered_geometries[geo->id].auto_release = false;
        geo->id = INVALID_ID;
        geo->generation = INVALID_ID;
        geo->internal_id = INVALID_ID;

        return false;
    }

    if (string_length(config.material_name)) {
        geo->material = material_system_acquire(config.material_name);
        if (!geo->material) {
            geo->material = material_system_get_default();
        }
    }

    return true;
}

void destroy_geometry(geometry *geo) {
    renderer_destroy_geometry(geo);
    geo->id = INVALID_ID;
    geo->generation = INVALID_ID;
    geo->internal_id = INVALID_ID;

    string_empty(geo->name);

    // Release material
    if (geo->material && string_length(geo->material->name) > 0) {
        material_system_release(geo->material->name);
        geo->material = 0;
    }
}

b8 create_default_geometries() {
    // Default 3d geometry
    vertex_3d verts3d[4];
    kzero_memory(verts3d, sizeof(vertex_3d) * 4);

    const f32 f = 10.0f;

    verts3d[0].position.x = -0.5 * f;
    verts3d[0].position.y = -0.5 * f;
    verts3d[0].texcoord.x = 0.0f;
    verts3d[0].texcoord.y = 0.0f;

    verts3d[1].position.x = 0.5 * f;
    verts3d[1].position.y = 0.5 * f;
    verts3d[1].texcoord.x = 1.0f;
    verts3d[1].texcoord.y = 1.0f;

    verts3d[2].position.x = -0.5 * f;
    verts3d[2].position.y = 0.5 * f;
    verts3d[2].texcoord.x = 0.0f;
    verts3d[2].texcoord.y = 1.0f;

    verts3d[3].position.x = 0.5 * f;
    verts3d[3].position.y = -0.5 * f;
    verts3d[3].texcoord.x = 1.0f;
    verts3d[3].texcoord.y = 0.0f;

    u32 indices3d[6] = {0, 1, 2, 0, 3, 1};

    if (!renderer_create_geometry(&state_ptr->default_3d_geometry,
                                  sizeof(vertex_3d), 4, verts3d, sizeof(u32), 6,
                                  indices3d)) {
        KFATAL(
            "Failed to create default geometry. Application cannot continue.");
        return false;
    }

    state_ptr->default_3d_geometry.material = material_system_get_default();

    // Default 2d geometry
    vertex_2d verts2d[4];
    kzero_memory(verts2d, sizeof(vertex_2d) * 4);

    verts2d[0].position.x = -0.5 * f;
    verts2d[0].position.y = -0.5 * f;
    verts2d[0].texcoord.x = 0.0f;
    verts2d[0].texcoord.y = 0.0f;

    verts2d[1].position.x = 0.5 * f;
    verts2d[1].position.y = 0.5 * f;
    verts2d[1].texcoord.x = 1.0f;
    verts2d[1].texcoord.y = 1.0f;

    verts2d[2].position.x = -0.5 * f;
    verts2d[2].position.y = 0.5 * f;
    verts2d[2].texcoord.x = 0.0f;
    verts2d[2].texcoord.y = 1.0f;

    verts2d[3].position.x = 0.5 * f;
    verts2d[3].position.y = -0.5 * f;
    verts2d[3].texcoord.x = 1.0f;
    verts2d[3].texcoord.y = 0.0f;

    // NOTE: counter_clockwise
    u32 indices2d[6] = {2, 1, 0, 3, 0, 1};

    if (!renderer_create_geometry(&state_ptr->default_2d_geometry,
                                  sizeof(vertex_2d), 4, verts2d, sizeof(u32), 6,
                                  indices2d)) {
        KFATAL(
            "Failed to create default geometry. Application cannot continue.");
        return false;
    }

    state_ptr->default_2d_geometry.material = material_system_get_default();

    return true;
}

geometry_config geometry_system_generate_plane_config(
    f32 width, f32 height, u32 x_segment_count, u32 y_segment_count, f32 tile_x,
    f32 tile_y, const char *name, const char *material_name) {

    if (width == 0) {
        KWARN("Width must be non-zero. Defaulting to one.");
        width = 1.0f;
    }
    if (height == 0) {
        KWARN("Height must be non-zero. Defaulting to one.");
        height = 1.0f;
    }
    if (x_segment_count < 1) {
        KWARN("x_segment_count must be non-zero. Defaulting to one.");
        x_segment_count = 1;
    }
    if (y_segment_count < 1) {
        KWARN("y_segment_count must be non-zero. Defaulting to one.");
        y_segment_count = 1;
    }
    if (tile_x == 0) {
        KWARN("tile_x must be non-zero. Defaulting to one.");
        tile_x = 1.0f;
    }
    if (tile_y == 0) {
        KWARN("tile_y must be non-zero. Defaulting to one.");
        tile_y = 1.0f;
    }

    geometry_config config;
    config.vertex_size = sizeof(vertex_3d);
    config.vertex_count = x_segment_count * y_segment_count * 4;
    config.vertices =
        kallocate(sizeof(vertex_3d) * config.vertex_count, MEMORY_TAG_ARRAY);
    config.index_size = sizeof(u32);
    config.index_count = x_segment_count * y_segment_count * 6;
    config.indices =
        kallocate(sizeof(u32) * config.index_count, MEMORY_TAG_ARRAY);

    // TODO: this generates extra vertices, we can de-duplicate later
    f32 seg_width = width / x_segment_count;
    f32 seg_height = height / x_segment_count;
    f32 half_width = width * 0.5f;
    f32 half_height = height * 0.5f;
    for (u32 y = 0; y < y_segment_count; y++) {
        for (u32 x = 0; x < x_segment_count; x++) {
            // Generate vertices
            f32 min_x = (x * seg_width) - half_width;
            f32 min_y = (y * seg_height) - half_height;
            f32 max_x = min_x + seg_width;
            f32 max_y = min_y + seg_height;
            f32 min_uvx = (x / (f32)x_segment_count) * tile_x;
            f32 min_uvy = (y / (f32)y_segment_count) * tile_y;
            f32 max_uvx = ((x + 1) / (f32)x_segment_count) * tile_x;
            f32 max_uvy = ((y + 1) / (f32)y_segment_count) * tile_y;

            u32 v_offset = ((y * x_segment_count) + x) * 4;
            vertex_3d *v0 = &((vertex_3d *)config.vertices)[v_offset + 0];
            vertex_3d *v1 = &((vertex_3d *)config.vertices)[v_offset + 1];
            vertex_3d *v2 = &((vertex_3d *)config.vertices)[v_offset + 2];
            vertex_3d *v3 = &((vertex_3d *)config.vertices)[v_offset + 3];

            v0->position.x = min_x;
            v0->position.y = min_y;
            v0->texcoord.x = min_uvx;
            v0->texcoord.y = min_uvy;

            v1->position.x = max_x;
            v1->position.y = max_y;
            v1->texcoord.x = max_uvx;
            v1->texcoord.y = max_uvy;

            v2->position.x = min_x;
            v2->position.y = max_y;
            v2->texcoord.x = min_uvx;
            v2->texcoord.y = max_uvy;

            v3->position.x = max_x;
            v3->position.y = min_y;
            v3->texcoord.x = max_uvx;
            v3->texcoord.y = min_uvy;

            // Generate indices
            u32 i_offset = ((y * x_segment_count) + x) * 6;
            ((u32 *)config.indices)[i_offset + 0] = v_offset + 0;
            ((u32 *)config.indices)[i_offset + 1] = v_offset + 1;
            ((u32 *)config.indices)[i_offset + 2] = v_offset + 2;
            ((u32 *)config.indices)[i_offset + 3] = v_offset + 0;
            ((u32 *)config.indices)[i_offset + 4] = v_offset + 3;
            ((u32 *)config.indices)[i_offset + 5] = v_offset + 1;
        }
    }

    if (name && string_length(name)) {
        string_ncopy(config.name, name, GEOMETRY_NAME_MAX_LENGTH);
    } else {
        string_ncopy(config.name, DEFAULT_GEOMETRY_NAME,
                     GEOMETRY_NAME_MAX_LENGTH);
    }

    if (material_name && string_length(material_name)) {
        string_ncopy(config.material_name, material_name,
                     MATERIAL_NAME_MAX_LENGTH);
    } else {
        string_ncopy(config.material_name, DEFAULT_MATERIAL_NAME,
                     MATERIAL_NAME_MAX_LENGTH);
    }

    return config;
}
