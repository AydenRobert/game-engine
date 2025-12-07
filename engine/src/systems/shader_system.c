#include "systems/shader_system.h"

#include "containers/hashtable.h"
#include "core/kmemory.h"
#include "core/kstring.h"
#include "core/logger.h"

#include "containers/darray.h"
#include "defines.h"
#include "renderer/renderer_frontend.h"

#include "systems/texture_system.h"

typedef struct shader_system_state {
    shader_system_config config;
    hashtable lookup;
    void *lookup_memory;
    u32 current_shader_id;
    shader *shaders;
} shader_system_state;

static shader_system_state *state_ptr = 0;

b8 add_attribute(shader *shader, const shader_attribute_config *config);
b8 add_sampler(shader *shader, const shader_uniform_config *config);
b8 add_uniform(shader *shader, const shader_uniform_config *config);
u32 get_shader_id(const char *shader_name);
u32 new_shader_id();
b8 uniform_add(shader *shader, const char *uniform_name, u32 size,
               shader_uniform_type type, shader_scope scope, u32 set_location,
               b8 is_sampler);
b8 uniform_name_valid(shader *shader, const char *uniform_name);
b8 shader_uniform_add_state_valid(shader *shader);
void shader_destroy(shader *s);

b8 shader_system_initialize(u64 *memory_requirement, void *memory,
                            shader_system_config config) {
    // Block of memory will contain state structure, material array and
    // hashtable
    u64 struct_requirement = sizeof(shader_system_state);
    u64 hashtable_requirement = sizeof(u32) * config.max_shader_count;
    u64 array_requirement = sizeof(shader) * config.max_shader_count;
    *memory_requirement =
        struct_requirement + array_requirement + hashtable_requirement;

    if (!memory) {
        return true;
    }

    // setup state_ptr
    state_ptr = memory;
    state_ptr->lookup_memory = (void *)((u64)memory + struct_requirement);
    state_ptr->shaders =
        (void *)((u64)memory + struct_requirement + hashtable_requirement);
    state_ptr->config = config;
    state_ptr->current_shader_id = INVALID_ID;
    hashtable_create(sizeof(u32), config.max_shader_count,
                     state_ptr->lookup_memory, false, &state_ptr->lookup);

    // invalidate
    for (u32 i = 0; i < state_ptr->config.max_shader_count; i++) {
        state_ptr->shaders[i].id = INVALID_ID;
    }

    u32 invalid_id = INVALID_ID;
    if (!hashtable_fill(&state_ptr->lookup, &invalid_id)) {
        KERROR("shader_system_initialize - hashtable_fill failed.");
        return false;
    }

    return true;
}

void shader_system_shutdown(void *state) {
    // TODO: this feels confusing, remove pointer passing in system shutdowns
    // later
    if (!state) {
        state_ptr = 0;
        return;
    }

    shader_system_state *st = (shader_system_state *)state;
    for (u32 i = 0; i < st->config.max_shader_count; i++) {
        shader *s = &st->shaders[i];
        if (s->id != INVALID_ID) {
            shader_destroy(s);
        }
    }
    hashtable_destroy(&st->lookup);
    kzero_memory(st, sizeof(shader_system_state));

    state_ptr = 0;
}

