#include "systems/texture_system.h"

#include "containers/hashtable.h"
#include "core/kmemory.h"
#include "core/kstring.h"
#include "core/logger.h"

#include "defines.h"
#include "renderer/renderer_frontend.h"
#include "renderer/vulkan/vulkan_backend.h"

#include "resources/resource_types.h"

#include "systems/resource_system.h"

typedef struct texture_system_state {
    texture_system_config config;
    texture default_texture;
    texture default_diffuse_texture;
    texture default_specular_texture;
    texture default_normal_texture;

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
b8 process_texture_reference(const char *name, i8 reference_diff,
                             b8 auto_release, b8 skip_load,
                             u32 *out_texture_id);

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
        renderer_texture_destroy(t);
    }

    destroy_default_textures(state_ptr);

    state_ptr = 0;
}

texture *texture_system_acquire(const char *name, b8 auto_release) {
    // Return default texture, warn against using this for default textures
    // TODO: check against other default names
    if (strings_equali(name, DEFAULT_TEXTURE_NAME)) {
        KWARN("texture_system_acquire called for default texture. Use "
              "texture_system_get_default_texture for texture '%s'",
              DEFAULT_TEXTURE_NAME);
    }

    u32 id = INVALID_ID;
    if (!process_texture_reference(name, 1, auto_release, false, &id)) {
        KERROR("texture_system_acquire failed to acquire new texture id.",
               name);
        return 0;
    }

    return &state_ptr->registered_textures[id];
}

texture *texture_system_acquire_writeable(const char *name, u32 width,
                                          u32 height, u8 channel_count,
                                          b8 has_transparency) {
    u32 id = INVALID_ID;
    if (!process_texture_reference(name, 1, false, true, &id)) {
        KERROR("texture_system_acquire_writeable failed to acquire new texture "
               "id.",
               name);
    }

    texture *t = &state_ptr->registered_textures[id];
    t->id = id;
    string_ncopy(t->name, name, TEXTURE_NAME_MAX_LENGTH);
    t->width = width;
    t->height = height;
    t->channel_count = channel_count;
    t->generation = INVALID_ID;
    t->flags |= has_transparency ? TEXTURE_FLAG_HAS_TRANSPARENCY : 0;
    t->flags |= TEXTURE_FLAG_IS_WRITEABLE;
    t->internal_data = 0;
    renderer_texture_create_writeable(t);
    return t;
}

void texture_system_release(const char *name) {
    // check against other default texture names
    if (strings_equali(name, DEFAULT_TEXTURE_NAME)) {
        KWARN("texture_system_release called for default texture.");
        return;
    }

    u32 id = INVALID_ID;
    if (!process_texture_reference(name, -1, false, false, &id)) {
        KERROR(
            "texture_system_release failed to release texture '%s' properly.",
            name);
    }
}

texture *texture_system_wrap_internal(const char *name, u32 width, u32 height,
                                      u8 channel_count, b8 has_transparency,
                                      b8 is_writeable, b8 register_texture,
                                      void *internal_data) {
    u32 id = INVALID_ID;
    texture *t = 0;
    if (register_texture) {
        if (!process_texture_reference(name, 1, false, true, &id)) {
            KERROR("texture_system_wrap_internal failed to obtain new texture "
                   "id.");
            return 0;
        }
        t = &state_ptr->registered_textures[id];
    } else {
        t = kallocate(sizeof(texture), MEMORY_TAG_TEXTURE);
        // KTRACE("texture_system_wrap_internal created texture '%s', but not "
               // "registered. Resulting in an allocation, it is up to the caller "
               // "to free this.",
               // name);
    }

    t->id = id;
    string_ncopy(t->name, name, TEXTURE_NAME_MAX_LENGTH);
    t->width = width;
    t->height = height;
    t->channel_count = channel_count;
    t->generation = INVALID_ID;
    t->flags |= has_transparency ? TEXTURE_FLAG_HAS_TRANSPARENCY : 0;
    t->flags |= is_writeable ? TEXTURE_FLAG_IS_WRITEABLE : 0;
    t->flags |= TEXTURE_FLAG_IS_WRAPPED;
    t->internal_data = internal_data;
    return t;
}

b8 texture_system_set_internal(texture *t, void *internal_data) {
    if (!t) {
        return false;
    }

    t->internal_data = internal_data;
    t->generation++;
    return true;
}

b8 texture_system_resize(texture *t, u32 width, u32 height,
                         b8 regenerate_internal_data) {
    if (!t) {
        return false;
    }

    if (!(t->flags & TEXTURE_FLAG_IS_WRITEABLE)) {
        KWARN("texture_system_resize - should not be called on a texture that "
              "isn't writeable.");
        return false;
    }
    t->width = width;
    t->height = height;

    if (!(t->flags & TEXTURE_FLAG_IS_WRAPPED) && regenerate_internal_data) {
        renderer_texture_resize(t, width, height);
    }
    t->generation++;
    return true;
}

