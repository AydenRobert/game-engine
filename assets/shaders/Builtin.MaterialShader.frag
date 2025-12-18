#version 450

layout(location = 0) out vec4 out_colour;

// Global UBO (Must match Vertex Shader exactly)
layout(set = 0, binding = 0) uniform global_uniform_object {
    mat4 projection;
    mat4 view;
    vec4 ambient_colour;
    vec3 view_position;
    int mode;
} global_ubo;

// Instance UBO
layout(set = 1, binding = 0) uniform local_uniform_object {
    vec4 diffuse_colour;
    float shininess;
} object_ubo;

layout(set = 1, binding = 1) uniform sampler2D diffuse_sampler;
layout(set = 1, binding = 2) uniform sampler2D specular_sampler;
layout(set = 1, binding = 3) uniform sampler2D normal_sampler;

// Receive mode from Vertex Shader
layout(location = 0) flat in int in_mode;

layout(location = 1) in struct dto {
    vec4 ambient;
    vec2 tex_coord;
    vec3 normal;
    vec3 frag_position;
    vec4 colour;
    vec4 tangent;
} in_dto;

struct directional_light {
    vec3 direction;
    vec4 colour;
};

struct point_light {
    vec3 position;
    vec4 colour;
    float constant;
    float linear;
    float quadratic;
};

// TODO: feed in from GPU
directional_light dir_light = {
        vec3(-0.57735, -0.57735, -0.57735),
        vec4(0.8, 0.8, 0.8, 1.0)
    };

// TODO: feed in from cpu
point_light p_light_0 = {
        vec3(-5.5, 0.0, -5.5),
        vec4(0.0, 1.0, 0.0, 1.0),
        1.0, // Constant
        0.35, // Linear
        0.44 // Quadratic
    };

// TODO: feed in from cpu
point_light p_light_1 = {
        vec3(5.5, 0.0, -5.5),
        vec4(1.0, 0.0, 0.0, 1.0),
        1.0, // Constant
        0.35, // Linear
        0.44 // Quadratic
    };

vec4 calculate_directional_light(directional_light light, vec3 normal, vec3 view_direction);
vec4 calculate_point_light(point_light light, vec3 normal, vec3 frag_position, vec3 view_direction);

void main() {
    // 1. Normalize Inputs
    vec3 norm_geom = normalize(in_dto.normal);
    vec3 tang_geom = normalize(in_dto.tangent.xyz);

    // 2. Calculate TBN
    tang_geom = normalize(tang_geom - dot(tang_geom, norm_geom) * norm_geom);
    vec3 bitangent = cross(norm_geom, tang_geom) * in_dto.tangent.w;
    mat3 TBN = mat3(tang_geom, bitangent, norm_geom);

    // 3. Calculate Normal
    vec3 final_normal = norm_geom;

    vec3 map_normal = texture(normal_sampler, in_dto.tex_coord).rgb;
    vec3 localNormal = 2.0 * map_normal - 1.0;
    localNormal.y *= -1.0;
    final_normal = normalize(TBN * localNormal);

    // 4. Mode Switching
    if (in_mode == 0 || in_mode == 1) {
        vec3 view_direction = normalize(global_ubo.view_position - in_dto.frag_position);

        out_colour = calculate_directional_light(dir_light, final_normal, view_direction);

        out_colour += calculate_point_light(p_light_0, final_normal, in_dto.frag_position, view_direction);
        out_colour += calculate_point_light(p_light_1, final_normal, in_dto.frag_position, view_direction);
    } else if (in_mode == 2) {
        // Mode 2: Show Normals
        out_colour = vec4(abs(final_normal), 1.0);
    }
}

vec4 calculate_directional_light(directional_light light, vec3 normal, vec3 view_direction) {
    float diffuse_factor = max(dot(normal, -light.direction), 0.0);

    // Default samples (White if no texture mode)
    vec4 diff_samp = vec4(1.0, 1.0, 1.0, 1.0);
    vec4 spec_samp = vec4(1.0, 1.0, 1.0, 1.0);

    // Only sample textures if we are in Mode 0
    if (in_mode == 0) {
        diff_samp = texture(diffuse_sampler, in_dto.tex_coord);
        spec_samp = texture(specular_sampler, in_dto.tex_coord);
    }

    // Ambient
    vec4 ambient = vec4(vec3(in_dto.ambient * object_ubo.diffuse_colour), diff_samp.a);

    // Diffuse
    vec4 diffuse = vec4(vec3(light.colour * diffuse_factor), diff_samp.a);

    // Specular
    vec3 reflect_direction = reflect(light.direction, normal);
    float spec = pow(max(dot(view_direction, reflect_direction), 0.0), object_ubo.shininess);

    // Apply specular sample (Red channel usually)
    vec4 specular = vec4(vec3(light.colour * spec * spec_samp.r), diff_samp.a);

    // Apply Texture Colors (if Mode 0, diff_samp is the texture. If Mode 1, it's white).
    diffuse *= diff_samp;
    ambient *= diff_samp;

    // Note: Specular is usually additive, so we don't multiply it by diffuse texture,
    // but we DO multiply it by the specular map (already done above).

    return (ambient + diffuse + specular);
}

vec4 calculate_point_light(point_light light, vec3 normal, vec3 frag_position, vec3 view_direction) {
    vec3 light_direction = normalize(light.position - frag_position);
    float diff = max(dot(normal, light_direction), 0.0);

    vec3 reflect_direction = reflect(-light_direction, normal);
    float spec = pow(max(dot(view_direction, reflect_direction), 0.0), object_ubo.shininess);

    // Calculate attenuation, or light falloff over distance.
    float distance = length(light.position - frag_position);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    vec4 ambient = in_dto.ambient;
    vec4 diffuse = light.colour * diff;
    vec4 specular = light.colour * spec;

    if (in_mode == 0) {
        vec4 diff_samp = texture(diffuse_sampler, in_dto.tex_coord);
        diffuse *= diff_samp;
        ambient *= diff_samp;
        specular *= vec4(texture(specular_sampler, in_dto.tex_coord).rgb, diffuse.a);
    }

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}