b8 shader_system_create(const shader_config *config) {
    // Create shader struct with new id
    u32 id = new_shader_id();
    shader *out_shader = &state_ptr->shaders[id];
    kzero_memory(out_shader, sizeof(shader));
    out_shader->id = id;
    if (out_shader->id == INVALID_ID) {
        KERROR("Unable to find a free slot for new shader. Aborting.");
        return false;
    }

    // initiate state
    out_shader->state = SHADER_STATE_NOT_CREATED;
    out_shader->name = string_duplicate(config->name);
    out_shader->use_instances = config->use_instances;
    out_shader->use_locals = config->use_locals;
    out_shader->push_constant_range_count = 0;
    // max push constants size, maybe make this a macro
    kzero_memory(out_shader->push_constant_ranges, sizeof(krange) * 32);
    out_shader->bound_instance_id = INVALID_ID;
    out_shader->attribute_stride = 0;

    // Setup arrays
    out_shader->global_textures = darray_create(texture *);
    out_shader->uniforms = darray_create(shader_uniform);
    out_shader->attributes = darray_create(shader_attribute);

    // Hashtable for uniform lookup
    u64 element_size = sizeof(u16);
    // TODO: make configurable
    u64 element_count = 1024;
    out_shader->hashtable_block =
        kallocate(element_size * element_count, MEMORY_TAG_SHADER);
    hashtable_create(element_size, element_count, out_shader->hashtable_block,
                     false, &out_shader->uniform_lookup);

    // Invalidate all spots in hashtable
    u32 invalid_id;
    hashtable_fill(&out_shader->uniform_lookup, &invalid_id);

    // Running total of global and instance uniform buffer size.
    out_shader->global_ubo_size = 0;
    out_shader->ubo_size = 0;

    // To avoid complexity, go with 128 bytes that is garuenteed by vulkan
    // TODO: allow for bigger push constants by overflowing onto dynamic ubo
    out_shader->push_constant_size = 128;
    out_shader->push_constant_size = 0;

    u8 renderpass_id = INVALID_ID_U8;
    if (!renderer_renderpass_id(config->renderpass_name, &renderpass_id)) {
        KERROR("Unable to find renderpass '%s'.", config->renderpass_name);
        return false;
    }

    if (!renderer_shader_create(out_shader, renderpass_id, config->stage_count,
                                (const char **)config->stage_filenames,
                                config->stages)) {
        KERROR("Unable to create shader.");
        return false;
    }

    // Ready to be initialised
    out_shader->state = SHADER_STATE_UNINITIALIZED;

    // Process attributes
    for (u32 i = 0; i < config->attribute_count; i++) {
        add_attribute(out_shader, &config->attributes[i]);
    }

    // Process uniforms
    for (u32 i = 0; i < config->uniform_count; i++) {
        if (config->uniforms[i].type == SHADER_UNIFORM_TYPE_SAMPLER) {
            add_sampler(out_shader, &config->uniforms[i]);
        } else {
            add_uniform(out_shader, &config->uniforms[i]);
        }
    }

    // Initialize shader
    if (!renderer_shader_initialize(out_shader)) {
        KERROR("shader_system_create - initialization failed for shader '%s'.",
               config->name);
        // renderer_shader_initialize automatically destroys if it fails
        return false;
    }

    // Shader has been successfully created
    if (!hashtable_set(&state_ptr->lookup, config->name, &out_shader->id)) {
        // :(
        KERROR("shader_system_create - hashtable full, cannot add '%s'.",
               config->name);
        renderer_destroy_shader(out_shader);
        return false;
    }

    return true;
}

u32 shader_system_get_id(const char *shader_name) {
    return get_shader_id(shader_name);
}

shader *shader_system_get_by_id(u32 shader_id) {
    if (shader_id >= state_ptr->config.max_shader_count ||
        state_ptr->shaders[shader_id].id == INVALID_ID) {
        KERROR("shader_system_get_by_id - passed in invalid id.");
        return 0;
    }

    return &state_ptr->shaders[shader_id];
}

shader *shader_system_get(const char *shader_name) {
    u32 shader_id = get_shader_id(shader_name);
    if (shader_id == INVALID_ID) {
        return 0;
    }
    return &state_ptr->shaders[shader_id];
}

b8 shader_system_use(const char *shader_name) {
    u32 next_shader_id = get_shader_id(shader_name);
    if (next_shader_id == INVALID_ID) {
        return false;
    }
    return shader_system_use_by_id(next_shader_id);
}

b8 shader_system_use_by_id(u32 shader_id) {
    if (state_ptr->current_shader_id == shader_id) {
        // KINFO("shader_system_use_by_id - shader already being used.");
        return true;
    }

    shader *next_shader = shader_system_get_by_id(shader_id);
    if (!next_shader) {
        return false;
    }
    state_ptr->current_shader_id = shader_id;
    if (!renderer_shader_use(next_shader)) {
        KERROR("shader_system_use_by_id - failed to use shader '%s'.",
               next_shader->name);
        return false;
    }

    if (!renderer_shader_bind_globals(next_shader)) {
        KERROR(
            "shader_system_use_by_id - failed to bind globals for shader '%s'.",
            next_shader->name);
        return false;
    }

    return true;
}

u16 shader_system_uniform_index(shader *s, const char *uniform_name) {
    if (!s || s->id == INVALID_ID) {
        KERROR("shader_system_uniform_index - called with invalid shader.");
        return INVALID_ID_U16;
    }

    u16 index = INVALID_ID_U16;
    if (!hashtable_get(&state_ptr->lookup, s->name, &index) ||
        index == INVALID_ID_U16) {
        KERROR("shader_system_uniform_index - no uniform with name '%s' in "
               "shader '%s'.",
               uniform_name, s->name);
        return INVALID_ID_U16;
    }
    return s->uniforms[index].index;
}

