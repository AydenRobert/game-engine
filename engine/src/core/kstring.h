#pragma once

#include "defines.h"
#include <stdio.h>

KAPI u64 string_length(const char *str);

KAPI char *string_duplicate(const char *str);

KAPI b8 strings_equal(const char *str0, const char *str1);

i32 string_format(char* dest, const char* format, ...);

i32 string_format_v(char *dest, const char *format, va_list arg_ptr);
