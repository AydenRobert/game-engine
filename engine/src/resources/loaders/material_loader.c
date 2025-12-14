#include "resources/loaders/material_loader.h"

#include "core/kmemory.h"
#include "core/kstring.h"
#include "core/logger.h"
#include "math/kmath.h"
#include "resources/loaders/loader_utils.h"
#include "resources/resource_types.h"
#include "systems/resource_system.h"

#include "platform/filesystem.h"

b8 material_loader_load(struct resource_loader *self, const char *name,
                        resource *out_resource) {
    if (!self || !name || !out_resource) {
        return false;
    }

    char *format_str = "%s/%s/%s%s";
    char full_file_path[512];
    string_format(full_file_path, format_str, resource_system_base_path(),
                  self->type_path, name, ".kmt");

    file_handle file;
    if (!filesystem_open(full_file_path, FILE_MODE_READ, false, &file)) {
        KERROR("material_loader_load - unable to open material file for "
               "reading: '%s'.",
               full_file_path);
    }

    // TODO: should use an allocator
    out_resource->full_path = string_duplicate(full_file_path);

    // TODO: should use an allocator
    material_config *resource_data =
        kallocate(sizeof(material_config), MEMORY_TAG_MATERIAL_INSTANCE);
    resource_data->shader_name = "Builtin.Material";
    resource_data->auto_release = true;
    resource_data->diffuse_colour = vec4_one();
    resource_data->diffuse_map_name[0] = 0;
    string_ncopy(resource_data->name, name, MATERIAL_NAME_MAX_LENGTH);

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
            string_ncopy(resource_data->name, trimmed_value,
                         MATERIAL_NAME_MAX_LENGTH);
        } else if (strings_equali(trimmed_variable_name, "diffuse_map_name")) {
            string_ncopy(resource_data->diffuse_map_name, trimmed_value,
                         TEXTURE_NAME_MAX_LENGTH);
        } else if (strings_equali(trimmed_variable_name, "diffuse_colour")) {
            if (!string_to_vec4(trimmed_value,
                                &resource_data->diffuse_colour)) {
                KWARN("Error parsing diffuse colour in file '%s'. Using "
                      "default of white instead.",
                      full_file_path);
            }
        } else if (strings_equali(trimmed_variable_name, "shader")) {
            resource_data->shader_name = string_duplicate(trimmed_value);
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

void material_loader_unload(struct resource_loader *self, resource *resource) {
    if (!resource_unload(self, resource, MEMORY_TAG_MATERIAL_INSTANCE)) {
        KWARN("material_loader_unload - called with nullptr for self or "
              "resource.");
    }
}

resource_loader material_resource_loader_create() {
    resource_loader loader;
    loader.type = RESOURCE_TYPE_MATERIAL;
    loader.custom_type = 0;
    loader.load = material_loader_load;
    loader.unload = material_loader_unload;
    loader.type_path = "materials";

    return loader;
}
