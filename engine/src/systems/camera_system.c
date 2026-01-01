#include "systems/camera_system.h"
#include "containers/hashtable.h"
#include "core/kstring.h"
#include "core/logger.h"
#include "defines.h"
#include "renderer/camera.h"

typedef struct camera_lookup {
    u16 id;
    u16 reference_count;
    camera c;
} camera_lookup;

typedef struct internal_state {
    camera_system_config config;
    hashtable lookup;
    void *hashtable_block;
    camera_lookup *cameras;

    camera default_camera;
} camera_system_state;

static camera_system_state *state_ptr;

b8 camera_system_initialize(u64 *memory_requirement, void *state,
                           camera_system_config config) {
    if (config.max_camera_count == 0) {
        KFATAL(
            "camera_system_initalize - config.max_camera_count must be > 0.");
        return false;
    }

    u64 struct_requirement = sizeof(camera_system_state);
    u64 array_requirement = sizeof(camera_lookup) * config.max_camera_count;
    u64 hashtable_requirement = sizeof(u16) * config.max_camera_count;
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
    state_ptr->cameras = array_block;

    // Invalidate all camera
    u32 count = state_ptr->config.max_camera_count;
    for (u32 i = 0; i < count; i++) {
        state_ptr->cameras[i].id = INVALID_ID_U16;
        state_ptr->cameras[i].reference_count = 0;
    }

    void *hashtable_block = array_block + array_requirement;
    hashtable_create(sizeof(u16), config.max_camera_count, hashtable_block,
                     false, &state_ptr->lookup);
    u16 invalid_id = INVALID_ID_U16;
    hashtable_fill(&state_ptr->lookup, &invalid_id);

    state_ptr->default_camera = camera_create();

    return true;
}

void camera_system_shutdown(void *state) {
    camera_system_state *s = (camera_system_state *)state;
    if (s) {
        // In case cameras use any extra resources
        // for (int i = 0; i < s->config.max_camera_count; i++) {
        // }
    }
    state_ptr = 0;
}

camera *camera_system_acquire(const char *name) {
    if (!state_ptr) {
        KERROR("camera_system_acquire - called before camera system was "
               "initialised.");
        return 0;
    }

    if (strings_equali(name, DEFAULT_CAMERA_NAME)) {
        return &state_ptr->default_camera;
    }

    u16 id = INVALID_ID_U16;
    if (!hashtable_get(&state_ptr->lookup, name, &id)) {
        KERROR(
            "camera_system_acquire - hashtable lookup failed. Returning null.");
        return 0;
    }

    if (id == INVALID_ID_U16) {
        for (u16 i = 0; i < state_ptr->config.max_camera_count; i++) {
            if (state_ptr->cameras[i].id != INVALID_ID_U16) {
                continue;
            }

            id = i;
            break;
        }

        if (id == INVALID_ID_U16) {
            KERROR("camera_system_acquire - failed to acquire new spot in "
                   "array. Increase config.max_camera_count. Returning null.");
            return 0;
        }

        // Create camera
        KTRACE("Creating new camera named '%s'.", name);
        state_ptr->cameras[id].id = id;
        state_ptr->cameras[id].c = camera_create();

        hashtable_set(&state_ptr->lookup, name, &id);
    }

    state_ptr->cameras[id].reference_count++;
    return &state_ptr->cameras[id].c;
}

void camera_system_release(const char *name) {
    if (!state_ptr) {
        KERROR("camera_system_release - called before camera system was "
               "initialised.");
        return;
    }

    if (strings_equali(name, DEFAULT_CAMERA_NAME)) {
        KWARN("camera_system_release - called on default camera.");
        return;
    }

    u16 id = INVALID_ID_U16;
    if (!hashtable_get(&state_ptr->lookup, name, &id)) {
        KERROR("camera_system_release - hashtable lookup failed.");
        return;
    }

    if (id == INVALID_ID_U16) {
        KWARN("camera_system_release - no camera with given name.");
        return;
    }

    state_ptr->cameras[id].reference_count--;

    if (state_ptr->cameras[id].reference_count == 0) {
        state_ptr->cameras[id].id = INVALID_ID_U16;
        camera_reset(&state_ptr->cameras[id].c);
        hashtable_set(&state_ptr->lookup, name, &id);
    }
}

camera *camera_system_get_default() {
    if (!state_ptr) {
        KERROR("camera_system_get_default - called before camera system was "
               "initialised. Returning null.");
        return 0;
    }
    return &state_ptr->default_camera;
}
