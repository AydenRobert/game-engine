#version 450

layout(location = 0) out vec4 out_colour;

// Instance Scope (Set 1)
// Binding 0: Uniform Buffer (Standard types packed together)
layout(set = 1, binding = 0) uniform local_uniform_object {
    vec4 diffuse_colour;
    float shininess; // Added to match config
} object_ubo;

// Binding 1: Diffuse Sampler
layout(set = 1, binding = 1) uniform sampler2D diffuse_sampler;

// Binding 2: Specular Sampler
layout(set = 1, binding = 2) uniform sampler2D specular_sampler;

struct directional_light {
    vec3 direction;
    vec4 colour;
};

// TODO: feed in from cpu
directional_light dir_light = {
        vec3(-0.57735, -0.57735, -0.57735),
        vec4(0.8, 0.8, 0.8, 1.0)
    };

layout(location = 1) in struct dto {
    vec4 ambient;
    vec2 tex_coord;
    vec3 normal;
} in_dto;

vec4 calculate_directional_light(directional_light light, vec3 normal);

void main() {
    // Normal lighting calculation
    vec4 lit_colour = calculate_directional_light(dir_light, in_dto.normal);

    // DEBUG: Sample the specular map
    vec4 spec_samp = texture(specular_sampler, in_dto.tex_coord);

    // VISUALIZATION 1: Output ONLY the specular map.
    // If your object turns black/white (matching your map), it works.
    out_colour = spec_samp;

    // VISUALIZATION 2 (Comment out above and uncomment this to see it added to lighting):
    // out_colour = lit_colour + spec_samp;
}

vec4 calculate_directional_light(directional_light light, vec3 normal) {
    float diffuse_factor = max(dot(normal, -light.direction), 0.0);

    vec4 diff_samp = texture(diffuse_sampler, in_dto.tex_coord);
    vec4 ambient = vec4(vec3(in_dto.ambient * object_ubo.diffuse_colour), diff_samp.a);
    vec4 diffuse = vec4(vec3(light.colour * diffuse_factor), diff_samp.a);

    diffuse *= diff_samp;
    ambient *= diff_samp;

    return (ambient + diffuse);
}