#define GET_DEFAULT_FUNC(texture_name)                                         \
    texture *texture_system_get_##texture_name() {                             \
        if (!state_ptr) {                                                      \
            KERROR(                                                            \
                "texture_system_get_%s failed. System should be initialised "  \
                "when using this function. Null pointer returned.",            \
                #texture_name);                                                \
            return 0;                                                          \
        }                                                                      \
        return &state_ptr->texture_name;                                       \
    }

GET_DEFAULT_FUNC(default_texture)
GET_DEFAULT_FUNC(default_diffuse_texture)
GET_DEFAULT_FUNC(default_specular_texture)
GET_DEFAULT_FUNC(default_normal_texture)

b8 create_default_textures(texture_system_state *state_ptr) {
    // NOTE: Create default texture, 256x256 blue/white checkerboard patten
    // KTRACE("Creating default texture...");
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
    state_ptr->default_texture.flags = 0;
    renderer_texture_create(pixels, &state_ptr->default_texture);
    state_ptr->default_texture.generation = INVALID_ID;

    // Diffuse texture.
    // KTRACE("Creating default diffuse texture...");
    u8 diff_pixels[16 * 16 * 4];
    // Default diff map is black (no diffuse)
    kset_memory(diff_pixels, 0, sizeof(u8) * 16 * 16 * 4);
    string_ncopy(state_ptr->default_diffuse_texture.name,
                 DEFAULT_DIFFUSE_TEXTURE_NAME, TEXTURE_NAME_MAX_LENGTH);
    state_ptr->default_diffuse_texture.width = 16;
    state_ptr->default_diffuse_texture.height = 16;
    state_ptr->default_diffuse_texture.channel_count = 4;
    state_ptr->default_diffuse_texture.generation = INVALID_ID;
    state_ptr->default_texture.flags = 0;
    renderer_texture_create(diff_pixels, &state_ptr->default_diffuse_texture);
    // Manually set the texture generation to invalid since this is a default
    // texture.
    state_ptr->default_diffuse_texture.generation = INVALID_ID;

    // Specular texture.
    // KTRACE("Creating default specular texture...");
    u8 spec_pixels[16 * 16 * 4];
    // Default spec map is black (no specular)
    kset_memory(spec_pixels, 0, sizeof(u8) * 16 * 16 * 4);
    string_ncopy(state_ptr->default_specular_texture.name,
                 DEFAULT_SPECULAR_TEXTURE_NAME, TEXTURE_NAME_MAX_LENGTH);
    state_ptr->default_specular_texture.width = 16;
    state_ptr->default_specular_texture.height = 16;
    state_ptr->default_specular_texture.channel_count = 4;
    state_ptr->default_specular_texture.generation = INVALID_ID;
    state_ptr->default_texture.flags = 0;
    renderer_texture_create(spec_pixels, &state_ptr->default_specular_texture);
    // Manually set the texture generation to invalid since this is a default
    // texture.
    state_ptr->default_specular_texture.generation = INVALID_ID;

    // Normal texture.
    // KTRACE("Creating default normal texture...");
    u8 normal_pixels[16 * 16 * 4]; // w * h * channels
    kset_memory(normal_pixels, 0, sizeof(u8) * 16 * 16 * 4);
    // Each pixel.
    for (u64 row = 0; row < 16; ++row) {
        for (u64 col = 0; col < 16; ++col) {
            u64 index = (row * 16) + col;
            u64 index_bpp = index * bpp;
            // Set blue, z-axis by default and alpha.
            normal_pixels[index_bpp + 0] = 128;
            normal_pixels[index_bpp + 1] = 128;
            normal_pixels[index_bpp + 2] = 255;
            normal_pixels[index_bpp + 3] = 255;
        }
    }
    string_ncopy(state_ptr->default_normal_texture.name,
                 DEFAULT_NORMAL_TEXTURE_NAME, TEXTURE_NAME_MAX_LENGTH);
    state_ptr->default_normal_texture.width = 16;
    state_ptr->default_normal_texture.height = 16;
    state_ptr->default_normal_texture.channel_count = 4;
    state_ptr->default_normal_texture.generation = INVALID_ID;
    state_ptr->default_texture.flags = 0;
    renderer_texture_create(normal_pixels, &state_ptr->default_normal_texture);
    // Manually set the texture generation to invalid since this is a default
    // texture.
    state_ptr->default_normal_texture.generation = INVALID_ID;

    return true;
}

