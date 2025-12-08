#pragma once

#include "defines.h"

typedef struct vmm_config {
    u32 max_reserved_pages;
    u32 max_mapped_pages;
    u32 max_pool_amount;
} vmm_config;

typedef struct memory_pool {
    void *base_address;
    u32 system_page_amount;
    u32 pages_reserved;
    u32 pages_mapped;
    u8 *state_array;
} memory_pool;

typedef enum page_state { PAGE_STATE_RESERVED, PAGE_STATE_MAPPED } page_state;

b8 vmm_initialise(vmm_config config);

memory_pool *vmm_new_page_pool(u32 size);

b8 vmm_commit_pages(void *base_ptr, u32 start_index, u32 size);

b8 vmm_decommit_pages(void *base_ptr, u32 start_index, u32 size);

b8 vmm_release_page_pool(void *base_ptr);

u32 vmm_page_size();
