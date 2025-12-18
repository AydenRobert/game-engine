#include "math/geometry_utils.h"
#include "math/kmath.h"

void geometry_generate_normals(u32 vertex_count, vertex_3d *vertices,
                               u32 index_count, u32 *incices) {
    for (u32 i = 0; i < index_count; i += 3) {
        u32 i0 = incices[i + 0];
        u32 i1 = incices[i + 1];
        u32 i2 = incices[i + 2];

        vec3 edge1 = vec3_sub(vertices[i1].position, vertices[i0].position);
        vec3 edge2 = vec3_sub(vertices[i2].position, vertices[i0].position);

        vec3 normal = vec3_normalized(vec3_cross(edge1, edge2));
    }
}

void geometry_generate_tangents(u32 vertex_count, vertex_3d *vertices,
                                u32 index_count, u32 *incices) {}
