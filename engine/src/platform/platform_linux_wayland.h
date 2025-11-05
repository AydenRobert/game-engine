#pragma once

#include "defines.h"
#include "platform/platform_linux.h"

struct platform_state;
struct vulkan_context;

KAPI b8 platform_startup_wayland(display_state *disp_state,
                                 const char *application_name, i32 x, i32 y,
                                 i32 width, i32 height);

KAPI void platform_shutdown_wayland(display_state *disp_state);

KAPI b8 platform_pump_messages_wayland(display_state *disp_state);

void platform_get_required_extension_names_wayland(const char ***names_darray);

b8 platform_create_vulkan_surface_wayland(display_state *plat_state,
                                          struct vulkan_context *context);
