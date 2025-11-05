#pragma once

#include "defines.h"

typedef struct game game;

// Application Configuration
typedef struct application_config {
    // Window starting position x axis, if applicable
    i16 start_pos_x;

    // Window starting position y axis, if applicable
    i16 start_pos_y;

    // Window starting position width axis, if applicable
    i16 start_width;

    // Window starting position height axis, if applicable
    i16 start_height;

    // The application title, if applicable
    char *name;
} application_config;

KAPI b8 application_create(game *game_inst);

KAPI b8 application_run();

void application_get_framebuffer_size(u32 *width, u32 *height);
