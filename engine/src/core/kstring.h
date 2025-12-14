#pragma once

#include "defines.h"
#include <stdio.h>

#include "math/math_types.h"

KAPI u64 string_length(const char *str);

KAPI char *string_duplicate(const char *str);

KAPI b8 strings_equal(const char *str0, const char *str1);

// Case-insensitive
KAPI b8 strings_equali(const char *str0, const char *str1);

KAPI i32 string_format(char *dest, const char *format, ...);

KAPI i32 string_format_v(char *dest, const char *format, va_list arg_ptr);

KAPI char *string_empty(char *str);

KAPI char *string_copy(char *dest, const char *source);

KAPI char *string_ncopy(char *dest, const char *source, i64 length);

KAPI char *string_trim(char *str);

KAPI void string_mid(char *dest, const char *source, i32 start, i32 length);

/**
 * @brief Returns the index of the first occurance of c; otherwise returns -1.
 */
KAPI i32 string_index_of_char(char *str, char c);

/**
 * @brief Attemps to parse a vector from the provided string.
 * Zeros out memory before attempting to parse.
 *
 * @param str The string to parse, should be space-delimited.
 * (ie. "1.0 2.0 3.0 4.0")
 * @param out_vec A pointer to the vector to write to.
 * @return True if parsed successfully; otherwise false.
 */
KAPI b8 string_to_vec4(char *str, vec4 *out_vec);

/**
 * @brief Attemps to parse a vector from the provided string.
 * Zeros out memory before attempting to parse.
 *
 * @param str The string to parse, should be space-delimited.
 * (ie. "1.0 2.0 3.0")
 * @param out_vec A pointer to the vector to write to.
 * @return True if parsed successfully; otherwise false.
 */
KAPI b8 string_to_vec3(char *str, vec3 *out_vec);

/**
 * @brief Attemps to parse a vector from the provided string.
 * Zeros out memory before attempting to parse.
 *
 * @param str The string to parse, should be space-delimited.
 * (ie. "1.0 2.0")
 * @param out_vec A pointer to the vector to write to.
 * @return True if parsed successfully; otherwise false.
 */
KAPI b8 string_to_vec2(char *str, vec2 *out_vec);

/**
 * @brief Attempts to parse a 32-bit floating point number from a string.
 *
 * @param str The string to parse. Should *NOT* be postfixed with 'f'.
 * @param out_float A pointer to the float to write to.
 * @return True if parsed successfully; otherwise false.
 */
KAPI b8 string_to_f32(char *str, f32 *out_float);

/**
 * @brief Attempts to parse a 64-bit floating point number from a string.
 *
 * @param str The string to parse.
 * @param out_float A pointer to the float to write to.
 * @return True if parsed successfully; otherwise false.
 */
KAPI b8 string_to_f64(char *str, f64 *out_float);

/**
 * @brief Attempts to parse a 8-bit signed integer from a string.
 *
 * @param str The string to parse.
 * @param out_float A pointer to the int to write to.
 * @return True if parsed successfully; otherwise false.
 */
KAPI b8 string_to_i8(char *str, i8 *out_int);

/**
 * @brief Attempts to parse a 16-bit signed integer from a string.
 *
 * @param str The string to parse.
 * @param out_float A pointer to the int to write to.
 * @return True if parsed successfully; otherwise false.
 */
KAPI b8 string_to_i16(char *str, i16 *out_int);

/**
 * @brief Attempts to parse a 32-bit signed integer from a string.
 *
 * @param str The string to parse.
 * @param out_float A pointer to the int to write to.
 * @return True if parsed successfully; otherwise false.
 */
KAPI b8 string_to_i32(char *str, i32 *out_int);

/**
 * @brief Attempts to parse a 64-bit signed integer from a string.
 *
 * @param str The string to parse.
 * @param out_float A pointer to the int to write to.
 * @return True if parsed successfully; otherwise false.
 */
KAPI b8 string_to_i64(char *str, i64 *out_int);

/**
 * @brief Attempts to parse a 8-bit unsigned integer from a string.
 *
 * @param str The string to parse.
 * @param out_float A pointer to the int to write to.
 * @return True if parsed successfully; otherwise false.
 */
KAPI b8 string_to_u8(char *str, u8 *out_int);

/**
 * @brief Attempts to parse a 16-bit unsigned integer from a string.
 *
 * @param str The string to parse.
 * @param out_float A pointer to the int to write to.
 * @return True if parsed successfully; otherwise false.
 */
KAPI b8 string_to_u16(char *str, u16 *out_int);

/**
 * @brief Attempts to parse a 32-bit unsigned integer from a string.
 *
 * @param str The string to parse.
 * @param out_float A pointer to the int to write to.
 * @return True if parsed successfully; otherwise false.
 */
KAPI b8 string_to_u32(char *str, u32 *out_int);

/**
 * @brief Attempts to parse a 64-bit unsigned integer from a string.
 *
 * @param str The string to parse.
 * @param out_float A pointer to the int to write to.
 * @return True if parsed successfully; otherwise false.
 */
KAPI b8 string_to_u64(char *str, u64 *out_int);

/**
 * @brief Attempts to parse a boolean from a string.
 *
 * @param str The string to parse. "true" or "1" is considered true;
 * otherwise is considered false.
 * @param out_bool A pointer to the bool to write to.
 * @return True if parsed successfully; otherwise false.
 */
KAPI b8 string_to_b8(char *str, b8 *out_bool);

KAPI u32 string_split(const char *str, char delimiter, char ***str_darray,
                      b8 trim_entries, b8 include_empty);
KAPI void string_cleanup_split_array(char **str_darray);
