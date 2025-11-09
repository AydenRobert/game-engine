#pragma once

#include "defines.h"

KINLINE u64 kclz_u64(u64 x) {
    if (x == 0) {
        return 64;
    }
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_clzll(x);
#elif defined(_MSC_VER)
    u32 index;
    _BitScanReverse64(&index, x);
    return 63 - index;
#else
    int count = 0;
    if (!(x & 0xFFFFFFFF00000000)) {
        count += 32;
        x <<= 32;
    }
    if (!(x & 0xFFFF000000000000)) {
        count += 16;
        x <<= 16;
    }
    if (!(x & 0xFF00000000000000)) {
        count += 8;
        x <<= 8;
    }
    if (!(x & 0xF000000000000000)) {
        count += 4;
        x <<= 4;
    }
    if (!(x & 0xC000000000000000)) {
        count += 2;
        x <<= 2;
    }
    if (!(n & 0x8000000000000000)) {
        count += 1;
    }
    return count;
#endif
}

KINLINE u64 next_pow2_u64(u64 x) {
    if (x <= 1) {
        return 1;
    }

    return (u64)1 << (64 - kclz_u64(x - 1));
}
