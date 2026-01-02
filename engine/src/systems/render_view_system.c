#include "systems/render_view_system.h"

#include "core/kstring.h"
#include "defines.h"

#include "core/kmemory.h"
#include "core/logger.h"

#include "containers/hashtable.h"

#include "renderer/renderer_frontend.h"
#include "renderer/renderer_types.inl"

// TODO: make factory
#include "renderer/views/render_view_ui.h"
#include "renderer/views/render_view_world.h"

typedef struct render_view_system_state {
    render_view_system_config config;
    hashtable lookup;
    void *hashtable_block;
    render_view *registered_views;
} render_view_system_state;

static render_view_system_state *state_ptr = 0;

b8 render_view_system_initialize(u64 *memory_requirement, void *state,
                                 render_view_system_config config) {
    if (config.max_view_count == 0) {
        KFATAL("render_view_system_initalize - config.max_view_count must be > "
               "0.");
        return false;
    }

    u64 struct_requirement = sizeof(render_view_system_state);
    u64 array_requirement = sizeof(render_view) * config.max_view_count;
    u64 hashtable_requirement = sizeof(u16) * config.max_view_count;
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
    state_ptr->registered_views = array_block;

    // Invalidate all views
    u32 count = state_ptr->config.max_view_count;
    for (u32 i = 0; i < count; i++) {
        state_ptr->registered_views[i].id = INVALID_ID_U16;
    }

    state_ptr->hashtable_block = array_block + array_requirement;
    hashtable_create(sizeof(u16), config.max_view_count,
                     state_ptr->hashtable_block, false, &state_ptr->lookup);
    u16 invalid_id = INVALID_ID_U16;
    hashtable_fill(&state_ptr->lookup, &invalid_id);

    return true;
}

void render_view_system_shutdown(void *state) { state_ptr = 0; }

b8 render_view_system_create(const render_view_config *config) {
    if (!config) {
        KERROR("render_view_system_create - requires valid config");
        return false;
    }

    if (!config->name || string_length(config->name) < 1) {
        KERROR("render_view_system_create - name is required.");
        return false;
    }

    if (config->pass_count < 1) {
        KERROR("render_view_system_create - render_view_config.pass_count must "
               "be at least 1.");
        return false;
    }

    u16 id = INVALID_ID_U16;
    if (!hashtable_get(&state_ptr->lookup, config->name, &id)) {
        KERROR("render_view_system_create - Hashtable lookup failed.");
        return false;
    }

    if (id != INVALID_ID_U16) {
        KERROR("render_view_system_create - A view named '%s' already exists.",
               config->name);
        return false;
    }

    for (u32 i = 0; i < state_ptr->config.max_view_count; i++) {
        if (state_ptr->registered_views[i].id != INVALID_ID_U16) {
            continue;
        }

        id = i;
        break;
    }

    if (id == INVALID_ID_U16) {
        KERROR("render_view_system_create - No more space in view array, "
               "increase config.max_view_count.");
        return false;
    }

    render_view *view = &state_ptr->registered_views[id];
    view->id = id;
    view->type = config->type;
    // TODO: name is leaking
    view->name = string_duplicate(config->name);
    view->custom_shader_name = config->custom_shader_name;
    view->renderpass_count = config->pass_count;
    view->passes = kallocate(sizeof(renderpass *) * view->renderpass_count,
                             MEMORY_TAG_ARRAY);

    for (u32 i = 0; i < view->renderpass_count; i++) {
        view->passes[i] = renderer_renderpass_get(config->passes[i].name);
        if (!view->passes[i]) {
            KFATAL("render_view_system_create - renderpass not found '%s'.",
                   config->passes[i].name);
            return false;
        }
    }

    // TODO: factory
    if (config->type == RENDER_VIEW_KNOWN_TYPE_WORLD) {
        view->on_create = render_view_world_on_create;
        view->on_destroy = render_view_world_on_destroy;
        view->on_resize = render_view_world_on_resize;
        view->on_build_packet = render_view_world_on_build_packet;
        view->on_render = render_view_world_on_render;
    } else if (config->type == RENDER_VIEW_KNOWN_TYPE_UI) {
        view->on_create = render_view_ui_on_create;
        view->on_destroy = render_view_ui_on_destroy;
        view->on_resize = render_view_ui_on_resize;
        view->on_build_packet = render_view_ui_on_build_packet;
        view->on_render = render_view_ui_on_render;
    }

    if (!view->on_create(view)) {
        KERROR("Failed to create view '%s'.", config->name);
        kfree(view->passes, sizeof(renderpass *) * view->renderpass_count,
              MEMORY_TAG_ARRAY);
        kzero_memory(&state_ptr->registered_views[id], sizeof(render_view));
        return false;
    }

    hashtable_set(&state_ptr->lookup, config->name, &id);

    return true;
}

void render_view_system_on_window_resize(u32 width, u32 height) {
    for (u32 i = 0; i < state_ptr->config.max_view_count; i++) {
        if (state_ptr->registered_views[i].id != INVALID_ID_U16) {
            state_ptr->registered_views[i].on_resize(
                &state_ptr->registered_views[i], width, height);
        }
    }
}

render_view *render_view_system_get(const char *name) {
    if (!state_ptr) {
        return 0;
    }

    u16 id = INVALID_ID_U16;
    if (!hashtable_get(&state_ptr->lookup, name, &id)) {
        KERROR("render_view_system_get - hashtable lookup failed.");
        return 0;
    }

    if (id == INVALID_ID_U16) {
        KERROR("render_view_system_get - no view with name '%s'.", name);
        return 0;
    }

    return &state_ptr->registered_views[id];
}

b8 render_view_system_build_packet(const render_view *view, void *data,
                                   struct render_view_packet *out_packet) {
    if (!view || !out_packet) {
        KERROR("render_view_system_build_packet - requires valid (render_view "
               "*, void *NULLABLE, render_view_packet *).");
        return false;
    }

    return view->on_build_packet(view, data, out_packet);
}

b8 render_view_system_on_render(const render_view *view,
                                const render_view_packet *packet,
                                u64 frame_number, u64 render_target_index) {
    if (!view || !packet) {
        KERROR("render_view_system_on_render - requires valid (render_view *, "
               "render_view_packet *, u64, u64).");
        return false;
    }

    return view->on_render(view, packet, frame_number, render_target_index);
}
