#include "containers/darray.h"
#include "resources/loaders/material_loader.h"

#include "core/kmemory.h"
#include "core/kstring.h"
#include "core/logger.h"
#include "math/kmath.h"
#include "resources/loaders/loader_utils.h"
#include "resources/resource_types.h"
#include "systems/resource_system.h"

#include "platform/filesystem.h"
#include "systems/shader_system.h"

b8 shader_loader_load(struct resource_loader *self, const char *name,
                      resource *out_resource) {
    if (!self || !name || !out_resource) {
        return false;
    }

    char *format_str = "%s/%s/%s%s";
    char full_file_path[512];
    string_format(full_file_path, format_str, resource_system_base_path(),
                  self->type_path, name, ".shadercfg");

    file_handle file;
    if (!filesystem_open(full_file_path, FILE_MODE_READ, false, &file)) {
        KERROR("material_loader_load - unable to open material file for "
               "reading: '%s'.",
               full_file_path);
    }

    // TODO: should use string interning
    out_resource->full_path = string_duplicate(full_file_path);

    shader_config *resource_data =
        kallocate(sizeof(shader_config), MEMORY_TAG_SHADER);
    resource_data->attribute_count = 0;
    resource_data->attributes = darray_create(shader_attribute_config);
    resource_data->uniform_count = 0;
    resource_data->uniforms = darray_create(shader_uniform_config);
    resource_data->stage_count = 0;
    resource_data->stages = darray_create(shader_stage);
    resource_data->use_instances = false;
    resource_data->use_locals = false;
    resource_data->stage_names = darray_create(char *);
    resource_data->stage_filenames = darray_create(char *);
    resource_data->renderpass_name = 0;
    resource_data->name = 0;

    // Read each line of the file
    char line_buff[512] = "";
    char *p = line_buff;
    u64 line_length = 0;
    u32 line_number = 1;
    while (filesystem_read_line(&file, 511, &p, &line_length)) {
        // Trim the string
        char *trimmed = string_trim(line_buff);
        line_length = string_length(trimmed);

        // Skip blank line or comment
        if (line_length < 1 || trimmed[0] == '#') {
            line_number++;
            continue;
        }

        i32 equal_index = string_index_of_char(trimmed, '=');
        if (equal_index == -1) {
            KWARN("Potential formatting issue found in file '%s': '=' token "
                  "not found. Skipping line %ui.",
                  full_file_path, line_number);
            line_number++;
            continue;
        }

        // Assume max of 64 length for variable names
        char raw_variable_name[64];
        kzero_memory(raw_variable_name, sizeof(char) * 64);
        string_mid(raw_variable_name, trimmed, 0, equal_index);
        char *trimmed_variable_name = string_trim(raw_variable_name);

        // 512 max line width; take away null terminator, 64 variable name, and
        // the equal sign
        // 512 - 1 - 64 - 1 = 446
        char raw_value[446];
        kzero_memory(raw_value, sizeof(char) * 446);
        string_mid(raw_value, trimmed, equal_index + 1, -1);
        char *trimmed_value = string_trim(raw_value);

        // Process variable
        if (strings_equali(trimmed_variable_name, "version")) {
            // TODO: handle versioning
        } else if (strings_equali(trimmed_variable_name, "name")) {
            resource_data->name = string_duplicate(trimmed_value);
        } else if (strings_equali(trimmed_variable_name, "renderpass")) {
            resource_data->renderpass_name = string_duplicate(trimmed_value);
        } else if (strings_equali(trimmed_variable_name, "stages")) {
            char **stage_names = darray_create(char *);
            u32 count =
                string_split(trimmed_value, ',', &stage_names, true, true);
            resource_data->stage_names = stage_names;
            // Ensure stage name and stage file name count are the same, as they
            // should align.
            if (resource_data->stage_count == 0) {
                resource_data->stage_count = count;
            } else if (resource_data->stage_count != count) {
                KERROR("shader_loader_load: Invalid file layout. Count "
                       "mismatch between stage names and stage filenames.");
            }
            // Parse the stages
            for (u8 i = 0; i < resource_data->stage_count; i++) {
                if (strings_equali(stage_names[i], "frag") ||
                    strings_equali(stage_names[i], "fragment")) {
                    darray_push(resource_data->stages, SHADER_STAGE_FRAGMENT);
                } else if (strings_equali(stage_names[i], "vert") ||
                           strings_equali(stage_names[i], "vertex")) {
                    darray_push(resource_data->stages, SHADER_STAGE_VERTEX);
                } else if (strings_equali(stage_names[i], "geom") ||
                           strings_equali(stage_names[i], "geometry")) {
                    darray_push(resource_data->stages, SHADER_STAGE_GEOMETRY);
                } else if (strings_equali(stage_names[i], "comp") ||
                           strings_equali(stage_names[i], "compute")) {
                    darray_push(resource_data->stages, SHADER_STAGE_COMPUTE);
                }
            }
        } else if (strings_equali(trimmed_variable_name, "stagefiles")) {
            resource_data->stage_filenames = darray_create(char *);
            u32 count =
                string_split(trimmed_value, ',',
                             &resource_data->stage_filenames, true, true);
            // Ensure stage name and stage file name count are the same, as they
            // should align.
            if (resource_data->stage_count == 0) {
                resource_data->stage_count = count;
            } else if (resource_data->stage_count != count) {
                KERROR("shader_loader_load: Invalid file layout. Count "
                       "mismatch between stage names and stage filenames.");
            }
        } else if (strings_equali(trimmed_variable_name, "use_instances")) {
            string_to_b8(trimmed_value, &resource_data->use_instances);
        } else if (strings_equali(trimmed_variable_name, "use_locals")) {
            string_to_b8(trimmed_value, &resource_data->use_locals);
        } else if (strings_equali(trimmed_variable_name, "attribute")) {
            // Parse attribute
            char **fields = darray_create(char *);
            u32 field_count =
                string_split(trimmed_value, ',', &fields, true, true);
            if (field_count != 2) {
                KERROR("shader_loader_load: Invalid file layout. Attribute "
                       "fields must be 'type,name'. Skipping.");
            } else {
                shader_attribute_config attribute;
                char *type_str = fields[0];
                // Parse field type
                // --- Float 32 ---
                if (strings_equali(type_str, "f32")) {
                    attribute.type = SHADER_ATTRIB_TYPE_FLOAT32;
                    attribute.size = 4;
                } else if (strings_equali(type_str, "vec2")) {
                    attribute.type = SHADER_ATTRIB_TYPE_FLOAT32_2;
                    attribute.size = 8;
                } else if (strings_equali(type_str, "vec3")) {
                    attribute.type = SHADER_ATTRIB_TYPE_FLOAT32_3;
                    attribute.size = 12;
                } else if (strings_equali(type_str, "vec4")) {
                    attribute.type = SHADER_ATTRIB_TYPE_FLOAT32_4;
                    attribute.size = 16;
                } else if (strings_equali(type_str, "mat4")) {
                    attribute.type = SHADER_ATTRIB_TYPE_MATRIX4;
                    attribute.size = 64; // 16 floats * 4 bytes

                    // --- Int 8 ---
                } else if (strings_equali(type_str, "i8")) {
                    attribute.type = SHADER_ATTRIB_TYPE_INT8;
                    attribute.size = 1;
                } else if (strings_equali(type_str, "i8vec2")) {
                    attribute.type = SHADER_ATTRIB_TYPE_INT8_2;
                    attribute.size = 2;
                } else if (strings_equali(type_str, "i8vec3")) {
                    attribute.type = SHADER_ATTRIB_TYPE_INT8_3;
                    attribute.size = 3;
                } else if (strings_equali(type_str, "i8vec4")) {
                    attribute.type = SHADER_ATTRIB_TYPE_INT8_4;
                    attribute.size = 4;

                    // --- UInt 8 ---
                } else if (strings_equali(type_str, "u8")) {
                    attribute.type = SHADER_ATTRIB_TYPE_UINT8;
                    attribute.size = 1;
                } else if (strings_equali(type_str, "u8vec2")) {
                    attribute.type = SHADER_ATTRIB_TYPE_UINT8_2;
                    attribute.size = 2;
                } else if (strings_equali(type_str, "u8vec3")) {
                    attribute.type = SHADER_ATTRIB_TYPE_UINT8_3;
                    attribute.size = 3;
                } else if (strings_equali(type_str, "u8vec4")) {
                    attribute.type = SHADER_ATTRIB_TYPE_UINT8_4;
                    attribute.size = 4;

                    // --- Int 16 ---
                } else if (strings_equali(type_str, "i16")) {
                    attribute.type = SHADER_ATTRIB_TYPE_INT16;
                    attribute.size = 2;
                } else if (strings_equali(type_str, "i16vec2")) {
                    attribute.type = SHADER_ATTRIB_TYPE_INT16_2;
                    attribute.size = 4;
                } else if (strings_equali(type_str, "i16vec3")) {
                    attribute.type = SHADER_ATTRIB_TYPE_INT16_3;
                    attribute.size = 6;
                } else if (strings_equali(type_str, "i16vec4")) {
                    attribute.type = SHADER_ATTRIB_TYPE_INT16_4;
                    attribute.size = 8;

                    // --- UInt 16 ---
                } else if (strings_equali(type_str, "u16")) {
                    attribute.type = SHADER_ATTRIB_TYPE_UINT16;
                    attribute.size = 2;
                } else if (strings_equali(type_str, "u16vec2")) {
                    attribute.type = SHADER_ATTRIB_TYPE_UINT16_2;
                    attribute.size = 4;
                } else if (strings_equali(type_str, "u16vec3")) {
                    attribute.type = SHADER_ATTRIB_TYPE_UINT16_3;
                    attribute.size = 6;
                } else if (strings_equali(type_str, "u16vec4")) {
                    attribute.type = SHADER_ATTRIB_TYPE_UINT16_4;
                    attribute.size = 8;

                    // --- Int 32 ---
                } else if (strings_equali(type_str, "i32") ||
                           strings_equali(type_str, "int")) {
                    attribute.type = SHADER_ATTRIB_TYPE_INT32;
                    attribute.size = 4;
                } else if (strings_equali(type_str, "ivec2")) {
                    attribute.type = SHADER_ATTRIB_TYPE_INT32_2;
                    attribute.size = 8;
                } else if (strings_equali(type_str, "ivec3")) {
                    attribute.type = SHADER_ATTRIB_TYPE_INT32_3;
                    attribute.size = 12;
                } else if (strings_equali(type_str, "ivec4")) {
                    attribute.type = SHADER_ATTRIB_TYPE_INT32_4;
                    attribute.size = 16;

                    // --- UInt 32 ---
                } else if (strings_equali(type_str, "u32") ||
                           strings_equali(type_str, "uint")) {
                    attribute.type = SHADER_ATTRIB_TYPE_UINT32;
                    attribute.size = 4;
                } else if (strings_equali(type_str, "uvec2")) {
                    attribute.type = SHADER_ATTRIB_TYPE_UINT32_2;
                    attribute.size = 8;
                } else if (strings_equali(type_str, "uvec3")) {
                    attribute.type = SHADER_ATTRIB_TYPE_UINT32_3;
                    attribute.size = 12;
                } else if (strings_equali(type_str, "uvec4")) {
                    attribute.type = SHADER_ATTRIB_TYPE_UINT32_4;
                    attribute.size = 16;
                } else {
                    KERROR("shader_loader_load: Invalid file layout. Attribute "
                           "type must be f32, vec2, vec3, vec4, i8, i16, i32, "
                           "u8, u16, or u32.");
                    KWARN("Defaulting to f32.");
                    attribute.type = SHADER_ATTRIB_TYPE_FLOAT32;
                    attribute.size = 4;
                }

                // Take a copy of the attribute name
                attribute.name_length = string_length(fields[1]);
                attribute.name = string_duplicate(fields[1]);

                // Add the attribute
                darray_push(resource_data->attributes, attribute);
                resource_data->attribute_count++;
            }

            string_cleanup_split_array(fields);
            darray_destroy(fields);
        } else if (strings_equali(trimmed_variable_name, "uniform")) {
            // Parse uniform.
            char **fields = darray_create(char *);
            u32 field_count =
                string_split(trimmed_value, ',', &fields, true, true);
            if (field_count != 3) {
                KERROR("shader_loader_load: Invalid file layout. Uniform "
                       "fields must be 'type,scope,name'. Skipping.");
            } else {
                shader_uniform_config uniform;
                // Parse field type
                if (strings_equali(fields[0], "f32")) {
                    uniform.type = SHADER_UNIFORM_TYPE_FLOAT32;
                    uniform.size = 4;
                } else if (strings_equali(fields[0], "vec2")) {
                    uniform.type = SHADER_UNIFORM_TYPE_FLOAT32_2;
                    uniform.size = 8;
                } else if (strings_equali(fields[0], "vec3")) {
                    uniform.type = SHADER_UNIFORM_TYPE_FLOAT32_3;
                    uniform.size = 12;
                } else if (strings_equali(fields[0], "vec4")) {
                    uniform.type = SHADER_UNIFORM_TYPE_FLOAT32_4;
                    uniform.size = 16;
                } else if (strings_equali(fields[0], "u8")) {
                    uniform.type = SHADER_UNIFORM_TYPE_UINT8;
                    uniform.size = 1;
                } else if (strings_equali(fields[0], "u16")) {
                    uniform.type = SHADER_UNIFORM_TYPE_UINT16;
                    uniform.size = 2;
                } else if (strings_equali(fields[0], "u32")) {
                    uniform.type = SHADER_UNIFORM_TYPE_UINT32;
                    uniform.size = 4;
                } else if (strings_equali(fields[0], "i8")) {
                    uniform.type = SHADER_UNIFORM_TYPE_INT8;
                    uniform.size = 1;
                } else if (strings_equali(fields[0], "i16")) {
                    uniform.type = SHADER_UNIFORM_TYPE_INT16;
                    uniform.size = 2;
                } else if (strings_equali(fields[0], "i32")) {
                    uniform.type = SHADER_UNIFORM_TYPE_INT32;
                    uniform.size = 4;
                } else if (strings_equali(fields[0], "mat4")) {
                    uniform.type = SHADER_UNIFORM_TYPE_MATRIX_4;
                    uniform.size = 64;
                } else if (strings_equali(fields[0], "samp") ||
                           strings_equali(fields[0], "sampler")) {
                    uniform.type = SHADER_UNIFORM_TYPE_SAMPLER;
                    uniform.size = 0; // Samplers don't have a size.
                } else {
                    KERROR("shader_loader_load: Invalid file layout. Uniform "
                           "type must be f32, vec2, vec3, vec4, i8, i16, i32, "
                           "u8, u16, u32 or mat4.");
                    KWARN("Defaulting to f32.");
                    uniform.type = SHADER_UNIFORM_TYPE_FLOAT32;
                    uniform.size = 4;
                }

                // Parse the scope
                if (strings_equal(fields[1], "0")) {
                    uniform.scope = SHADER_SCOPE_GLOBAL;
                } else if (strings_equal(fields[1], "1")) {
                    uniform.scope = SHADER_SCOPE_INSTANCE;
                } else if (strings_equal(fields[1], "2")) {
                    uniform.scope = SHADER_SCOPE_LOCAL;
                } else {
                    KERROR("shader_loader_load: Invalid file layout: Uniform "
                           "scope must be 0 for global, 1 for instance or 2 "
                           "for local.");
                    KWARN("Defaulting to global.");
                    uniform.scope = SHADER_SCOPE_GLOBAL;
                }

                // Take a copy of the attribute name.
                uniform.name_length = string_length(fields[2]);
                uniform.name = string_duplicate(fields[2]);

                // Add the attribute.
                darray_push(resource_data->uniforms, uniform);
                resource_data->uniform_count++;
            }

            string_cleanup_split_array(fields);
            darray_destroy(fields);
        }

        // TODO: more fields

        // Clear the line buffer
        kzero_memory(line_buff, sizeof(char) * 512);
        line_number++;
    }

    filesystem_close(&file);

    out_resource->data = resource_data;
    out_resource->data_size = sizeof(material_config);
    out_resource->name = name;

    return true;
}

void shader_loader_unload(struct resource_loader *self, resource *resource) {
    if (!resource_unload(self, resource, MEMORY_TAG_MATERIAL_INSTANCE)) {
        KWARN("material_loader_unload - called with nullptr for self or "
              "resource.");
    }
}

resource_loader shader_resource_loader_create() {
    resource_loader loader;
    loader.type = RESOURCE_TYPE_SHADER;
    loader.custom_type = 0;
    loader.load = shader_loader_load;
    loader.unload = shader_loader_unload;
    loader.type_path = "shaders";

    return loader;
}
