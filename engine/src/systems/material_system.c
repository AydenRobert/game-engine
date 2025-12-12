#include "systems/material_system.h"

#include "core/kmemory.h"
#include "defines.h"
#include "resources/resource_types.h"
#include "systems/resource_system.h"
#include "systems/shader_system.h"
#include "systems/texture_system.h"

#include "core/kstring.h"
#include "core/logger.h"
#include "math/kmath.h"

#include "containers/hashtable.h"

#include "renderer/renderer_frontend.h"

typedef struct material_shader_uniform_locations {
    u16 projection;
    u16 view;
    u16 diffuse_colour;
    u16 diffuse_texture;
    u16 model;
} material_shader_uniform_locations;

typedef struct ui_shader_uniform_locations {
    u16 projection;
    u16 view;
    u16 diffuse_colour;
    u16 diffuse_texture;
    u16 model;
} ui_shader_uniform_locations;

typedef struct material_system_state {
    material_system_config config;

    material default_material;

    // Array of registered materials
    material *registered_materials;

    // Hashtable for material lookups
    hashtable registered_material_table;

    material_shader_uniform_locations material_locations;
    u32 material_shader_id;

    ui_shader_uniform_locations ui_locations;
    u32 ui_shader_id;
} material_system_state;

typedef struct material_reference {
    u64 reference_count;
    u32 handle;
    b8 auto_release;
} material_reference;

static material_system_state *state_ptr = 0;

b8 create_default_material();
b8 load_material(material_config config, material *material);
void destroy_material(material *material);

b8 material_system_initialize(u64 *memory_requirement, void *state,
                              material_system_config config) {
    if (config.max_material_count == 0) {
        KFATAL(
            "material_system_initialize - config.max_material_count must be > "
            "0.");
        return false;
    }

    // Block of memory will contain state structure, material array and
    // hashtable
    u64 struct_requirement = sizeof(material_system_state);
    u64 array_requirement = sizeof(material) * config.max_material_count;
    u64 hashtable_requirement =
        sizeof(material_reference) * config.max_material_count;
    *memory_requirement =
        struct_requirement + array_requirement + hashtable_requirement;

    if (!state) {
        return true;
    }

    state_ptr = state;
    state_ptr->config = config;

    // The array block is after the state. Already allocated, so just set the
    // pointer.
    void *array_block = state + struct_requirement;
    state_ptr->registered_materials = array_block;

    // Hashtable block is after array block
    void *hashtable_block = array_block + array_requirement;
    hashtable_create(sizeof(material_reference), config.max_material_count,
                     hashtable_block, false,
                     &state_ptr->registered_material_table);

    // Fill the hash table with invalid references to use as a default
    material_reference invalid_ref;
    invalid_ref.auto_release = false;
    invalid_ref.handle = INVALID_ID;
    invalid_ref.reference_count = 0;
    hashtable_fill(&state_ptr->registered_material_table, &invalid_ref);

    // Invalidate all materials in material array
    u32 count = state_ptr->config.max_material_count;
    for (u32 i = 0; i < count; i++) {
        state_ptr->registered_materials[i].id = INVALID_ID;
        state_ptr->registered_materials[i].generation = INVALID_ID;
        state_ptr->registered_materials[i].internal_id = INVALID_ID;
    }

    // Create default material
    if (!create_default_material()) {
        KFATAL(
            "Failed to create default material. Application cannot continue.");
        return false;
    }

    return true;
}

void material_system_shutdown(void *state) {
    if (!state_ptr) {
        return;
    }

    // Destroy all loaded materials
    for (u32 i = 0; i < state_ptr->config.max_material_count; i++) {
        material *t = &state_ptr->registered_materials[i];
        if (t->generation == INVALID_ID) {
            continue;
        }
        destroy_material(t);
    }
    destroy_material(&state_ptr->default_material);
    state_ptr = 0;
}

