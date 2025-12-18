#version 450

layout(location = 0) out vec4 out_colour;

// Global UBO (Set 0)
layout(set = 0, binding = 0) uniform global_uniform_object {
    mat4 projection;
    mat4 view;
    vec4 ambient_colour;
    vec3 view_position; // Added
} global_ubo;

// Instance UBO (Set 1, Binding 0)
layout(set = 1, binding = 0) uniform local_uniform_object {
    vec4 diffuse_colour;
    float shininess; // Added
} object_ubo;

// Samplers (Set 1, Bindings 1 & 2)
layout(set = 1, binding = 1) uniform sampler2D diffuse_sampler;
layout(set = 1, binding = 2) uniform sampler2D specular_sampler;

// Data Transfer Object (must match Vertex Shader)
layout(location = 1) in struct dto {
    vec4 ambient;
    vec2 tex_coord;
    vec3 normal;
    vec3 frag_position;
} in_dto;

struct directional_light {
    vec3 direction;
    vec4 colour;
};

// TODO: feed in from cpu
directional_light dir_light = {
        vec3(-0.57735, -0.57735, -0.57735),
        vec4(0.8, 0.8, 0.8, 1.0)
    };

vec4 calculate_directional_light(directional_light light, vec3 normal, vec3 view_direction);

void main() {
    vec3 view_direction = normalize(global_ubo.view_position - in_dto.frag_position);

    out_colour = calculate_directional_light(dir_light, normalize(in_dto.normal), view_direction);
}

vec4 calculate_directional_light(directional_light light, vec3 normal, vec3 view_direction) {
    float diffuse_factor = max(dot(normal, -light.direction), 0.0);

    // 1. Get Texture Samples
    vec4 diff_samp = texture(diffuse_sampler, in_dto.tex_coord);
    vec4 spec_samp = texture(specular_sampler, in_dto.tex_coord);

    // 2. Ambient
    vec4 ambient = vec4(vec3(in_dto.ambient * object_ubo.diffuse_colour), diff_samp.a);

    // 3. Diffuse
    vec4 diffuse = vec4(vec3(light.colour * diffuse_factor), diff_samp.a);

    // 4. Specular
    // Reflect light direction around the normal
    vec3 reflect_direction = reflect(light.direction, normal);

    // Calculate the "dot" between the reflection and the eye (view direction)
    float spec = pow(max(dot(view_direction, reflect_direction), 0.0), object_ubo.shininess);

    // Combine specular colour, light colour, and texture sample
    // Note: Assuming specular map is grayscale (Red channel used often), but vec4 works for colored spec maps.
    vec4 specular = vec4(vec3(light.colour * spec * spec_samp.r), diff_samp.a);

    // 5. Combine results
    // Apply textures to ambient/diffuse
    diffuse *= diff_samp;
    ambient *= diff_samp;

    return (ambient + diffuse + specular);
}
