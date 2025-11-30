#include "systems/resource_system.h"

#include "core/kstring.h"
#include "core/logger.h"
#include "resources/resource_types.h"

// Known loader types
#include "resources/loaders/text_loader.h"
#include "resources/loaders/binary_loader.h"
#include "resources/loaders/image_loader.h"
#include "resources/loaders/material_loader.h"

typedef struct resource_system_state {
    resource_system_config config;
    resource_loader *registered_loaders;
} resource_system_state;

static resource_system_state *state_ptr = 0;

b8 load(const char *name, resource_loader *loader, resource *out_resource);

b8 resource_system_initialize(u64 *memory_requirement, void *state,
                              resource_system_config config) {
    if (config.max_loader_count == 0) {
        KFATAL("resource_system_initialize - config.max_loader_count must be > "
               "0.");
        return false;
    }

    u64 struct_requirement = sizeof(resource_system_state);
    u64 array_requirement = sizeof(resource_loader) * config.max_loader_count;
    *memory_requirement = struct_requirement + array_requirement;

    if (!state) {
        return true;
    }

    state_ptr = state;
    state_ptr->config = config;

    // The array block is after the state. Already allocated, so just set the
    // pointer.
    void *array_block = state + struct_requirement;
    state_ptr->registered_loaders = array_block;

    // Invalidate all resource loaders
    u32 count = state_ptr->config.max_loader_count;
    for (u32 i = 0; i < count; i++) {
        state_ptr->registered_loaders[i].id = INVALID_ID;
        state_ptr->registered_loaders[i].type = INVALID_ID;
    }

    // NOTE: Auto register known loader types
    resource_system_register_loader(text_resource_loader_create());
    resource_system_register_loader(binary_resource_loader_create());
    resource_system_register_loader(image_resource_loader_create());
    resource_system_register_loader(material_resource_loader_create());

    KINFO("Resource system loaded with base path: '%s'",
          config.asset_base_path);

    return true;
}

void resource_system_shutdown(void *state) {
    if (!state_ptr) {
        return;
    }

    state_ptr = 0;
}

b8 resource_system_register_loader(resource_loader loader) {
    if (!state_ptr) {
        return false;
    }

    u32 count = state_ptr->config.max_loader_count;
    // Ensure no loaders exist for the type
    for (int i = 0; i < count; i++) {
        resource_loader *l = &state_ptr->registered_loaders[i];
        if (loader.type == l->type) {
            KERROR("resource_system_register_loader - loader type '%d' already "
                   "exists and will not be registered.",
                   loader.type);
            return false;
        } else if (loader.custom_type &&
                   string_length(loader.custom_type) > 0 &&
                   strings_equali(loader.custom_type, l->custom_type)) {
            KERROR("resource_system_register_loader - custom loader type '%s' "
                   "already exists and will not be registered.",
                   loader.custom_type);
            return false;
        }
    }

    for (u32 i = 0; i < count; i++) {
        if (state_ptr->registered_loaders[i].id == INVALID_ID) {
            state_ptr->registered_loaders[i] = loader;
            state_ptr->registered_loaders[i].id = i;
            KTRACE("Loader '%d' registered.", loader.type);
#if (_DEBUG)
            if (loader.custom_type) {
                KTRACE("Custom loader '%s' registered.", loader.custom_type);
            }
#endif
            break;
        }
    }
    return true;
}

b8 resource_system_load(const char *name, resource_type type,
                        resource *out_resource) {
    if (!state_ptr) {
        KERROR("resource_system_load - used before resource system has been "
               "initialised.");
        return false;
    }

    if (type == RESOURCE_TYPE_CUSTOM) {
        KERROR("resource_system_load - used with RESOURCE_TYPE_CUSTOM, please "
               "use resource_system_load_custom.");
        return false;
    }

    u32 count = state_ptr->config.max_loader_count;
    for (u32 i = 0; i < count; i++) {
        resource_loader *l = &state_ptr->registered_loaders[i];
        if (l->type == INVALID_ID) {
            continue;
        }

        if (type == l->type) {
            return load(name, l, out_resource);
        }
    }

    out_resource->loader_id = INVALID_ID;
    KERROR("resource_system_load - no loader type found for '%d'.", type);
    return false;
}

b8 resource_system_load_custom(const char *name, const char *custom_type,
                               resource *out_resource) {
    if (!state_ptr) {
        KERROR("resource_system_load_custom - used before resource system has "
               "been initialised.");
        return false;
    }

    if (!custom_type) {
        KERROR("resource_system_load_custom - no custom_type string provided.");
        return false;
    }

    u32 count = state_ptr->config.max_loader_count;
    for (u32 i = 0; i < count; i++) {
        resource_loader *l = &state_ptr->registered_loaders[i];
        if (l->type != RESOURCE_TYPE_CUSTOM) {
            continue;
        }

        if (strings_equali(custom_type, l->custom_type)) {
            return load(name, l, out_resource);
        }
    }

    out_resource->loader_id = INVALID_ID;
    KERROR(
        "resource_system_load_custom - no custom loader type found for '%s'.",
        custom_type);
    return false;
}

void resource_system_unload(resource *resource) {
    if (!state_ptr) {
        KERROR("resource_system_unload - used before resource system has been "
               "initialised.");
        return;
    }

    if (!resource) {
        KWARN("resource_system_unload - no resource provided.");
        return;
    }

    if (resource->loader_id == INVALID_ID) {
        KWARN("resource_system_unload - invalid resource provided.");
        return;
    }

    resource_loader *loader =
        &state_ptr->registered_loaders[resource->loader_id];

    if (loader->id == INVALID_ID || !loader->unload) {
        KERROR("resource_system_unload - loader with id '%d' is invalid.",
               resource->loader_id);
        return;
    }

    loader->unload(loader, resource);
}

const char *resource_system_base_path() {
    if (!state_ptr) {
        KERROR("resource_system_base_path - used before resource system has "
               "been initialised. Returning emptry string.");
        return "";
    }

    return state_ptr->config.asset_base_path;
}

b8 load(const char *name, resource_loader *loader, resource *out_resource) {
    if (!name || !loader || !loader->load || !out_resource) {
        out_resource->loader_id = INVALID_ID;
        return false;
    }

    out_resource->loader_id = loader->id;
    return loader->load(loader, name, out_resource);
}
