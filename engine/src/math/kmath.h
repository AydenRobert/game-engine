#pragma once

#include "math/math_types.h"

#define K_PI 3.14159265358979323846f
#define K_PI_2 2.0f * K_PI
#define K_HALF_PI 0.5f * K_PI
#define K_QUATER_PI 0.25f * K_PI
#define K_ONE_ON_PI 1.0f / K_PI
#define K_ONE_ON_TWO_PI 1.0f / K_PI_2
#define K_SQRT_TWO 1.41421356237309504880f
#define K_SQRT_THREE 1.73205080756887729352f
#define K_ONE_ON_SQRT_TWO 0.70710678118654752440f
#define K_ONE_ON_SQRT_THREE 0.70710678118654752440f
#define K_DEG2RAD_MULTIPLIER K_PI / 180.0f
#define K_RAD2DEG_MULTIPLIER 180.0f / K_PI

#define K_SEC_TO_MS_MULTIPLIER 1000.0f
#define K_MS_TO_SEC_MULTIPLIER 0.001f

#define K_INFINITY 1e30f
#define K_FLOAT_EPSILON 1.192092896e-07f

// General math functions
KAPI f32 ksin(f32 x);
KAPI f32 kcos(f32 x);
KAPI f32 ktan(f32 x);
KAPI f32 kacos(f32 x);
KAPI f32 ksqrt(f32 x);
KAPI f32 kabs(f32 x);

/**
 * Indicates if the value is a power of 2. 0 indicates that it's not.
 * @param value The value to be interpreted.
 * @returns True if a power of 2, otherwise, false.
 */
KINLINE b8 is_power_of_two(u64 value) {
    return (value != 0) && ((value & (value - 1)) == 0);
}

KAPI i32 krandom();
KAPI i32 krandom_in_range(i32 min, i32 max);

KAPI f32 fkrandom();
KAPI f32 fkrandom_in_range(f32 min, f32 max);

// VECTOR 2

/**
 * @brief Creates and returns a new 2-element vector using the supplied values.
 *
 * @param x The x value.
 * @param y The y value.
 * @return A new 2-element vector.
 */
KINLINE vec2 vec2_create(f32 x, f32 y) {
    vec2 out_vector;
    out_vector.x = x;
    out_vector.y = y;
    return out_vector;
}

KINLINE vec2 vec2_zero() { return (vec2){{0.0f, 0.0f}}; }

KINLINE vec2 vec2_one() { return (vec2){{1.0f, 1.0f}}; };

KINLINE vec2 vec2_up() { return (vec2){{0.0f, 1.0f}}; };

KINLINE vec2 vec2_down() { return (vec2){{0.0f, -1.0f}}; };

KINLINE vec2 vec2_left() { return (vec2){{-1.0f, 0.0f}}; };

KINLINE vec2 vec2_right() { return (vec2){{1.0f, 0.0f}}; };

KINLINE vec2 vec2_add(vec2 vector_0, vec2 vector_1) {
    return (vec2){{vector_0.x + vector_1.x, vector_0.y + vector_1.y}};
}

KINLINE vec2 vec2_sub(vec2 vector_0, vec2 vector_1) {
    return (vec2){{vector_0.x - vector_1.x, vector_0.y - vector_1.y}};
}

KINLINE vec2 vec2_mul(vec2 vector_0, vec2 vector_1) {
    return (vec2){{vector_0.x * vector_1.x, vector_0.y * vector_1.y}};
}

KINLINE vec2 vec2_div(vec2 vector_0, vec2 vector_1) {
    return (vec2){{vector_0.x / vector_1.x, vector_0.y / vector_1.y}};
}

KINLINE f32 vec2_len_squared(vec2 vector) {
    return vector.x * vector.x + vector.y * vector.y;
}

KINLINE f32 vec2_len(vec2 vector) { return ksqrt(vec2_len_squared(vector)); }

KINLINE void vec2_normalize(vec2 *vector) {
    const f32 len = vec2_len(*vector);
    vector->x /= len;
    vector->y /= len;
}

