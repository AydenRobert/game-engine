#include "renderer/camera.h"
#include "defines.h"
#include "math/kmath.h"

// typedef struct camera {
//     vec3 position;
//     vec3 euler_rotation;
//     b8 is_dirty;
//     mat4 view_matrix;
// } camera;

camera camera_create() {
    camera c;
    camera_reset(&c);
    return c;
}

void camera_reset(camera *c) {
    c->position = vec3_zero();
    c->euler_rotation = vec3_zero();
    c->is_dirty = false;
    c->view_matrix = mat4_identity();
}

vec3 camera_position_get(const camera *c) {
    if (!c) {
        return vec3_zero();
    }
    return c->position;
}

void camera_position_set(camera *c, vec3 position) {
    if (!c) {
        return;
    }
    c->position = position;
    c->is_dirty = true;
}

vec3 camera_euler_get(const camera *c) {
    if (!c) {
        return vec3_zero();
    }
    return c->euler_rotation;
}

void camera_euler_set(camera *c, vec3 rotation) {
    if (!c) {
        return;
    }
    c->euler_rotation = rotation;
    c->is_dirty = true;
}

mat4 camera_view_get(camera *c) {
    if (!c) {
        return mat4_identity();
    }

    if (!c->is_dirty) {
        return c->view_matrix;
    }

    mat4 rotation = mat4_euler_xyz(c->euler_rotation.x, c->euler_rotation.y,
                                   c->euler_rotation.z);
    mat4 translation = mat4_translation(c->position);

    c->view_matrix = mat4_inverse(mat4_mul(rotation, translation));
    c->is_dirty = false;
    return c->view_matrix;
}

vec3 camera_forward(camera *c) {
    if (!c) {
        return vec3_zero();
    }

    return mat4_forward(camera_view_get(c));
}

vec3 camera_backward(camera *c) {
    if (!c) {
        return vec3_zero();
    }

    return mat4_backward(camera_view_get(c));
}

vec3 camera_left(camera *c) {
    if (!c) {
        return vec3_zero();
    }

    return mat4_left(camera_view_get(c));
}

vec3 camera_right(camera *c) {
    if (!c) {
        return vec3_zero();
    }

    return mat4_right(camera_view_get(c));
}

vec3 camera_up(camera *c) {
    if (!c) {
        return vec3_zero();
    }

    return mat4_up(camera_view_get(c));
}

vec3 camera_down(camera *c) {
    if (!c) {
        return vec3_zero();
    }

    return mat4_down(camera_view_get(c));
}

void camera_move_forward(camera *c, f32 amount) {
    if (!c) {
        return;
    }

    vec3 direction = camera_forward(c);
    direction = vec3_mul_scalar(direction, amount);
    c->position = vec3_add(c->position, direction);
    c->is_dirty = true;
}

void camera_move_backward(camera *c, f32 amount) {
    if (!c) {
        return;
    }

    vec3 direction = camera_backward(c);
    direction = vec3_mul_scalar(direction, amount);
    c->position = vec3_add(c->position, direction);
    c->is_dirty = true;
}

void camera_move_left(camera *c, f32 amount) {
    if (!c) {
        return;
    }

    vec3 direction = camera_left(c);
    direction = vec3_mul_scalar(direction, amount);
    c->position = vec3_add(c->position, direction);
    c->is_dirty = true;
}

void camera_move_right(camera *c, f32 amount) {
    if (!c) {
        return;
    }

    vec3 direction = camera_right(c);
    direction = vec3_mul_scalar(direction, amount);
    c->position = vec3_add(c->position, direction);
    c->is_dirty = true;
}

void camera_move_up(camera *c, f32 amount) {
    if (!c) {
        return;
    }

    vec3 direction = vec3_up();
    direction = vec3_mul_scalar(direction, amount);
    c->position = vec3_add(c->position, direction);
    c->is_dirty = true;
}

void camera_move_down(camera *c, f32 amount) {
    if (!c) {
        return;
    }

    vec3 direction = vec3_down();
    direction = vec3_mul_scalar(direction, amount);
    c->position = vec3_add(c->position, direction);
    c->is_dirty = true;
}

void camera_yaw(camera *c, f32 amount) {
    if (!c) {
        return;
    }

    c->euler_rotation.y += amount;
    c->is_dirty = true;
}

void camera_pitch(camera *c, f32 amount) {
    if (!c) {
        return;
    }

    c->euler_rotation.x += amount;

    static const f32 limit = 1.55334306f; // 89 degrees to avoid gimball lock
    c->euler_rotation.x = KCLAMP(c->euler_rotation.x, -limit, limit);

    c->is_dirty = true;
}
