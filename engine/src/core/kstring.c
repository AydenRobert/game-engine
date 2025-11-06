#include "core/kstring.h"

#include "core/kmemory.h"

// TODO: temporary
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

KAPI void string_format(char *out_string, char *format_string, ...) {
    va_list arg_ptr;
    va_start(arg_ptr, format_string);
    vsnprintf(out_string, strlen(format_string), format_string, arg_ptr);
    va_end(arg_ptr);
}
