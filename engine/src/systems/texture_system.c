#include "systems/texture_system.h"

#include "containers/hashtable.h"
#include "core/kmemory.h"
#include "core/kstring.h"
#include "core/logger.h"

#include "defines.h"
#include "renderer/renderer_frontend.h"
#include "resources/resource_types.h"

#include "systems/resource_system.h"

typedef struct texture_system_state {
    texture_system_config config;
    texture default_texture;

    // Array of registered textures
    texture *registered_textures;

    // Hashtable for texture lookups
    hashtable registered_texture_table;
} texture_system_state;

typedef struct texture_reference {
    u64 reference_count;
    u32 handle;
    b8 auto_release;
} texture_reference;

static texture_system_state *state_ptr = 0;

b8 create_default_textures(texture_system_state *state_ptr);
void destroy_default_textures(texture_system_state *state_ptr);
b8 load_texture(const char *texture_name, texture *t);
void destroy_texture(texture *texture);

b8 texture_system_initialize(u64 *memory_requirement, void *state,
                             texture_system_config config) {
    if (config.max_texture_count == 0) {
        KFATAL("texture_system_initialize - config.max_texture_count must be > "
               "0.");
        return false;
    }

    // Block of memory will contain state structure, texture array and hashtable
    u64 struct_requirement = sizeof(texture_system_state);
    u64 array_requirement = sizeof(texture) * config.max_texture_count;
    u64 hashtable_requirement =
        sizeof(texture_reference) * config.max_texture_count;
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
    state_ptr->registered_textures = array_block;

    // Hashtable block is after array block
    void *hashtable_block = array_block + array_requirement;
    hashtable_create(sizeof(texture_reference), config.max_texture_count,
                     hashtable_block, false,
                     &state_ptr->registered_texture_table);

    // Fill the hash table with invalid references to use as a default
    texture_reference invalid_ref;
    invalid_ref.auto_release = false;
    invalid_ref.handle = INVALID_ID;
    invalid_ref.reference_count = 0;
    hashtable_fill(&state_ptr->registered_texture_table, &invalid_ref);

    // Invalidate all textures in texture array
    u32 count = state_ptr->config.max_texture_count;
    for (u32 i = 0; i < count; i++) {
        state_ptr->registered_textures[i].id = INVALID_ID;
        state_ptr->registered_textures[i].generation = INVALID_ID;
    }

    // Create default textures
    create_default_textures(state_ptr);

    return true;
}

void texture_system_shutdown(void *state) {
    if (!state_ptr) {
        return;
    }

    // Destroy all loaded textures
    for (u32 i = 0; i < state_ptr->config.max_texture_count; i++) {
        texture *t = &state_ptr->registered_textures[i];
        if (t->generation == INVALID_ID) {
            continue;
        }
        renderer_destroy_texture(t);
    }

    destroy_default_textures(state_ptr);

    state_ptr = 0;
}

texture *texture_system_acquire(const char *name, b8 auto_release) {
    // Return default texture, warn against using this for default textures
    if (strings_equali(name, DEFAULT_TEXTURE_NAME)) {
        KWARN("texture_system_acquire called for default texture. Use "
              "texture_system_get_default_texture for texture '%s'",
              DEFAULT_TEXTURE_NAME);
    }

    if (!state_ptr) {
        KERROR("texture_system_acquire failed to acquire texture '%s'. System "
               "should be initialized before using this function.",
               name);
        return 0;
    }

    // This should hopefully never get called...
    texture_reference texture_reference;
    if (!hashtable_get(&state_ptr->registered_texture_table, name,
                       &texture_reference)) {
        KERROR("texture_system_acquire failed to acquire texture '%s'. Null "
               "pointer will be returned.",
               name);
        return 0;
    }

    if (texture_reference.reference_count == 0) {
        texture_reference.auto_release = auto_release;
    }
    texture_reference.reference_count++;

    if (texture_reference.handle == INVALID_ID) {
        // find a free index
        u32 count = state_ptr->config.max_texture_count;
        texture *texture = 0;
        for (u32 i = 0; i < count; i++) {
            if (state_ptr->registered_textures[i].id != INVALID_ID) {
                continue;
            }
            texture_reference.handle = i;
            texture = &state_ptr->registered_textures[i];
            break;
        }

        if (!texture || texture_reference.handle == INVALID_ID) {
            KFATAL("texture_system_acquire - Texture system cannot hold "
                   "anymore textures. Adjust configuration to allow more.");
            return 0;
        }

        if (!load_texture(name, texture)) {
            KERROR("Failed to load texture '%s'.", name);
            return 0;
        }

        texture->id = texture_reference.handle;

    } else {
        KTRACE("Texture '%s' already exists, ref count has been increased to "
               "'%i'.",
               name, texture_reference.reference_count);
    }

    // Update the entry
    hashtable_set(&state_ptr->registered_texture_table, name,
                  &texture_reference);
    return &state_ptr->registered_textures[texture_reference.handle];
}

