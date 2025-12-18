#pragma once

#include "math/math_types.h"

void geometry_generate_normals(u32 vertex_count, vertex_3d *vertices, u32 index_count, u32 *incices);
void geometry_generate_tangents(u32 vertex_count, vertex_3d *vertices, u32 index_count, u32 *incices);