void destroy_default_textures(texture_system_state *state_ptr) {
    if (!state_ptr) {
        return;
    }

    destroy_texture(&state_ptr->default_texture);
    destroy_texture(&state_ptr->default_diffuse_texture);
    destroy_texture(&state_ptr->default_specular_texture);
    destroy_texture(&state_ptr->default_normal_texture);
}

b8 load_texture(const char *texture_name, texture *t) {
    resource img_resource;
    if (!resource_system_load(texture_name, RESOURCE_TYPE_IMAGE,
                              &img_resource)) {
        KERROR("Failed to load image resource for texture '%s'.", texture_name);
        return false;
    }

    image_resource_data *resource_data = img_resource.data;

    texture temp_texture = {0};
    temp_texture.width = resource_data->width;
    temp_texture.height = resource_data->height;
    temp_texture.channel_count = resource_data->channel_count;
    state_ptr->default_texture.flags = 0;

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
    temp_texture.flags = has_transparency ? TEXTURE_FLAG_HAS_TRANSPARENCY : 0;

    // Acquire internal texture resources and upload to GPU.
    renderer_texture_create(resource_data->pixels, &temp_texture);

    // Take a copy of the old texture.
    texture old = *t;

    // Assign the temp texture to the pointer.
    *t = temp_texture;

    // Destroy the old texture.
    renderer_texture_destroy(&old);

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
    renderer_texture_destroy(tex);

    kzero_memory(tex->name, sizeof(char) * TEXTURE_NAME_MAX_LENGTH);
    kzero_memory(tex, sizeof(texture));
    tex->id = INVALID_ID;
    tex->generation = INVALID_ID;
}

b8 process_texture_reference(const char *name, i8 reference_diff,
                             b8 auto_release, b8 skip_load,
                             u32 *out_texture_id) {
    *out_texture_id = INVALID_ID;

    if (!state_ptr) {
        KERROR("process_texture_reference called before texture system "
               "initialised");
        return false;
    }

    texture_reference ref;
    if (!hashtable_get(&state_ptr->registered_texture_table, name, &ref)) {
        KERROR("Failed to acquire id for name '%s'. INVALID_ID returned.",
               name);
    }

    // If reference count is zero, then this is a new entry, or an entry that
    // doesn't exist/isn't referenced.
    if (ref.reference_count == 0) {
        if (reference_diff > 0) {
            ref.auto_release = auto_release;
        } else {
            if (ref.auto_release) {
                KWARN("Tried to release a non-existent texture, '%s'.", name);
                return false;
            } else {
                KWARN("Tried to release a texture with auto_release=false & "
                      "reference_count=0.");
                return true;
            }
        }
    }

    ref.reference_count += reference_diff;

    char name_copy[TEXTURE_NAME_MAX_LENGTH];
    string_ncopy(name_copy, name, TEXTURE_NAME_MAX_LENGTH);

    if (reference_diff < 0) {
        // decrementing the reference count
        if (ref.reference_count == 0 && ref.auto_release) {
            texture *t = &state_ptr->registered_textures[ref.handle];

            destroy_texture(t);

            ref.handle = INVALID_ID;
            ref.auto_release = false;
            // KTRACE("Release texture '%s', texture unloaded because "
                   // "reference_count=0 and auto_release=true.",
                   // name_copy);
        } else {
            // KTRACE(
                // "Release texture '%s', reference_count=%i and auto_release=%s.",
                // name_copy, ref.reference_count,
                // ref.auto_release ? "true" : "false");
        }
    } else {
        // incrementing the reference count
        if (ref.handle == INVALID_ID) {
            // find a free place in the array
            u32 count = state_ptr->config.max_texture_count;
            for (u32 i = 0; i < count; i++) {
                if (state_ptr->registered_textures[i].id != INVALID_ID) {
                    continue;
                }

                ref.handle = i;
                *out_texture_id = i;
                break;
            }

            // no index found
            if (*out_texture_id == INVALID_ID) {
                KFATAL("Texture system cannot hold any more textures, please "
                       "adjust the config.");
                return false;
            }

            texture *t = &state_ptr->registered_textures[ref.handle];
            // Create new texture
            if (skip_load) {
                // KTRACE("Load skipped for texture '%s'.", name);
            } else {
                if (!load_texture(name, t)) {
                    *out_texture_id = INVALID_ID;
                    KERROR("Failed to load texture '%s'.", name);
                    return false;
                }
                t->id = ref.handle;
            }
            // KTRACE("Texture '%s' does not exist yet. Created, ref_count=%i.",
                   // name, ref.reference_count);
        } else {
            *out_texture_id = ref.handle;
            // KTRACE("Texture '%s' already exists. ref_count=%i.", name,
                   // ref.reference_count);
        }
    }

    hashtable_set(&state_ptr->registered_texture_table, name_copy, &ref);
    return true;
}
