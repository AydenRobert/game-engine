#pragma once

#include "renderer/renderer_types.inl"

#include "resources/resource_types.h"

typedef struct material_system_config {
    u32 max_material_count;
} material_system_config;

#define DEFAULT_MATERIAL_NAME "default"

b8 material_system_initialize(u64 *memory_requirement, void *state,
                              material_system_config config);
void material_system_shutdown(void *state);

material *material_system_acquire(const char *name);
material *material_system_acquire_from_config(material_config config);
void material_system_release(const char *name);

material *material_system_get_default();

b8 material_system_apply_global(u32 shader_id, const mat4 *projection,
                                const mat4 *view);
b8 material_system_apply_instance(material *m);
b8 material_system_apply_local(material *m, const mat4 *model);