void texture_system_release(const char *name) {
    if (strings_equali(name, DEFAULT_TEXTURE_NAME)) {
        KWARN("texture_system_release called for default texture.");
        return;
    }

    if (!state_ptr) {
        KERROR("texture_system_release failed to acquire texture '%s'. System "
               "should be initialized when using this function.",
               name);
        return;
    }

    // This should hopefully never get called...
    texture_reference ref;
    if (!hashtable_get(&state_ptr->registered_texture_table, name, &ref)) {
        KERROR("texture_system_release failed to release texture '%s'.", name);
        return;
    }

    if (ref.reference_count == 0) {
        KWARN("texture_system_release tried to release non-existant texture "
              "'%s'.",
              name);
        return;
    }

    // A lot of the time, name will be passed in from texture.name
    char name_copy[TEXTURE_NAME_MAX_LENGTH];
    string_ncopy(name_copy, name, TEXTURE_NAME_MAX_LENGTH);

    ref.reference_count--;

    if (ref.reference_count == 0 && ref.auto_release) {
        texture *texture = &state_ptr->registered_textures[ref.handle];

        destroy_texture(texture);

        // Reset the reference
        ref.handle = INVALID_ID;
        ref.auto_release = false;
        KTRACE("Released texture '%s'. Texture is now unloaded as "
               "reference_count = 0 and auto_release = true.",
               name_copy);
    } else {
        KTRACE(
            "Released texture '%s'. reference_count = %i, auto_release = %s.",
            name_copy, ref.reference_count,
            ref.auto_release ? "true" : "false");
    }

    // Update the entry
    hashtable_set(&state_ptr->registered_texture_table, name_copy, &ref);
}

texture *texture_system_get_default_texture() {
    if (!state_ptr) {
        KERROR("texture_system_get_default_texture failed. System should be "
               "initialized when using this function. Null pointer returned.");
        return 0;
    }

    return &state_ptr->default_texture;
}

b8 create_default_textures(texture_system_state *state_ptr) {
    // NOTE: Create default texture, 256x256 blue/white checkerboard patten
    KTRACE("Creating default texture...");
    const u32 tex_dimension = 256;
    const u32 bpp = 4;
    const u32 pixel_count = tex_dimension * tex_dimension;

    u8 pixels[pixel_count * bpp];
    // u8* pixels = kallocate(sizeof(u8) * pixel_count * bpp,
    // MEMORY_TAG_TEXTURE);
    kset_memory(pixels, 255, sizeof(u8) * pixel_count * bpp);

    for (u64 row = 0; row < tex_dimension; row++) {
        for (u64 col = 0; col < tex_dimension; col++) {
            u64 index = (row * tex_dimension) + col;
            u64 index_bpp = index * bpp;

            if (row % 2) {
                if (col % 2) {
                    pixels[index_bpp + 0] = 0;
                    pixels[index_bpp + 1] = 0;
                }
            } else {
                if (!(col % 2)) {
                    pixels[index_bpp + 0] = 0;
                    pixels[index_bpp + 1] = 0;
                }
            }
        }
    }

    string_ncopy(state_ptr->default_texture.name, DEFAULT_TEXTURE_NAME,
                 TEXTURE_NAME_MAX_LENGTH);
    state_ptr->default_texture.width = tex_dimension;
    state_ptr->default_texture.height = tex_dimension;
    state_ptr->default_texture.channel_count = 4;
    state_ptr->default_texture.generation = INVALID_ID;
    state_ptr->default_texture.has_transparency = false;
    renderer_create_texture(pixels, &state_ptr->default_texture);
    state_ptr->default_texture.generation = INVALID_ID;

    return true;
}

void destroy_default_textures(texture_system_state *state_ptr) {
    if (!state_ptr) {
        return;
    }

    destroy_texture(&state_ptr->default_texture);
}

b8 load_texture(const char *texture_name, texture *t) {
    resource img_resource;
    if (!resource_system_load(texture_name, RESOURCE_TYPE_IMAGE,
                              &img_resource)) {
        KERROR("Failed to load image resource for texture '%s'.", texture_name);
        return false;
    }

    image_resource_data *resource_data = img_resource.data;

    texture temp_texture;
    temp_texture.width = resource_data->width;
    temp_texture.height = resource_data->height;
    temp_texture.channel_count = resource_data->channel_count;

    u32 current_generation = t->generation;
    t->generation = INVALID_ID;

    u64 total_size =
        temp_texture.width * temp_texture.height * temp_texture.channel_count;
    // Check for transparency
    b32 has_transparency = false;
    for (u64 i = 0; i < total_size; i += temp_texture.channel_count) {
        u8 a = resource_data->pixels[i + 3];
        if (a < 255) {
            has_transparency = true;
            break;
        }
    }

    // Take a copy of the name
    string_ncopy(temp_texture.name, texture_name, TEXTURE_NAME_MAX_LENGTH);
    temp_texture.generation = INVALID_ID;
    temp_texture.has_transparency = has_transparency;

    // Acquire internal texture resources and upload to GPU.
    renderer_create_texture(resource_data->pixels, &temp_texture);

    // Take a copy of the old texture.
    texture old = *t;

    // Assign the temp texture to the pointer.
    *t = temp_texture;

    // Destroy the old texture.
    renderer_destroy_texture(&old);

    if (current_generation == INVALID_ID) {
        t->generation = 0;
    } else {
        t->generation = current_generation + 1;
    }

    // Clean up data.
    resource_system_unload(&img_resource);
    return true;
}

void destroy_texture(texture *tex) {
    renderer_destroy_texture(tex);

    kzero_memory(tex->name, sizeof(char) * TEXTURE_NAME_MAX_LENGTH);
    kzero_memory(tex, sizeof(texture));
    tex->id = INVALID_ID;
    tex->generation = INVALID_ID;
}