b8 shader_system_uniform_set(const char *uniform_name, const void *value) {
    if (state_ptr->current_shader_id == INVALID_ID) {
        KERROR("shader_system_uniform_set - called a shader in use.");
        return false;
    }

    shader *shader = &state_ptr->shaders[state_ptr->current_shader_id];
    u16 index = shader_system_uniform_index(shader, uniform_name);
    return shader_system_uniform_set_by_index(index, value);
}

b8 shader_system_sampler_set(const char *uniform_name, const texture *t) {
    return shader_system_uniform_set(uniform_name, (void *)t);
}

b8 shader_system_uniform_set_by_index(u16 index, const void *value) {
    shader *shader = &state_ptr->shaders[state_ptr->current_shader_id];
    shader_uniform *uniform = &shader->uniforms[index];
    if (shader->bound_scope != uniform->scope) {
        if (uniform->scope == SHADER_SCOPE_GLOBAL) {
            renderer_shader_bind_globals(shader);
        } else if (uniform->scope == SHADER_SCOPE_INSTANCE) {
            renderer_shader_bind_instance(shader, shader->bound_instance_id);
        } else {
            // Nothing to do for locals
        }
        shader->bound_scope = uniform->scope;
    }
    return renderer_set_uniform(shader, uniform, value);
}

b8 shader_system_sampler_set_by_index(u16 index, const texture *t) {
    return shader_system_uniform_set_by_index(index, (void *)t);
}

b8 shader_system_apply_global() {
    return renderer_shader_apply_globals(
        &state_ptr->shaders[state_ptr->current_shader_id]);
}

b8 shader_system_apply_instance() {
    return renderer_shader_apply_instance(
        &state_ptr->shaders[state_ptr->current_shader_id]);
}

b8 shader_system_bind_instance(u32 instance_id) {
    shader *shader = &state_ptr->shaders[state_ptr->current_shader_id];
    shader->bound_instance_id = instance_id;
    return renderer_shader_bind_instance(shader, instance_id);
}

b8 add_attribute(shader *shader, const shader_attribute_config *config) {
    u32 size = 0;
    switch (config->type) {
    case SHADER_ATTRIB_TYPE_INT8:
    case SHADER_ATTRIB_TYPE_UINT8:
        size = 1;
        break;
    case SHADER_ATTRIB_TYPE_INT16:
    case SHADER_ATTRIB_TYPE_UINT16:
        size = 2;
        break;
    case SHADER_ATTRIB_TYPE_INT32:
    case SHADER_ATTRIB_TYPE_UINT32:
    case SHADER_ATTRIB_TYPE_FLOAT32:
        size = 4;
        break;
    case SHADER_ATTRIB_TYPE_FLOAT32_2:
        size = 8;
        break;
    case SHADER_ATTRIB_TYPE_FLOAT32_3:
        size = 12;
        break;
    case SHADER_ATTRIB_TYPE_FLOAT32_4:
        size = 16;
        break;
    default:
        KERROR("Unrecognised attribute type, defaulting to size of 4.");
        size = 4;
        break;
    }

    shader->attribute_stride += size;

    // Create/push the attribute
    shader_attribute attribute = {};
    attribute.name = string_duplicate(config->name);
    attribute.size = size;
    attribute.type = config->type;
    darray_push(shader->attributes, attribute);

    return true;
}

b8 add_sampler(shader *shader, const shader_uniform_config *config) {
    if (config->scope == SHADER_SCOPE_INSTANCE && !shader->use_instances) {
        KERROR("add_sampler - cannot add an instance shader for a sampler that "
               "does not use instances.");
        return false;
    }

    // Samplers can't be used for push_constants
    if (config->scope == SHADER_SCOPE_LOCAL) {
        KERROR("add_sampler - cannot add sampler at a local scope.");
        return false;
    }

    if (!uniform_name_valid(shader, config->name) ||
        !shader_uniform_add_state_valid(shader)) {
        return false;
    }

    // if global, push onto global list
    u32 location = 0;
    if (config->scope == SHADER_SCOPE_GLOBAL) {
        u32 global_texture_count = darray_length(shader->global_textures);
        if (global_texture_count + 1 > state_ptr->config.max_global_textures) {
            KERROR("add_sampler - shader global texture count (%i) exceeds the "
                   "max of %i.",
                   global_texture_count, state_ptr->config.max_global_textures);
            return false;
        }

        location = global_texture_count;
        darray_push(shader->global_textures,
                    texture_system_get_default_texture());
    } else {
        // Otherwise it's instance level. Update details for resource
        // acquisition
        if (shader->instance_texture_count + 1 >
            state_ptr->config.max_instance_textures) {
            KERROR("add_sampler - shader instance texture count (%i) exceeds "
                   "the max of %i.",
                   shader->instance_texture_count,
                   state_ptr->config.max_instance_textures);
            return false;
        }

        location = shader->instance_texture_count++;
    }

    // TODO: might need to store this somewhere else.
    if (!uniform_add(shader, config->name, 0, config->type, config->scope,
                     location, true)) {
        KERROR("add_sampler - unable to add sampler.");
        return false;
    }

    return true;
}

