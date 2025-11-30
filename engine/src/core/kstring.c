#include "core/kstring.h"

#include "core/kmemory.h"

// TODO: temporary
#include <ctype.h> // isspace
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef _MSC_VER
#include <strings.h>
#endif

u64 string_length(const char *str) { return strlen(str); }

char *string_duplicate(const char *str) {
    u64 length = string_length(str);
    char *copy = kallocate(length + 1, MEMORY_TAG_STRING);
    kcopy_memory(copy, str, length + 1);
    return copy;
}

b8 strings_equal(const char *str0, const char *str1) {
    return strcmp(str0, str1) == 0;
}

KAPI b8 strings_equali(const char *str0, const char *str1) {
#if defined(__GNUC__)
    return strcasecmp(str0, str1) == 0;
#elif defined(_MSC_VER)
    return _strcmpi(str0, str1) == 0;
#endif
}

i32 string_format(char *dest, const char *format, ...) {
    if (!dest) {
        return -1;
    }
    __builtin_va_list arg_ptr;
    va_start(arg_ptr, format);
    i32 written = string_format_v(dest, format, arg_ptr);
    va_end(arg_ptr);
    return written;
}

i32 string_format_v(char *dest, const char *format, va_list arg_ptr) {
    if (!dest)
        return -1;

    char buffer[32000];
    i32 written = vsnprintf(buffer, sizeof(buffer), format, arg_ptr);
    if (written < 0)
        return written;

    buffer[written] = '\0';
    kcopy_memory(dest, buffer, (u64)written + 1);
    return written;
}

KAPI char *string_empty(char *str) {
    if (str) {
        str[0] = 0;
    }

    return str;
}

char *string_copy(char *dest, const char *source) {
    return strcpy(dest, source);
}

char *string_ncopy(char *dest, const char *source, i64 length) {
    return strncpy(dest, source, length);
}

char *string_trim(char *str) {
    while (isspace((unsigned char)*str)) {
        str++;
    }
    if (*str) {
        char *p = str;
        while (*p) {
            p++;
        }
        while (isspace((unsigned char)*(--p)))
            ;
        p[1] = '\0';
    }

    return str;
}

void string_mid(char *dest, const char *source, i32 start, i32 length) {
    u64 src_length = string_length(source);
    if (length == 0 || start >= src_length) {
        dest[0] = 0;
        return;
    }
    u64 j = 0;
    if (length > 0) {
        // NOTE: implicit type cast from i32 to u64
        for (u64 i = start; j < length && source[i]; i++, j++) {
            dest[j] = source[i];
        }
    } else {
        // If negative is passed, proceed to end of string
        for (u64 i = start; source[i]; i++, j++) {
            dest[j] = source[i];
        }
    }
    dest[j] = 0;
}

i32 string_index_of_char(char *str, char c) {
    if (!str) {
        return -1;
    }

    u32 length = string_length(str);
    if (length <= 0) {
        return -1;
    }

    for (u32 i = 0; i < length; i++) {
        if (str[i] == c) {
            return i;
        }
    }

    return -1;
}

b8 string_to_vec4(char *str, vec4 *out_vec) {
    if (!str) {
        return false;
    }

    kzero_memory(out_vec, sizeof(vec4));
    i32 result = sscanf(str, "%f %f %f %f", &out_vec->x, &out_vec->y,
                        &out_vec->z, &out_vec->w);
    return result != -1;
}

b8 string_to_vec3(char *str, vec3 *out_vec) {
    if (!str) {
        return false;
    }

    kzero_memory(out_vec, sizeof(vec3));
    i32 result = sscanf(str, "%f %f %f", &out_vec->x, &out_vec->y, &out_vec->z);
    return result != -1;
}

b8 string_to_vec2(char *str, vec2 *out_vec) {
    if (!str) {
        return false;
    }

    kzero_memory(out_vec, sizeof(vec2));
    i32 result = sscanf(str, "%f %f", &out_vec->x, &out_vec->y);
    return result != -1;
}

b8 string_to_f32(char *str, f32 *out_float) {
    if (!str) {
        return false;
    }

    *out_float = 0;
    i32 result = sscanf(str, "%f", out_float);
    return result != -1;
}

b8 string_to_f64(char *str, f64 *out_float) {
    if (!str) {
        return false;
    }

    *out_float = 0;
    i32 result = sscanf(str, "%lf", out_float);
    return result != -1;
}

b8 string_to_i8(char *str, i8 *out_int) {
    if (!str) {
        return false;
    }

    *out_int = 0;
    i32 result = sscanf(str, "%hhi", out_int);
    return result != -1;
}

b8 string_to_i16(char *str, i16 *out_int) {
    if (!str) {
        return false;
    }

    *out_int = 0;
    i32 result = sscanf(str, "%hi", out_int);
    return result != -1;
}

b8 string_to_i32(char *str, i32 *out_int) {
    if (!str) {
        return false;
    }

    *out_int = 0;
    i32 result = sscanf(str, "%i", out_int);
    return result != -1;
}

b8 string_to_i64(char *str, i64 *out_int) {
    if (!str) {
        return false;
    }

    *out_int = 0;
    i32 result = sscanf(str, "%lli", out_int);
    return result != -1;
}

b8 string_to_u8(char *str, u8 *out_int) {
    if (!str) {
        return false;
    }

    *out_int = 0;
    i32 result = sscanf(str, "%hhu", out_int);
    return result != -1;
}

b8 string_to_u16(char *str, u16 *out_int) {
    if (!str) {
        return false;
    }

    *out_int = 0;
    i32 result = sscanf(str, "%hu", out_int);
    return result != -1;
}

b8 string_to_u32(char *str, u32 *out_int) {
    if (!str) {
        return false;
    }

    *out_int = 0;
    i32 result = sscanf(str, "%u", out_int);
    return result != -1;
}

b8 string_to_u64(char *str, u64 *out_int) {
    if (!str) {
        return false;
    }

    *out_int = 0;
    i32 result = sscanf(str, "%llu", out_int);
    return result != -1;
}

b8 string_to_b8(char *str, b8 *out_bool) {
    if (!str) {
        return false;
    }

    *out_bool = strings_equal(str, "1") || strings_equali(str, "true");
    return true;
}
