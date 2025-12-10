#pragma once

#include "defines.h"

typedef struct platform_state {
    void *internal_state;
} platform_state;

b8 platform_startup(platform_state *plat_state, const char *application_name,
                    i32 x, i32 y, i32 width, i32 height);

void platform_shutdown(platform_state *plat_state);

b8 platform_pump_messages(platform_state *plat_state);

void *platform_allocate(u64 size, b8 aligned);
void platform_free(void *block, b8 aligned);
void *platform_zero_memory(void *block, u64 size);
void *platform_copy_memory(void *dest, const void *source, u64 size);
void *platform_set_memory(void *dest, i32 value, u64 size);

void platform_console_write(const char *message, u8 colour);
void platform_console_write_error(const char *message, u8 colour);

f64 platform_get_absolute_time();

void platform_sleep(u64 ms);

b8 platform_memory_reserve(void *ptr, u64 size, void **memory);
b8 platform_memory_commit(void *ptr, u64 size);
b8 platform_memory_decommit(void *ptr, u64 size);
b8 platform_memory_release(void *ptr, u64 size);
u64 platform_get_page_size();

u32 platform_ctz(u64 val);
u32 platform_popcount64(u64 val);