b8 add_uniform(shader *shader, const shader_uniform_config *config) {
    if (!uniform_name_valid(shader, config->name) ||
        !shader_uniform_add_state_valid(shader)) {
        return false;
    }
    return uniform_add(shader, config->name, config->size, config->type,
                       config->scope, 0, false);
}

u32 get_shader_id(const char *shader_name) {
    u32 shader_id = INVALID_ID;
    if (!hashtable_get(&state_ptr->lookup, shader_name, &shader_id)) {
        KERROR("get_shader_id - no shader registered with the name '%s'.",
               shader_name);
        return INVALID_ID;
    }
    return shader_id;
}

u32 new_shader_id() {
    for (u32 i = 0; i < state_ptr->config.max_shader_count; i++) {
        if (state_ptr->shaders[i].id == INVALID_ID) {
            return i;
        }
    }
    return INVALID_ID;
}

b8 uniform_add(shader *shader, const char *uniform_name, u32 size,
               shader_uniform_type type, shader_scope scope, u32 set_location,
               b8 is_sampler) {
    u32 uniform_count = darray_length(shader->uniforms);
    if (uniform_count + 1 > state_ptr->config.max_uniform_count) {
        KERROR("Shaders can only accept a maximum of %d uniforms.",
               state_ptr->config.max_uniform_count);
        return false;
    }

    shader_uniform entry;
    entry.index = uniform_count;
    entry.scope = scope;
    entry.type = type;
    b8 is_global = (scope == SHADER_SCOPE_GLOBAL);
    if (is_sampler) {
        entry.location = set_location;
    } else {
        entry.location = entry.index;
    }

    if (scope != SHADER_SCOPE_LOCAL) {
        entry.set_index = (u32)scope;
        entry.offset = is_sampler  ? 0
                       : is_global ? shader->global_ubo_size
                                   : shader->ubo_size;
        entry.size = is_sampler ? 0 : size;
    } else {
        if (!shader->use_locals) {
            KERROR("uniform_add - attempted to add local uniform to '%s' which "
                   "doesn't support locals.",
                   shader->name);
            return false;
        }

        // Push a new aligned (by 4) range.
        entry.set_index = INVALID_ID_U8;
        krange r = get_aligned_range(shader->push_constant_size, size, 4);
        entry.offset = r.offset;
        entry.size = r.size;

        // track in configuration
        shader->push_constant_ranges[shader->push_constant_range_count++] = r;

        shader->push_constant_size += r.size;
    }

    if (!hashtable_set(&shader->uniform_lookup, uniform_name, &entry.index)) {
        KERROR("uniform_add - failed to add uniform entry to hastable.");
        return false;
    }
    darray_push(shader->uniforms, entry);

    if (!is_sampler) {
        if (entry.scope == SHADER_SCOPE_GLOBAL) {
            shader->global_ubo_size += entry.size;
        } else if (entry.scope == SHADER_SCOPE_INSTANCE) {
            shader->ubo_size += entry.size;
        }
    }

    return true;
}

b8 uniform_name_valid(shader *shader, const char *uniform_name) {
    if (!uniform_name || string_length(uniform_name)) {
        KERROR("uniform_name_valid - invalid uniform_name passed.");
        return false;
    }

    u16 location;
    if (hashtable_get(&shader->uniform_lookup, uniform_name, &location) &&
        location != INVALID_ID_U16) {
        KERROR("uniform_name_valid - a uniform with the name '%s' already "
               "exists on shader '%s'.",
               uniform_name, shader->name);
        return false;
    }
    return true;
}

b8 shader_uniform_add_state_valid(shader *shader) {
    if (shader->state != SHADER_STATE_UNINITIALIZED) {
        KERROR("shader_uniform_add_state_valid - uniforms must be added before "
               "initialisation.");
        return false;
    }
    return true;
}

void shader_destroy(shader *s) {
    renderer_shader_destroy(s);

    s->state = SHADER_STATE_NOT_CREATED;
    if (s->name) {
        u32 length = string_length(s->name);
        kfree(s->name, length + 1, MEMORY_TAG_STRING);
    }
    s->name = 0;
}