material *material_system_acquire(const char *name) {
    // Load the material configuration from resource
    resource material_resource;
    if (!resource_system_load(name, RESOURCE_TYPE_MATERIAL,
                              &material_resource)) {
        KERROR("Failed to load material resource for '%s'. Retuning nullptr.",
               name);
        return 0;
    }

    material *mat = 0;
    if (material_resource.data) {
        mat = material_system_acquire_from_config(
            *(material_config *)material_resource.data);
    }

    resource_system_unload(&material_resource);

    if (!mat) {
        KERROR("Failed to load material resource for '%s'. Retuning nullptr.",
               name);
    }

    return mat;
}

material *material_system_acquire_from_config(material_config config) {
    if (!state_ptr) {
        KERROR(
            "material_system_acquire failed to acquire material '%s'. System "
            "should be initialized before using this function.",
            config.name);
        return 0;
    }

    // Return default material
    if (strings_equali(config.name, DEFAULT_MATERIAL_NAME)) {
        return &state_ptr->default_material;
    }

    material_reference material_reference;

    if (!hashtable_get(&state_ptr->registered_material_table, config.name,
                       &material_reference)) {
        KERROR("material_system_acquire failed to acquire material '%s'. Null "
               "pointer will be returned.",
               config.name);
        return 0;
    }

    if (material_reference.reference_count == 0) {
        material_reference.auto_release = config.auto_release;
    }
    material_reference.reference_count++;

    if (material_reference.handle == INVALID_ID) {
        // Find a free index
        u32 count = state_ptr->config.max_material_count;
        material *material = 0;
        for (u32 i = 0; i < count; i++) {
            if (state_ptr->registered_materials[i].id != INVALID_ID) {
                continue;
            }
            material_reference.handle = i;
            material = &state_ptr->registered_materials[i];
            break;
        }

        if (!material || material_reference.handle == INVALID_ID) {
            KFATAL("material_system_acquire - Texture system cannot hold "
                   "anymore materials. Adjust configuration to allow more.");
            return 0;
        }

        if (!load_material(config, material)) {
            KERROR("Failed to load material '%s'.", config.name);
            return 0;
        }

        // Get uniform indecies
        shader *s = shader_system_get_id(material->shader_id);
        // Save the locations
        if (state_ptr->material_shader_id == INVALID_ID &&
            strings_equal(config.shader_name, BUILTIN_SHADER_NAME_MATERIAL)) {
            state_ptr->material_shader_id = s->id;
            state_ptr->material_locations.projection =
                shader_system_uniform_index(s, "projection");
            state_ptr->material_locations.view =
                shader_system_uniform_index(s, "view");
            state_ptr->material_locations.diffuse_colour =
                shader_system_uniform_index(s, "diffuse_colour");
            state_ptr->material_locations.diffuse_texture =
                shader_system_uniform_index(s, "diffuse_texture");
            state_ptr->material_locations.model =
                shader_system_uniform_index(s, "model");
        } else if (state_ptr->ui_shader_id == INVALID_ID &&
                   strings_equal(config.shader_name, BUILTIN_SHADER_NAME_UI)) {
            state_ptr->ui_shader_id = s->id;
            state_ptr->ui_locations.projection =
                shader_system_uniform_index(s, "projection");
            state_ptr->ui_locations.view =
                shader_system_uniform_index(s, "view");
            state_ptr->ui_locations.diffuse_colour =
                shader_system_uniform_index(s, "diffuse_colour");
            state_ptr->ui_locations.diffuse_texture =
                shader_system_uniform_index(s, "diffuse_texture");
            state_ptr->ui_locations.model =
                shader_system_uniform_index(s, "model");
        }

        if (material->generation == INVALID_ID) {
            material->generation = 0;
        } else {
            material->generation++;
        }

        material->id = material_reference.handle;
    } else {
        KTRACE("Material '%s' already exists, ref count has been increased to "
               "'%i'.",
               config.name, material_reference.reference_count);
    }

    // Update the entry
    hashtable_set(&state_ptr->registered_material_table, config.name,
                  &material_reference);
    return &state_ptr->registered_materials[material_reference.handle];
}

