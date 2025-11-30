#pragma once

#include "core/kmemory.h"
#include "defines.h"
#include "resources/resource_types.h"

struct resource_loader;

b8 resource_unload(struct resource_loader *self, resource *resource,
                   memory_tag tag);
