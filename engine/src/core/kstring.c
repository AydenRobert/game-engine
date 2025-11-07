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
    if (!dest) return -1;

    char buffer[32000];
    i32 written = vsnprintf(buffer, sizeof(buffer), format, arg_ptr);
    if (written < 0) return written;

    buffer[written] = '\0';
    kcopy_memory(dest, buffer, (u64)written + 1);
    return written;
}
