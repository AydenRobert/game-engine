#include "kmemory.h"

#include "core/kstring.h"
#include "core/logger.h"
#include "memory/dynamic_allocator.h"
#include "platform/platform.h"

#include <stdio.h>

struct memory_stats {
    u64 total_allocated;
    u64 tagged_allocations[MEMORY_TAG_MAX_TAGS];
};

static const char *memory_tag_strings[MEMORY_TAG_MAX_TAGS] = {
    "UNKNOWN          ", "ARRAY            ", "LINEAR ALLOCATOR ",
    "DARRAY           ", "DICT             ", "RING_QUEUE       ",
    "BST              ", "STRING           ", "APPLICATION      ",
    "JOB              ", "TEXTURE          ", "MATERIAL_INSTANCE",
    "RENDERER         ", "GAME             ", "TRANSFORM        ",
    "ENTITY           ", "ENTITY_NODE      ", "SCENE            ",
    "SHADER           "};

typedef struct memory_system_state {
    memory_system_configuration config;
    b8 initialized;
    struct memory_stats stats;
    u64 alloc_count;
    u64 allocator_memory_requirement;
    dynamic_allocator allocator;
    void *allocator_block;
} memory_system_state;

static memory_system_state *state_ptr;

b8 memory_system_initialize(memory_system_configuration config) {
    u64 alloc_memory_requirement = 0;
    dynamic_allocator_create(config.total_alloc_count,
                             &alloc_memory_requirement, 0, 0);
    u64 total_memory_size =
        sizeof(memory_system_state) + alloc_memory_requirement;

    void *memory_block = platform_allocate(total_memory_size, false);
    if (!memory_block) {
        KFATAL("Couldn't allocate memory for Memory System. Cannot continue.");
        return false;
    }

    state_ptr = (memory_system_state *)memory_block;
    state_ptr->allocator_memory_requirement = alloc_memory_requirement;

    state_ptr->allocator_block =
        (void *)((u64)state_ptr + sizeof(memory_system_state));
    dynamic_allocator_create(config.total_alloc_count,
                             &state_ptr->allocator_memory_requirement,
                             state_ptr->allocator_block, &state_ptr->allocator);

    state_ptr->alloc_count = 0;
    state_ptr->initialized = true;

    return true;
}

void memory_system_shutdown() {
    if (!state_ptr) {
        KWARN("Tried to shutdown memory system without it being initialized.");
        return;
    }

    dynamic_allocator_destroy(&state_ptr->allocator);
    u64 total_memory_size =
        sizeof(memory_system_state) + state_ptr->allocator_memory_requirement;
    platform_free(state_ptr, total_memory_size);
    state_ptr = 0;
}

KAPI void *kallocate(u64 size, memory_tag tag) {
    if (tag == MEMORY_TAG_UNKNOWN) {
        KWARN("kallocate called using MEMORY_TAG_UNKNOWN. Please re-class this "
              "allocation.");
    }

    // TODO: Memory alignment
    void *block;
    if (!state_ptr) {
        KWARN("kallocate called before memory system initialized.");
        block = platform_allocate(size, false);
    } else {
        state_ptr->stats.total_allocated += size;
        state_ptr->stats.tagged_allocations[tag] += size;
        state_ptr->alloc_count++;

        block = dynamic_allocator_allocate(&state_ptr->allocator, size);
    }

    if (block) {
        platform_zero_memory(block, size);
    }

    return block;
}

KAPI void kfree(void *block, u64 size, memory_tag tag) {
    if (tag == MEMORY_TAG_UNKNOWN) {
        KWARN("kfree called using MEMORY_TAG_UNKNOWN. Please re-class this "
              "allocation.");
    }

    // TODO: Memory alignment
    if (!state_ptr) {
        KWARN("kallocate called before memory system initialized.");
        platform_free(block, false);
    } else {
        state_ptr->stats.total_allocated -= size;
        state_ptr->stats.tagged_allocations[tag] -= size;

        b8 result = dynamic_allocator_free(&state_ptr->allocator, block, size);
        if (!result) {
            // Handle dynamic allocator failing gracefully
            // The piece of memory could have been created before initialisation
            platform_free(block, false);
        }
    }
}

KAPI void *kzero_memory(void *block, u64 size) {
    return platform_zero_memory(block, size);
}
KAPI void *kcopy_memory(void *dest, const void *source, u64 size) {
    return platform_copy_memory(dest, source, size);
}
KAPI void *kset_memory(void *dest, i32 value, u64 size) {
    return platform_set_memory(dest, value, size);
}

KAPI char *get_memory_usage_str() {
    const u64 gib = 1024 * 1024 * 1024;
    const u64 mib = 1024 * 1024;
    const u64 kib = 1024;

    char buffer[8000] = "System memory use (tagged):\n";
    u64 offset = string_length(buffer);

    for (u32 i = 0; i < MEMORY_TAG_MAX_TAGS; i++) {
        char unit[4] = "XiB";
        f32 amount = 1.0f;

        if (state_ptr->stats.tagged_allocations[i] >= gib) {
            unit[0] = 'G';
            amount = state_ptr->stats.tagged_allocations[i] / (f32)gib;
        } else if (state_ptr->stats.tagged_allocations[i] >= mib) {
            unit[0] = 'M';
            amount = state_ptr->stats.tagged_allocations[i] / (f32)mib;
        } else if (state_ptr->stats.tagged_allocations[i] >= kib) {
            unit[0] = 'K';
            amount = state_ptr->stats.tagged_allocations[i] / (f32)kib;
        } else {
            unit[0] = 'B';
            unit[1] = 0;
            amount = state_ptr->stats.tagged_allocations[i];
        }

        i32 length = snprintf(buffer + offset, 8000, "  %s: %.2f%s\n",
                              memory_tag_strings[i], amount, unit);
        offset += length;
    }

    char *out_string = string_duplicate(buffer);
    return out_string;
}

u64 get_memory_alloc_count() {
    if (!state_ptr) {
        return 0;
    }
    return state_ptr->alloc_count;
}