void material_system_release(const char *name) {
    if (strings_equali(name, DEFAULT_MATERIAL_NAME)) {
        KWARN("material_system_release called for default material.");
        return;
    }

    if (!state_ptr) {
        KERROR(
            "material_system_release failed to acquire material '%s'. System "
            "should be initialized when using this function.",
            name);
        return;
    }

    // This should hopefully never get called...
    material_reference ref;
    if (!hashtable_get(&state_ptr->registered_material_table, name, &ref)) {
        KERROR("material_system_release failed to release material '%s'.",
               name);
        return;
    }

    if (ref.reference_count == 0) {
        KWARN("material_system_release tried to release non-existant material "
              "'%s'.",
              name);
        return;
    }

    ref.reference_count--;

    if (ref.reference_count == 0 && ref.auto_release) {
        material *material = &state_ptr->registered_materials[ref.handle];

        destroy_material(material);

        // Reset the reference
        ref.handle = INVALID_ID;
        ref.auto_release = false;
        KTRACE("Released material '%s'. Texture is now unloaded as "
               "reference_count = 0 and auto_release = true.",
               name);
    } else {
        KTRACE(
            "Released material '%s'. reference_count = %i, auto_release = %s.",
            name, ref.reference_count, ref.auto_release ? "true" : "false");
    }

    // Update the entry
    hashtable_set(&state_ptr->registered_material_table, name, &ref);
}

material *material_system_get_default() {
    if (!state_ptr) {
        KFATAL("material_system_get_default - called before material system "
               "initialised.");
        return 0;
    }

    return &state_ptr->default_material;
}

#define MATERIAL_APPLY_OR_FAIL(expr)                                           \
    if (!expr) {                                                               \
        KERROR("Failed to apply material: '%s'.", expr);                       \
        return false;                                                          \
    }

b8 material_system_apply_global(u32 shader_id, const mat4 *projection,
                                const mat4 *view) {
    if (shader_id == state_ptr->material_shader_id) {
        MATERIAL_APPLY_OR_FAIL(shader_system_uniform_set_by_index(
            state_ptr->material_locations.projection, projection));
        MATERIAL_APPLY_OR_FAIL(shader_system_uniform_set_by_index(
            state_ptr->material_locations.view, view));
    } else if (shader_id == state_ptr->ui_shader_id) {
        MATERIAL_APPLY_OR_FAIL(shader_system_uniform_set_by_index(
            state_ptr->ui_locations.projection, projection));
        MATERIAL_APPLY_OR_FAIL(shader_system_uniform_set_by_index(
            state_ptr->ui_locations.view, view));
    } else {
        KERROR("material_system_apply_global - unrecognised shader id: '%d'.",
               shader_id);
        return false;
    }
    MATERIAL_APPLY_OR_FAIL(shader_system_apply_global());
    return true;
}

b8 material_system_apply_instance(material *m) {
    if (m->shader_id == state_ptr->material_shader_id) {
        MATERIAL_APPLY_OR_FAIL(shader_system_uniform_set_by_index(
            state_ptr->material_locations.diffuse_colour, &m->diffuse_colour));
        MATERIAL_APPLY_OR_FAIL(shader_system_uniform_set_by_index(
            state_ptr->material_locations.diffuse_texture,
            m->diffuse_map.texture));
    } else if (m->shader_id == state_ptr->ui_shader_id) {
        MATERIAL_APPLY_OR_FAIL(shader_system_uniform_set_by_index(
            state_ptr->ui_locations.diffuse_colour, &m->diffuse_colour));
        MATERIAL_APPLY_OR_FAIL(shader_system_uniform_set_by_index(
            state_ptr->ui_locations.diffuse_texture, m->diffuse_map.texture));
    } else {
        KERROR("material_system_apply_instance - unrecognised shader id: '%d'.",
               m->shader_id);
        return false;
    }
    MATERIAL_APPLY_OR_FAIL(shader_system_apply_instance());
    return true;
}