KINLINE vec2 vec2_normalized(vec2 vector) {
    vec2_normalize(&vector);
    return vector;
}

KINLINE b8 vec2_compare(vec2 vector_0, vec2 vector_1, f32 tolerance) {
    if (kabs(vector_0.x - vector_1.x) > tolerance) {
        return FALSE;
    }

    if (kabs(vector_0.y - vector_1.y) > tolerance) {
        return FALSE;
    }

    return TRUE;
}

KINLINE f32 vec2_distance(vec2 vector_0, vec2 vector_1) {
    vec2 d = vec2_sub(vector_0, vector_1);
    return vec2_len(d);
}

/**
 * @brief Creates and returns a new 2-element vector using the supplied values.
 *
 * @param x The x value.
 * @param y The y value.
 * @return A new 2-element vector.
 */
KINLINE vec3 vec3_create(f32 x, f32 y, f32 z) {
    vec3 out_vector;
    out_vector.x = x;
    out_vector.y = y;
    out_vector.z = z;
    return out_vector;
}

KINLINE vec4 vec3_to_vec4(vec3 vector, f32 w) {
#if defined(KUSE_SIMD)
    vec4 out_vector;
    out_vector.data = _mm_setr_ps(x, y, z, w);
    return out_vector
#else
    return (vec4){{vector.x, vector.y, vector.z, w}};
#endif
}

KINLINE vec3 vec3_zero() { return (vec3){{0.0f, 0.0f, 0.0f}}; }

KINLINE vec3 vec3_one() { return (vec3){{1.0f, 1.0f, 1.0f}}; };

KINLINE vec3 vec3_up() { return (vec3){{0.0f, 1.0f, 0.0f}}; };

KINLINE vec3 vec3_down() { return (vec3){{0.0f, -1.0f, 0.0f}}; };

KINLINE vec3 vec3_left() { return (vec3){{-1.0f, 0.0f, 0.0f}}; };

KINLINE vec3 vec3_right() { return (vec3){{1.0f, 0.0f, 0.0f}}; };

KINLINE vec3 vec3_forward() { return (vec3){{0.0f, 0.0f, -1.0f}}; };

KINLINE vec3 vec3_backward() { return (vec3){{0.0f, 0.0f, 1.0f}}; };

KINLINE vec3 vec3_add(vec3 vector_0, vec3 vector_1) {
    return (vec3){{vector_0.x + vector_1.x, vector_0.y + vector_1.y,
                   vector_0.z + vector_1.z}};
}

KINLINE vec3 vec3_sub(vec3 vector_0, vec3 vector_1) {
    return (vec3){{vector_0.x - vector_1.x, vector_0.y - vector_1.y,
                   vector_0.z - vector_1.z}};
}

KINLINE vec3 vec3_mul(vec3 vector_0, vec3 vector_1) {
    return (vec3){{vector_0.x * vector_1.x, vector_0.y * vector_1.y,
                   vector_0.z * vector_1.z}};
}

KINLINE vec3 vec3_div(vec3 vector_0, vec3 vector_1) {
    return (vec3){{vector_0.x / vector_1.x, vector_0.y / vector_1.y,
                   vector_0.z / vector_1.z}};
}

KINLINE vec3 vec3_mul_scalar(vec3 vector_0, f32 value) {
    return (vec3){{vector_0.x * value, vector_0.y * value, vector_0.z * value}};
}

KINLINE f32 vec3_len_squared(vec3 vector) {
    return vector.x * vector.x + vector.y * vector.y + vector.z * vector.z;
}

KINLINE f32 vec3_len(vec3 vector) { return ksqrt(vec3_len_squared(vector)); }

KINLINE void vec3_normalize(vec3 *vector) {
    const f32 len = vec3_len(*vector);
    vector->x /= len;
    vector->y /= len;
    vector->z /= len;
}

KINLINE vec3 vec3_normalized(vec3 vector) {
    vec3_normalize(&vector);
    return vector;
}

