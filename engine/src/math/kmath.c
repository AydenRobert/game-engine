#include "math/kmath.h"

#include "platform/platform.h"

#include <math.h>
#include <stdlib.h>

static b8 rand_seeded = false;

KAPI f32 ksin(f32 x) { return sinf(x); }

KAPI f32 kcos(f32 x) { return cosf(x); }

KAPI f32 ktan(f32 x) { return tanf(x); }

KAPI f32 kacos(f32 x) { return acosf(x); }

KAPI f32 ksqrt(f32 x) { return sqrtf(x); }

KAPI f32 kabs(f32 x) { return fabsf(x); }

KAPI i32 krandom() {
    if (!rand_seeded) {
        srand((u32)platform_get_absolute_time());
        rand_seeded = true;
    }
    return rand();
}

KAPI i32 krandom_in_range(i32 min, i32 max) {
    if (!rand_seeded) {
        srand((u32)platform_get_absolute_time());
        rand_seeded = true;
    }
    return (rand() % (max - min + 1)) + min;
}

KAPI f32 fkrandom() { return (float)krandom() / (f32)RAND_MAX; }

KAPI f32 fkrandom_in_range(f32 min, f32 max) {
    return min + ((float)krandom() / ((f32)RAND_MAX / (max - min)));
}