b8 material_system_apply_local(material *m, const mat4 *model) {
    if (m->shader_id == state_ptr->material_shader_id) {
        return shader_system_uniform_set_by_index(
            state_ptr->material_locations.model, model);
    } else if (m->shader_id == state_ptr->ui_shader_id) {
        return shader_system_uniform_set_by_index(state_ptr->ui_locations.model,
                                                  model);
    }
    KERROR("material_system_apply_local - unrecognised shader id: '%d'.",
           m->shader_id);
    return false;
}

b8 load_material(material_config config, material *mat) {
    kzero_memory(mat, sizeof(material));

    string_ncopy(mat->name, config.name, MATERIAL_NAME_MAX_LENGTH);

    mat->shader_id = shader_system_get_id(config.shader_name);

    mat->diffuse_colour = config.diffuse_colour;

    // Diffuse map
    if (string_length(config.diffuse_map_name) > 0) {
        mat->diffuse_map.use = TEXTURE_USE_MAP_DIFFUSE;
        mat->diffuse_map.texture =
            texture_system_acquire(config.diffuse_map_name, true);
        if (!mat->diffuse_map.texture) {
            KWARN(
                "Unable to load texture '%s' for material '%s', using default",
                config.diffuse_map_name, mat->name);
            mat->diffuse_map.texture = texture_system_get_default_texture();
        }
    } else {
        mat->diffuse_map.use = TEXTURE_USE_UNKNOWN;
        mat->diffuse_map.texture = 0;
    }

    // TODO: other maps

    // acquire resources
    shader *s = shader_system_get(config.shader_name);
    if (!s) {
        KERROR("load_material - Unable to load material because its shader was "
               "not found: '%s'. This is likely a problem with the material "
               "asset.",
               config.shader_name);
        return false;
    }
    if (!renderer_shader_acquire_instance_resources(s, &mat->internal_id)) {
        KERROR("load_material - failed to acquire renderer resources for "
               "material '%s'.",
               mat->name);
        return false;
    }

    return true;
}

void destroy_material(material *mat) {
    KTRACE("Destroying material '%s'...", mat->name);

    // Release texture references
    if (mat->diffuse_map.texture) {
        texture_system_release(mat->diffuse_map.texture->name);
    }

    // free renderer resources
    if (mat->shader_id != INVALID_ID && mat->internal_id != INVALID_ID) {
        renderer_shader_release_instance_resources(
            shader_system_get_by_id(mat->shader_id), mat->internal_id);
        mat->internal_id = 0;
    }

    // invalidate
    kzero_memory(mat, sizeof(material));
    mat->id = INVALID_ID;
    mat->generation = INVALID_ID;
    mat->internal_id = INVALID_ID;
}

b8 create_default_material() {
    kzero_memory(&state_ptr->default_material, sizeof(material));
    state_ptr->default_material.id = INVALID_ID;
    state_ptr->default_material.generation = INVALID_ID;
    string_ncopy(state_ptr->default_material.name, DEFAULT_MATERIAL_NAME,
                 MATERIAL_NAME_MAX_LENGTH);
    state_ptr->default_material.diffuse_colour = vec4_one();
    state_ptr->default_material.diffuse_map.use = TEXTURE_USE_MAP_DIFFUSE;
    state_ptr->default_material.diffuse_map.texture =
        texture_system_get_default_texture();

    shader *s = shader_system_get(BUILTIN_SHADER_NAME_MATERIAL);
    if (!renderer_shader_acquire_instance_resources(
            s, &state_ptr->default_material.internal_id)) {
        KFATAL("Failed to acquire renderer resources for default material. "
               "Application cannot continue.");
        return false;
    }

    return true;
}