KINLINE f32 vec3_dot(vec3 vector_0, vec3 vector_1) {
    f32 p = 0;
    p += vector_0.x * vector_1.x;
    p += vector_0.y * vector_1.y;
    p += vector_0.z * vector_1.z;
    return p;
}

KINLINE vec3 vec3_cross(vec3 vector_0, vec3 vector_1) {
    return (vec3){{vector_0.y * vector_1.z - vector_0.z * vector_1.y,
                   vector_0.z * vector_1.x - vector_0.x * vector_1.z,
                   vector_0.x * vector_1.y - vector_0.y * vector_1.x}};
}

KINLINE b8 vec3_compare(vec3 vector_0, vec3 vector_1, f32 tolerance) {
    if (kabs(vector_0.x - vector_1.x) > tolerance) {
        return FALSE;
    }

    if (kabs(vector_0.y - vector_1.y) > tolerance) {
        return FALSE;
    }

    if (kabs(vector_0.z - vector_1.z) > tolerance) {
        return FALSE;
    }

    return TRUE;
}

KINLINE f32 vec3_distance(vec3 vector_0, vec3 vector_1) {
    vec3 d = vec3_sub(vector_0, vector_1);
    return vec3_len(d);
}

// VECTOR 4

KINLINE vec4 vec4_create(f32 x, f32 y, f32 z, f32 w) {
    vec4 out_vector;
#if defined(KUSE_SIMD)
    out_vector.data = _mm_setr_ps(x, y, z, w);
#else
    out_vector.x = x;
    out_vector.y = y;
    out_vector.z = z;
    out_vector.w = w;
#endif
    return out_vector;
}

KINLINE vec3 vec4_to_vec3(vec4 vector) {
    return (vec3){{vector.x, vector.y, vector.z}};
}

KINLINE vec4 vec4_zero() { return (vec4){{0.0f, 0.0f, 0.0f, 0.0f}}; }

KINLINE vec4 vec4_one() { return (vec4){{1.0f, 1.0f, 1.0f, 1.0f}}; };

KINLINE vec4 vec4_add(vec4 vector_0, vec4 vector_1) {
    return (vec4){{vector_0.x + vector_1.x, vector_0.y + vector_1.y,
                   vector_0.z + vector_1.z, vector_0.w + vector_1.w}};
}

KINLINE vec4 vec4_sub(vec4 vector_0, vec4 vector_1) {
    return (vec4){{vector_0.x - vector_1.x, vector_0.y - vector_1.y,
                   vector_0.z - vector_1.z, vector_0.w - vector_1.w}};
}

KINLINE vec4 vec4_mul(vec4 vector_0, vec4 vector_1) {
    return (vec4){{vector_0.x * vector_1.x, vector_0.y * vector_1.y,
                   vector_0.z * vector_1.z, vector_0.w * vector_1.w}};
}

KINLINE vec4 vec4_div(vec4 vector_0, vec4 vector_1) {
    return (vec4){{vector_0.x / vector_1.x, vector_0.y / vector_1.y,
                   vector_0.z / vector_1.z, vector_0.w / vector_1.w}};
}

KINLINE f32 vec4_len_squared(vec4 vector) {
    return vector.x * vector.x + vector.y * vector.y + vector.z * vector.z +
           vector.w * vector.w;
}

KINLINE f32 vec4_len(vec4 vector) { return ksqrt(vec4_len_squared(vector)); }

KINLINE void vec4_normalize(vec4 *vector) {
    const f32 len = vec4_len(*vector);
    vector->x /= len;
    vector->y /= len;
    vector->z /= len;
    vector->w /= len;
}

KINLINE vec4 vec4_normalized(vec4 vector) {
    vec4_normalize(&vector);
    return vector;
}

KINLINE f32 vec4_dot_f32(f32 a0, f32 a1, f32 a2, f32 a3, f32 b0, f32 b1, f32 b2,
                         f32 b3) {
    return a0 * b0 + a1 * b1 + a2 * b2 + a3 * b3;
}
